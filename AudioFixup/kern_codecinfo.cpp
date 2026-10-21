//
//  kern_codecinfo.cpp
//  AudioFixup
//
//  Copyright © 2026 TechProgenitor. All rights reserved.
//

#include <Headers/kern_api.hpp>
#include <Headers/kern_util.hpp>
#include "kern_codecinfo.hpp"
#include <IOKit/IOService.h>

static const char *hdaId {"com.apple.driver.AppleHDAController"};
static const char *hdaPath[] {"/System/Library/Extensions/AppleHDA.kext/Contents/PlugIns/AppleHDAController.kext/Contents/MacOS/AppleHDAController"};

static const char *gfxhdaId {"com.apple.driver.AppleGFXHDA"};
static const char *gfxhdaPath[] {"/System/Library/Extensions/AppleGFXHDA.kext/Contents/MacOS/AppleGFXHDA"};

static KernelPatcher::KextInfo applehda { hdaId, hdaPath, 1, {true, true}, {}, KernelPatcher::KextInfo::Unloaded };
static KernelPatcher::KextInfo applegfxhda { gfxhdaId, gfxhdaPath, 1, {true, true}, {}, KernelPatcher::KextInfo::Unloaded };

CodecInfo *CodecInfo::callbackCodecInfo;

bool CodecInfo::init() {
    callbackCodecInfo = this;
    LiluAPI::Error error;
    
    error = lilu.onKextLoad(&applehda, 1,
        [](void *user, KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size) {
            static_cast<CodecInfo *>(user)->processHDA(patcher, index, address, size);
        }, this);

    if (error != LiluAPI::Error::NoError) {
        SYSLOG("codecinfo", "failed to register onPatcherLoad method %d", error);
        return false;
    }
    
    error = lilu.onKextLoad(&applegfxhda, 1,
        [](void *user, KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size) {
            static_cast<CodecInfo *>(user)->processGFXHDA(patcher, index, address, size);
        }, this);

    if (error != LiluAPI::Error::NoError) {
        SYSLOG("codecinfo", "failed to register onPatcherLoad method %d", error);
        return false;
    }

    return true;
}

void CodecInfo::deinit() {}

IOReturn CodecInfo::wrapGetOSDataOrOSNumberPropertyFromEntry(void *controller, IORegistryEntry *framebuffer, const char *property, uint *value, IOReturn (*wrapFunction)(void*, IORegistryEntry*, const char*, uint*), mach_vm_address_t orgFunction) {
    
    // Verify that the arguments are not null
    if ((framebuffer == nullptr) || (property == nullptr) || (value == nullptr))
        return FunctionCast(wrapFunction, orgFunction)(controller, framebuffer, property, value);
    
    // Check if AppleGFXHDA wants the codec information
    if (strcmp(property, "audio-codec-info") == 0) {
        
        // Get the current port-number value
        uint portNumber = 0;
        IOReturn result = FunctionCast(wrapFunction, orgFunction)(controller, framebuffer, "port-number", &portNumber);
        if (result != kIOReturnSuccess)
            return FunctionCast(wrapFunction, orgFunction)(controller, framebuffer, property, value);
        
        // Convert the port-number value into a device property to look for
        char propertyName[25];
        snprintf(propertyName, sizeof(propertyName), "audio-port%u-codec-info", portNumber);
        
        // See which GPU has our custom device property
        IORegistryEntry *currentGPU = framebuffer;
        
        while (currentGPU != nullptr && currentGPU->getProperty("vendor-id") == nullptr) {
            currentGPU = currentGPU->getParentEntry(gIOServicePlane);
        }
        
        // If we screw up finding the GPU let the original code run and call it a day
        if (currentGPU == nullptr) {
            return FunctionCast(wrapFunction, orgFunction)(controller, framebuffer, property, value);
        }
        
        // Check Patcher Enable Flag
        OSData *enabled = OSDynamicCast(OSData, currentGPU->getProperty("audio-patch-enable"));
        if (!enabled || enabled->getLength() == 0 || static_cast<const uint8_t *>(enabled->getBytesNoCopy())[0] == 0)
            return FunctionCast(wrapFunction, orgFunction)(controller, framebuffer, property, value);
        
        // Get our custom device property value and replace the current codec information
        OSObject *codecInfo = currentGPU->getProperty(propertyName);
        uint32_t codecValue = 0;
        
        // Checks if the input is data
        if (OSData *data = OSDynamicCast(OSData, codecInfo)) {
            if (data->getLength() == 4) {
                memcpy(&codecValue, data->getBytesNoCopy(), 4);
                framebuffer->setProperty("audio-codec-info", &codecValue, 4);
            }
            
        // Checks if the input is a number
        } else if (OSNumber *number = OSDynamicCast(OSNumber, codecInfo)) {
            if (number->unsigned64BitValue() <= UINT32_MAX) {
                codecValue = number->unsigned32BitValue();

                framebuffer->setProperty("audio-codec-info", &codecValue, 4);
            }
        }
        
        // Let the original function run and cast the value to the third argument
        return FunctionCast(wrapFunction, orgFunction)(controller, framebuffer, property, value);
    }
    
    // Let the original function run if audio codec information isn't wanted
    return FunctionCast(wrapFunction, orgFunction)(controller, framebuffer, property, value);
}

IOReturn CodecInfo::wrapAppleHDA(void *controller, IORegistryEntry *framebuffer, const char *property, uint *value) {
    return wrapGetOSDataOrOSNumberPropertyFromEntry(controller, framebuffer, property, value, wrapAppleHDA, callbackCodecInfo->orgAppleHDAGetOSDataOrOSNumberPropertyFromEntry);
}

IOReturn CodecInfo::wrapAppleGFXHDA(void *controller, IORegistryEntry *framebuffer, const char *property, uint *value) {
    return wrapGetOSDataOrOSNumberPropertyFromEntry(controller, framebuffer, property, value, wrapAppleGFXHDA, callbackCodecInfo->orgAppleGFXHDAGetOSDataOrOSNumberPropertyFromEntry);
}

void CodecInfo::processHDA(KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size) {
    if (progressHDAState != ProcessingHDAState::EverythingDone) {
        if (applehda.loadIndex == index) {
            DBGLOG("codecinfo", "found %s", applehda.id);

            KernelPatcher::RouteRequest request("__ZN18AppleHDAController36getOSDataOrOSNumberPropertyFromEntryEP15IORegistryEntryPKcPj", wrapAppleHDA, orgAppleHDAGetOSDataOrOSNumberPropertyFromEntry);
            patcher.routeMultiple(index, &request, 1, address, size);

            if (patcher.getError() == KernelPatcher::Error::NoError) {
                DBGLOG("codecinfo", "hda property getter hook installed");
                progressHDAState |= ProcessingHDAState::AudioPatched;
            } else {
                SYSLOG("codecinfo", "failed to hook hda property getter");
                patcher.clearError();
            }
        }
    }
}


void CodecInfo::processGFXHDA(KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size) {
    if (progressGFXHDAState != ProcessingGFXHDAState::EverythingDone) {
        if (applegfxhda.loadIndex == index) {
            DBGLOG("codecinfo", "found %s", applegfxhda.id);

            KernelPatcher::RouteRequest request("__ZN21AppleGFXHDAController36getOSDataOrOSNumberPropertyFromEntryEP15IORegistryEntryPKcPj", wrapAppleGFXHDA, orgAppleGFXHDAGetOSDataOrOSNumberPropertyFromEntry);
            patcher.routeMultiple(index, &request, 1, address, size);

            if (patcher.getError() == KernelPatcher::Error::NoError) {
                DBGLOG("codecinfo", "gfxhda property getter hook installed");
                progressGFXHDAState |= ProcessingGFXHDAState::AudioPatched;
            } else {
                SYSLOG("codecinfo", "failed to hook gfxhda property getter");
                patcher.clearError();
            }
        }
    }
}
