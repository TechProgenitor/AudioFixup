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

int HDAWAKE::performFormatChange(AppleHDAEngineOutput *that, IOAudioStream *stream, const IOAudioStreamFormat *format, const IOAudioSampleRate *samplerate) {
    int ret = FunctionCast(performFormatChange, callbackHDAWAKE->orgPerformFormatChange)(that, stream, format, samplerate);
    if (callbackHDAWAKE->AFGNodePtr && callbackHDAWAKE->afgForceEnabled) {
        auto deviceInfo = DeviceInfo::create();
        if (deviceInfo) {
            auto hdef = deviceInfo->audioBuiltinAnalog;
            if (hdef) {
                uint32_t AFGLowPowerState = 0;
                if (WIOKit::getOSDataValue(hdef, "AFGLowPowerState", AFGLowPowerState)) {
                    DBGLOG("afg", "AFG low-power state: %u", AFGLowPowerState);
                    wrapSetPowerState(callbackHDAWAKE->AFGNodePtr, AFGLowPowerState, true);
                    if (!callbackHDAWAKE->afgRecoveryEnabled) {
                        wrapSetPowerState(callbackHDAWAKE->AFGNodePtr, 0, true);
                    }
                } else {
                    SYSLOG("afg", "failed to retrieve AFGLowPowerState");
                }
            }
            DeviceInfo::deleter(deviceInfo);
        }
    }
    return ret;
}

int HDAWAKE::wrapSetPowerState(AppleHDANode *that, uint powerState, bool force) {
    
    // Retrieve current Node ID being processed
    uint8_t currentNodeBeingProcessed = 0;
    bool retrievedCurrentNodeID = callbackHDAWAKE->orgNodeID != nullptr;
    if (retrievedCurrentNodeID)
        currentNodeBeingProcessed = callbackHDAWAKE->orgNodeID(that);
    
    // Check that current codec only has analog I/O.
    bool isAnalogCodec = false;
    bool retrievedCurrentVendorProductID = callbackHDAWAKE->orgVendorProductID != nullptr;
    if (retrievedCurrentVendorProductID) {
        uint16_t currentVendorID = callbackHDAWAKE->orgVendorProductID(that) >> 16;
        isAnalogCodec = currentVendorID != 0x8086 &&  // Intel
                        currentVendorID != 0x1002 &&  // AMD
                        currentVendorID != 0x10DE;    // NVIDIA
    }
    
    // Capture AppleHDANode pointer for afg-force device property.
    if (!callbackHDAWAKE->AFGNodePtr && retrievedCurrentNodeID &&
        currentNodeBeingProcessed == callbackHDAWAKE->selectedNodeForRecovery &&
        retrievedCurrentVendorProductID && isAnalogCodec)
        callbackHDAWAKE->AFGNodePtr = that;
    
    // Delay entry into D0 for the selected AFG node.
    if (powerState == 0 && retrievedCurrentNodeID &&
        currentNodeBeingProcessed == callbackHDAWAKE->selectedNodeForRecovery &&
        isAnalogCodec && callbackHDAWAKE->d0EntryDelay > 0) {
            IODelay(callbackHDAWAKE->d0EntryDelay);
    }
            
    int ret = FunctionCast(wrapSetPowerState, callbackHDAWAKE->orgSetPowerState)(that, powerState, force);
    
    // Schedule deferred AFG recovery when the selected node exits D0.
    if (powerState > 0 && retrievedCurrentNodeID &&
        currentNodeBeingProcessed == callbackHDAWAKE->selectedNodeForRecovery &&
        isAnalogCodec && callbackHDAWAKE->afgRecoveryEnabled) {
        // Queue deferred AFG D0 recovery.
        if (afgRecoveryCall != nullptr)
            thread_call_enter1(afgRecoveryCall, reinterpret_cast<thread_call_param_t>(that));
    }
        
    return ret;
}

void HDAWAKE::recoverAFG(thread_call_param_t param0, thread_call_param_t param1) {

    AppleHDANode *that = reinterpret_cast<AppleHDANode *>(param1);
    
    if (that == nullptr)
        return;
    
    callbackHDAWAKE->wrapSetPowerState(that, 0, true);
}

void HDAWAKE::loadConfiguration(DeviceInfo *deviceInfo) {
    
    if (deviceInfo) {
        
        // Retrieve HDEF device properties for AFG configuration.
        auto hdef = deviceInfo->audioBuiltinAnalog;
        
        if (hdef) {
            
            OSData *afgRecoveryEnabledValue = OSDynamicCast(OSData, hdef->getProperty("afg-recovery"));
            if (afgRecoveryEnabledValue && afgRecoveryEnabledValue->getLength() > 0 &&
                static_cast<const uint8_t *>(afgRecoveryEnabledValue->getBytesNoCopy())[0] != 0) {
                callbackHDAWAKE->afgRecoveryEnabled = true;
            }
            
            OSData *afgForceEnabledValue = OSDynamicCast(OSData, hdef->getProperty("afg-force"));
            if (afgForceEnabledValue && afgForceEnabledValue->getLength() > 0 &&
                static_cast<const uint8_t *>(afgForceEnabledValue->getBytesNoCopy())[0] != 0) {
                callbackHDAWAKE->afgForceEnabled = true;
            }
            
            if (WIOKit::getOSDataValue(hdef, "afg-node", callbackHDAWAKE->selectedNodeForRecovery)) {
                DBGLOG("afg", "selected node: %u", callbackHDAWAKE->selectedNodeForRecovery);
            }
            
            if (WIOKit::getOSDataValue(hdef, "afg-delay", callbackHDAWAKE->d0EntryDelay)) {
                DBGLOG("afgondelay", "found afgondelay override %u", callbackHDAWAKE->d0EntryDelay);
                
                // Delay cannot be longer than one second.
                if (callbackHDAWAKE->d0EntryDelay > 1000000)
                    callbackHDAWAKE->d0EntryDelay = 1000000;
            }
            
        } else {
            SYSLOG("afg", "analog controller not found");
        }
        
        DeviceInfo::deleter(deviceInfo);
    }
}

void HDAWAKE::processKext(KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size) {
    if (progressState != ProcessingState::EverythingDone) {
        if (kext.loadIndex == index) {
            DBGLOG("hdawake", "found %s", kext.id);
            
            orgNodeID = reinterpret_cast<t_NodeID>(patcher.solveSymbol(index, "__ZN12AppleHDANode6nodeIDEv"));
            orgVendorProductID = reinterpret_cast<t_VendorProductID>(patcher.solveSymbol(index, "__ZN12AppleHDANode15vendorProductIDEv"));
            
            loadConfiguration(DeviceInfo::create());
            
            KernelPatcher::RouteRequest requests[] {
                KernelPatcher::RouteRequest("__ZN12AppleHDANode13setPowerStateEjb", wrapSetPowerState, orgSetPowerState),
                KernelPatcher::RouteRequest(symPerformFormatChange, performFormatChange, orgPerformFormatChange)
            };
            patcher.routeMultiple(index, requests, address, size);
            
            if (patcher.getError() == KernelPatcher::Error::NoError) {
                DBGLOG("hdawake", "setPowerState hook installed");
                progressState |= ProcessingState::HDAPatched;
            } else {
                SYSLOG("hdawake", "failed to hook setPowerState");
                patcher.clearError();
                return;
            }
        }
    }
}
