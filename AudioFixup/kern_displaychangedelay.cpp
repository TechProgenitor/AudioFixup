//
//  kern_displaychangedelay.cpp
//  AudioFixup
//
//  Copyright © 2026 TechProgenitor. All rights reserved.
//

#include <Headers/kern_api.hpp>
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
    
    uint32_t delay = 0;
    
    if (lilu_get_boot_args("displaychangedelay", &delay, sizeof(delay))){
        DBGLOG("displaychangedelay", "found displaychangedelay override %u", delay);
    }
    
    if (delay > 1000000)
        delay = 1000000;
    
    if (delay > 0)
        IODelay(delay);
    
    // Let Apple's implementation run.
    IOReturn ret = FunctionCast(wrapDisplayChangeHandler, callbackDisplayChangeDelay->orgDisplayChangeHandler)(that, param_1, param_2, param_3, param_4);
    
    return ret;
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
