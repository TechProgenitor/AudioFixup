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

void CodecInfo::populateCodecInfo(IORegistryEntry *currentGPU, IORegistryEntry *framebuffer, const char *propertyName) {
    
    OSObject *codecInfo = currentGPU->getProperty(propertyName);
    uint32_t codecInfoValue = 0;
    
    // Checks if the input is data
    if (OSData *data = OSDynamicCast(OSData, codecInfo)) {
        
        if (data->getLength() == 4) {
            memcpy(&codecInfoValue, data->getBytesNoCopy(), 4);
            
            if (framebuffer->setProperty("audio-codec-info", &codecInfoValue, 4)) {
                DBGLOG("codecinfo", "Successfully replaced %s", propertyName);
            } else {
                SYSLOG("codecinfo", "Failed to replace %s", propertyName);
            }
            
            return;
        }
    }
        
    // Checks if the input is a number
    else if (OSNumber *number = OSDynamicCast(OSNumber, codecInfo)) {
        
        if (number->unsigned64BitValue() <= UINT32_MAX) {
            codecInfoValue = number->unsigned32BitValue();
            
            if (framebuffer->setProperty("audio-codec-info", &codecInfoValue, 4)) {
                DBGLOG("codecinfo", "Successfully replaced %s", propertyName);
            } else {
                SYSLOG("codecinfo", "Failed to replace %s", propertyName);
            }
            
            return;
        }
    }
}

void CodecInfo::populateCodecInfoBytes(IORegistryEntry *currentGPU, IORegistryEntry *framebuffer, const char *propertyName, uint byteOffset) {
    
    // Search for the requested property
    OSData *propertyInfo = OSDynamicCast(OSData, currentGPU->getProperty(propertyName));

    if (!propertyInfo || propertyInfo->getLength() == 0)
        return;
    
    // Retrieve the requested property value
    uint8_t propertyValue = static_cast<const uint8_t *>(propertyInfo->getBytesNoCopy())[0];
    
    // Retrieve the current audio-codec-info value
    OSData *currentCodecInfo = OSDynamicCast(OSData, framebuffer->getProperty("audio-codec-info"));
    uint8_t modifyCodecInfo[4] = {};
    
    if (currentCodecInfo && currentCodecInfo->getLength() == 4) {
        memcpy(modifyCodecInfo, currentCodecInfo->getBytesNoCopy(), sizeof(modifyCodecInfo));
    }
    
    // Check that the byteOffset is between 0-3
    if (byteOffset >= sizeof(modifyCodecInfo))
        return;
    
    // Modify the audio-codec-info value
    modifyCodecInfo[byteOffset] = propertyValue;

    // Replace the audio-codec-info value
    OSData *newCodecInfo = OSData::withBytes(modifyCodecInfo, sizeof(modifyCodecInfo));
    
    if (!newCodecInfo) {
        SYSLOG("codecinfo", "Failed to create new audio-codec-info OSData for %s", propertyName);
        return;
    }

    if (framebuffer->setProperty("audio-codec-info", newCodecInfo)) {
        DBGLOG("codecinfo", "Successfully replaced %s", propertyName);
    } else {
        SYSLOG("codecinfo", "Failed to replace %s", propertyName);
    }
    
    newCodecInfo->release();
}

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
        
        // GPU lookup didn't screw up, and we want to patch the audio-codec-info value
        
        char portAdr[32];
        snprintf(portAdr, sizeof(portAdr), "audio-port%u-adr", portNumber);
        callbackCodecInfo->populateCodecInfoBytes(currentGPU, framebuffer, portAdr, 0);
        
        char portAfg[32];
        snprintf(portAfg, sizeof(portAfg), "audio-port%u-afg", portNumber);
        callbackCodecInfo->populateCodecInfoBytes(currentGPU, framebuffer, portAfg, 1);
        
        char portPin[32];
        snprintf(portPin, sizeof(portPin), "audio-port%u-pin", portNumber);
        callbackCodecInfo->populateCodecInfoBytes(currentGPU, framebuffer, portPin, 2);
        
        char portEnd[32];
        snprintf(portEnd, sizeof(portEnd), "audio-port%u-end", portNumber);
        callbackCodecInfo->populateCodecInfoBytes(currentGPU, framebuffer, portEnd, 3);
        
        char portInfo[32];
        snprintf(portInfo, sizeof(portInfo), "audio-port%u-info", portNumber);
        callbackCodecInfo->populateCodecInfo(currentGPU, framebuffer, portInfo);
        
        // Let the original function run and return its value to the third argument
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
