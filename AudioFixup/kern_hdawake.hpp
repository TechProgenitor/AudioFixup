//
//  kern_hdawake.hpp
//  AudioFixup
//
//  Copyright © 2026 TechProgenitor. All rights reserved.
//

#ifndef kern_hdawake_hpp
#define kern_hdawake_hpp

#include <Headers/kern_patcher.hpp>
#include <IOKit/IOService.h>
#include <IOKit/audio/IOAudioStream.h>

class AppleHDAEngineOutput;
class AppleHDANode;
class DeviceInfo;

class HDAWAKE {
public:
    bool init();
    void deinit();

private:
    /**
     *  Private self instance for callbacks
     */
    static HDAWAKE *callbackHDAWAKE;
    
    /**
     * Patch kext if needed and prepare other patches
     *
     * @param patcher KernelPatcher instance
     * @param index   kinfo handle
     * @param address kinfo load address
     * @param size    kinfo memory size
     */
    void processKext(KernelPatcher &patcher, size_t index,
                     mach_vm_address_t address, size_t size);
    
    void loadConfiguration(DeviceInfo *deviceInfo);
    bool afgRecoveryEnabled = false;
    bool afgForceEnabled = false;
    uint8_t selectedNodeForRecovery = 1;
    uint32_t selectedCodecForRecovery = 0;
    uint32_t d0EntryDelay = 0;

    /**
     * setPowerState wrapper
     */
    static int wrapSetPowerState(AppleHDANode *that, uint powerState, bool force);
    
    /**
     * Original setPowerState
     */
    mach_vm_address_t orgSetPowerState {};
    
    /**
     *  Prototype of orgNodeID
     */
    using t_NodeID = uint8_t (*)(AppleHDANode *);
    
    /**
     *  Original nodeID function
     */
    t_NodeID orgNodeID {};
    
    /**
     *  Prototype of orgVendorProductID
     */
    using t_VendorProductID = uint64_t (*)(AppleHDANode *);
    
    /**
     *  Original vendorProductID function
     */
    t_VendorProductID orgVendorProductID {};

    /**
     * Concurrent Thread for Deferring D0
     */
    static void recoverAFG(thread_call_param_t param0, thread_call_param_t param1);
    
    /**
     *  performFormatChange method symbol
     */
    static constexpr const char *symPerformFormatChange = "__ZN20AppleHDAEngineOutput19performFormatChangeEP13IOAudioStreamPK20_IOAudioStreamFormatPK18_IOAudioSampleRate";
    
    /**
     *  Hooked performFormatChange method triggering a verb sequence on jack event
     */
    static int performFormatChange(AppleHDAEngineOutput *that, IOAudioStream *stream, const IOAudioStreamFormat *format, const IOAudioSampleRate *samplerate);
    
    /**
     *  AppleHDAEngineOutput::performFormatChange original method
     */
    mach_vm_address_t orgPerformFormatChange {0};
    
    AppleHDANode *AFGNodePtr = nullptr;
    
    /**
     * Current progress mask
     */
    struct ProcessingState {
        enum {
            NothingReady = 0,
            HDAPatched = 2,
            EverythingDone = HDAPatched,
        };
    };

    int progressState {ProcessingState::NothingReady};
};

#endif /* kern_hdawake_hpp */
