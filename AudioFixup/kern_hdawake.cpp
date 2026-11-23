//
//  kern_hdawake.cpp
//  AudioFixup
//
//  Copyright © 2026 TechProgenitor. All rights reserved.
//

#include <Headers/kern_api.hpp>
#include <Headers/kern_devinfo.hpp>
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

int HDAWAKE::wrapSetPowerState(AppleHDANode *that, uint powerState, bool force) {
    
    bool afgRecoveryEnabled = false;
    uint8_t selectedNodeForRecovery = 1;
    uint8_t currentNodeBeingProcessed = 0;
    bool retrievedCurrentNodeID = callbackHDAWAKE->orgNodeID != nullptr;
    if (retrievedCurrentNodeID)
        currentNodeBeingProcessed = callbackHDAWAKE->orgNodeID(that);
    
    uint32_t selectedCodecForRecovery = 0;
    uint32_t d0EntryDelay = 0;
    
    uint32_t currentCodecBeingProcessed = 0;
    bool retrievedCurrentVendorProductID = callbackHDAWAKE->orgVendorProductID != nullptr;
    if (retrievedCurrentVendorProductID)
        currentCodecBeingProcessed = static_cast<uint32_t>(callbackHDAWAKE->orgVendorProductID(that) & 0xffffffff);
    
    auto deviceInfo = DeviceInfo::create();
    if (deviceInfo) {
        
        // Retrieve HDEF device properties for AFG configuration.
        auto hdef = deviceInfo->audioBuiltinAnalog;
        if (hdef) {
            
            OSData *afgRecoveryEnabledValue = OSDynamicCast(OSData, hdef->getProperty("afg-recovery"));
            if (afgRecoveryEnabledValue && afgRecoveryEnabledValue->getLength() > 0 &&
                static_cast<const uint8_t *>(afgRecoveryEnabledValue->getBytesNoCopy())[0] != 0) {
                afgRecoveryEnabled = true;
            }
            
            if (WIOKit::getOSDataValue(hdef, "afg-node", selectedNodeForRecovery)) {
                DBGLOG("afg", "selected node: %u", selectedNodeForRecovery);
            }
            
            if (WIOKit::getOSDataValue(hdef, "afg-codec", selectedCodecForRecovery)) {
                DBGLOG("afg", "selected codec: %08X", selectedCodecForRecovery);
            }
            
            if (WIOKit::getOSDataValue(hdef, "afg-delay", d0EntryDelay)) {
                DBGLOG("afgondelay", "found afgondelay override %u", d0EntryDelay);
            }
            
        } else {
            SYSLOG("afg", "analog controller not found");
        }
        
        DeviceInfo::deleter(deviceInfo);
    }
    
    // Delay entry into D0 for the selected AFG node.
    if (powerState == 0 &&
        retrievedCurrentNodeID &&
        currentNodeBeingProcessed == selectedNodeForRecovery) {
        
        if (selectedCodecForRecovery == 0 ||
            (retrievedCurrentVendorProductID &&
            currentCodecBeingProcessed == selectedCodecForRecovery)) {
            
            // Delay cannot be longer than one second.
            if (d0EntryDelay > 1000000)
                d0EntryDelay = 1000000;
            
            if (d0EntryDelay > 0)
                IODelay(d0EntryDelay);
        }
    }
            
    int ret = FunctionCast(wrapSetPowerState, callbackHDAWAKE->orgSetPowerState)(that, powerState, force);
    
    // Schedule deferred AFG recovery when the selected node exits D0.
    if (powerState > 0 &&
        retrievedCurrentNodeID &&
        currentNodeBeingProcessed == selectedNodeForRecovery) {
                
        if (afgRecoveryEnabled && (selectedCodecForRecovery == 0 ||
            (retrievedCurrentVendorProductID &&
            currentCodecBeingProcessed == selectedCodecForRecovery))) {
            
            // Queue deferred AFG D0 recovery.
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
            
            KernelPatcher::RouteRequest request("__ZN12AppleHDANode13setPowerStateEjb", wrapSetPowerState, orgSetPowerState);
            patcher.routeMultiple(index, &request, 1, address, size);
            
            if (patcher.getError() == KernelPatcher::Error::NoError) {
                DBGLOG("hdawake", "setPowerState hook installed");
                progressState |= ProcessingState::HDAPatched;
            } else {
                SYSLOG("hdawake", "failed to hook setPowerState");
                patcher.clearError();
                return;
            }
            
            orgNodeID = reinterpret_cast<t_NodeID>(patcher.solveSymbol(index, "__ZN12AppleHDANode6nodeIDEv"));
            orgVendorProductID = reinterpret_cast<t_VendorProductID>(patcher.solveSymbol(index, "__ZN12AppleHDANode15vendorProductIDEv"));
        }
    }
}
