//
//  kern_displaychangedelay.cpp
//  AudioFixup
//
//  Copyright © 2026 TechProgenitor. All rights reserved.
//

#include <Headers/kern_api.hpp>
#include <Headers/kern_devinfo.hpp>
#include <Headers/kern_util.hpp>
#include "kern_displaychangedelay.hpp"
#include <IOKit/IOService.h>

static const char *kextId {
    "com.apple.iokit.IOAcceleratorFamily2"
};

static const char *kextPath[] {
    "/System/Library/Extensions/IOAcceleratorFamily2.kext/Contents/MacOS/IOAcceleratorFamily2"
};

static KernelPatcher::KextInfo kext { kextId, kextPath, 1, {true, true}, {}, KernelPatcher::KextInfo::Unloaded };

DISPLAYCHANGEDELAY *DISPLAYCHANGEDELAY::callbackDisplayChangeDelay;

bool DISPLAYCHANGEDELAY::init() {
    callbackDisplayChangeDelay = this;
    
    LiluAPI::Error error = lilu.onKextLoad(&kext, 1,
        [](void *user, KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size) {
            static_cast<DISPLAYCHANGEDELAY *>(user)->processKext(patcher, index, address, size);
        }, this);

    if (error != LiluAPI::Error::NoError) {
        SYSLOG("displaychangedelay", "failed to register onPatcherLoad method %d", error);
        return false;
    }

    return true;
}

void DISPLAYCHANGEDELAY::deinit() {}

IOReturn DISPLAYCHANGEDELAY::wrapDisplayChangeHandler(IOAccelDisplayPipe *that, void *param_1, IOFramebuffer *param_2, IOIndex param_3, void *param_4) {
    
    // Retrieve the current port-number value
    uint32_t portNumberValue = 0;
    OSNumber *portNumberProperty = nullptr;
    
    if (param_2 != nullptr)
        portNumberProperty = OSDynamicCast(OSNumber, param_2->getProperty("port-number"));
    else
        return FunctionCast(wrapDisplayChangeHandler, callbackDisplayChangeDelay->orgDisplayChangeHandler)(that, param_1, param_2, param_3, param_4);
    
    if (portNumberProperty != nullptr) {
        portNumberValue = portNumberProperty->unsigned32BitValue();
        DBGLOG("displaychangedelay", "current port-number: %u", portNumberValue);
    } else {
        SYSLOG("displaychangedelay", "failed to retrieve port-number");
        return FunctionCast(wrapDisplayChangeHandler, callbackDisplayChangeDelay->orgDisplayChangeHandler)(that, param_1, param_2, param_3, param_4);
    }
    
    // See which GPU has our custom device property
    IORegistryEntry *currentGPU = param_2;
    
    while (currentGPU != nullptr && currentGPU->getProperty("vendor-id") == nullptr) {
        currentGPU = currentGPU->getParentEntry(gIOServicePlane);
    }
    
    // If we screw up finding the GPU let the original code run and call it a day
    if (currentGPU == nullptr) {
        return FunctionCast(wrapDisplayChangeHandler, callbackDisplayChangeDelay->orgDisplayChangeHandler)(that, param_1, param_2, param_3, param_4);
    }
    
    // Check Patcher Enable Flag
    OSData *enabled = OSDynamicCast(OSData, currentGPU->getProperty("audio-patch-enable"));
    if (!enabled || enabled->getLength() == 0 || static_cast<const uint8_t *>(enabled->getBytesNoCopy())[0] == 0)
        return FunctionCast(wrapDisplayChangeHandler, callbackDisplayChangeDelay->orgDisplayChangeHandler)(that, param_1, param_2, param_3, param_4);
    
    // GPU lookup didn't screw up, and we want to delay the display change event.
    
    char portDelay[32];
    snprintf(portDelay, sizeof(portDelay), "audio-port%u-delay", portNumberValue);
    
    uint32_t delay = 0;
    
    if (WIOKit::getOSDataValue(currentGPU, portDelay, delay)) {
        DBGLOG("displaychangedelay", "selected delay: %u", delay);
    }
    
    if (delay > 1000000)
        delay = 1000000;
    
    if (delay > 0)
        IODelay(delay);
    
    return FunctionCast(wrapDisplayChangeHandler, callbackDisplayChangeDelay->orgDisplayChangeHandler)(that, param_1, param_2, param_3, param_4);
}

void DISPLAYCHANGEDELAY::processKext(KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size) {
    if (progressState != ProcessingState::EverythingDone) {
        if (kext.loadIndex == index) {
            DBGLOG("displaychangedelay", "found %s", kext.id);

            KernelPatcher::RouteRequest request("__ZN18IOAccelDisplayPipe22display_change_handlerEPvP13IOFramebufferiS0_", wrapDisplayChangeHandler, orgDisplayChangeHandler);
            patcher.routeMultiple(index, &request, 1, address, size);

            if (patcher.getError() == KernelPatcher::Error::NoError) {
                DBGLOG("displaychangedelay", "display_change_handler hook installed");
                progressState |= ProcessingState::DisplayChangeDelayPatched;
            } else {
                SYSLOG("displaychangedelay", "failed to hook display_change_handler");
                patcher.clearError();
            }
        }
    }
}
