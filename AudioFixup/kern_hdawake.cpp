//
//  kern_hdawake.cpp
//  AudioFixup
//
//  Copyright © 2026 TechProgenitor. All rights reserved.
//

#include <Headers/kern_api.hpp>
#include <Headers/kern_util.hpp>
#include "kern_hdawake.hpp"
#include <IOKit/IOService.h>

static const char *kextId {
    "com.apple.driver.AppleHDA"
};

static const char *kextPath[] {
    "/System/Library/Extensions/AppleHDA.kext/Contents/MacOS/AppleHDA"
};

static KernelPatcher::KextInfo kext { kextId, kextPath, 1, {true, true}, {}, KernelPatcher::KextInfo::Unloaded };

HDAWAKE *HDAWAKE::callbackHDAWAKE;

// Thread call used for deferred AFG power recovery.
static thread_call_t afgRecoveryCall = nullptr;

bool HDAWAKE::init() {
    callbackHDAWAKE = this;
    
    LiluAPI::Error error = lilu.onKextLoad(&kext, 1,
        [](void *user, KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size) {
            static_cast<HDAWAKE *>(user)->processKext(patcher, index, address, size);
        }, this);

    if (error != LiluAPI::Error::NoError) {
        SYSLOG("hdawake", "failed to register onPatcherLoad method %d", error);
        return false;
    }
    
    afgRecoveryCall = thread_call_allocate(recoverAFG, nullptr);

    return true;
}

void HDAWAKE::deinit() {
    
    // Disable pending deferred D0 threads
    if (afgRecoveryCall) {
        while (!thread_call_free(afgRecoveryCall))
            thread_call_cancel(afgRecoveryCall);

        afgRecoveryCall = nullptr;
    }
    
}

uint64_t HDAWAKE::wrapVendorProductID(AppleHDANode *that) {
    return FunctionCast(wrapVendorProductID, callbackHDAWAKE->orgVendorProductID)(that);
}

int HDAWAKE::wrapNodeID(AppleHDANode *that) {
    return FunctionCast(wrapNodeID, callbackHDAWAKE->orgNodeID)(that);
}

int HDAWAKE::wrapSetPowerState(AppleHDANode *that, uint powerState, bool force) {
    
    if ((powerState == 0) && wrapNodeID(that) == 1) {
        
        uint64_t selectedCodec = 0;
        
        if (lilu_get_boot_args("afgcodec", &selectedCodec, sizeof(selectedCodec))){
            DBGLOG("afgcodec", "found afgcodec override %llu", selectedCodec);
        }
        
        if (selectedCodec == 0 || static_cast<uint32_t>(selectedCodec & 0xffffffff) == static_cast<uint32_t>(wrapVendorProductID(that) & 0xffffffff)) {
        
        uint32_t delay = 0;
        
        if (lilu_get_boot_args("afgondelay", &delay, sizeof(delay))){
            DBGLOG("afgondelay", "found afgondelay override %u", delay);
        }
            
            if (delay > 1000000)
                delay = 1000000;
            
            if (delay > 0)
                IODelay(delay);
        }
    }
    
    int ret = FunctionCast(wrapSetPowerState, callbackHDAWAKE->orgSetPowerState)(that, powerState, force);
    
    if (powerState > 0 && wrapNodeID(that) == 1) {
        
        uint32_t recovery = 0;
        
        if (lilu_get_boot_args("afgrecovery", &recovery, sizeof(recovery))){
            DBGLOG("afgrecovery", "found afgrecovery override %u", recovery);
        }
        
        if (recovery == 0)
            return ret;
        
        uint64_t selectedCodec = 0;
        
        if (lilu_get_boot_args("afgcodec", &selectedCodec, sizeof(selectedCodec))){
            DBGLOG("afgcodec", "found afgcodec override %llu", selectedCodec);
        }
        
        if (selectedCodec == 0 || static_cast<uint32_t>(selectedCodec & 0xffffffff) == static_cast<uint32_t>(wrapVendorProductID(that) & 0xffffffff)) {
            
            if (afgRecoveryCall != nullptr) {
                thread_call_enter1(afgRecoveryCall, reinterpret_cast<thread_call_param_t>(that));
            }
        }
    }
    
    return ret;
}

void HDAWAKE::recoverAFG(thread_call_param_t param0, thread_call_param_t param1) {

    AppleHDANode *that = reinterpret_cast<AppleHDANode *>(param1);
    
    if (that == nullptr)
        return;
    
    callbackHDAWAKE->wrapSetPowerState(that, 0, true);
}

void HDAWAKE::processKext(KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size) {
    if (progressState != ProcessingState::EverythingDone) {
        if (kext.loadIndex == index) {
            DBGLOG("hdawake", "found %s", kext.id);
            
            KernelPatcher::RouteRequest requests[] {
                KernelPatcher::RouteRequest("__ZN12AppleHDANode13setPowerStateEjb", wrapSetPowerState, orgSetPowerState),
                KernelPatcher::RouteRequest("__ZN12AppleHDANode6nodeIDEv", wrapNodeID, orgNodeID),
                KernelPatcher::RouteRequest("__ZN12AppleHDANode15vendorProductIDEv", wrapVendorProductID, orgVendorProductID),
            };
            patcher.routeMultiple(index, requests, address, size);

            if (patcher.getError() == KernelPatcher::Error::NoError) {
                DBGLOG("hdawake", "setPowerState hook installed");
                progressState |= ProcessingState::HDAPatched;
            } else {
                SYSLOG("hdawake", "failed to hook setPowerState");
                patcher.clearError();
            }
        }
    }
}
