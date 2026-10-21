//
//  kern_codecinfo.hpp
//  AudioFixup
//
//  Copyright © 2026 TechProgenitor. All rights reserved.
//

#ifndef kern_codecinfo_hpp
#define kern_codecinfo_hpp

#include <Headers/kern_patcher.hpp>
#include <IOKit/IOService.h>

class CodecInfo {
public:
    bool init();
    void deinit();

private:
    /**
     *  Private self instance for callbacks
     */
    static CodecInfo *callbackCodecInfo;
    
    /**
     * Patch kext if needed and prepare other patches
     *
     * @param patcher KernelPatcher instance
     * @param index   kinfo handle
     * @param address kinfo load address
     * @param size    kinfo memory size
     */
    void processHDA(KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size);
    void processGFXHDA(KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size);

    /**
     * getOSDataOrOSNumberPropertyFromEntry wrapper
     */
    static IOReturn wrapGetOSDataOrOSNumberPropertyFromEntry(void *controller, IORegistryEntry *framebuffer, const char *property, uint *value, IOReturn (*wrapFunction)(void*, IORegistryEntry*, const char*, uint*), mach_vm_address_t orgFunction);
    
    static IOReturn wrapAppleHDA(void *controller, IORegistryEntry *framebuffer, const char *property, uint *value);
    static IOReturn wrapAppleGFXHDA(void *controller, IORegistryEntry *framebuffer, const char *property, uint *value);
    
    /**
     * Original getOSDataOrOSNumberPropertyFromEntry
     */
    mach_vm_address_t orgAppleHDAGetOSDataOrOSNumberPropertyFromEntry {};
    mach_vm_address_t orgAppleGFXHDAGetOSDataOrOSNumberPropertyFromEntry {};

    /**
     * Current progress mask
     */
    struct ProcessingHDAState {
        enum {
            NothingReady = 0,
            AudioPatched = 2,
            EverythingDone = AudioPatched,
        };
    };
    
    struct ProcessingGFXHDAState {
        enum {
            NothingReady = 0,
            AudioPatched = 2,
            EverythingDone = AudioPatched,
        };
    };

    int progressHDAState {ProcessingHDAState::NothingReady};
    int progressGFXHDAState {ProcessingGFXHDAState::NothingReady};
};

#endif /* kern_codecinfo_hpp */
