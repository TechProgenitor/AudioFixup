//
//  kern_displaychangedelay.hpp
//  AudioFixup
//
//  Copyright © 2026 TechProgenitor. All rights reserved.
//

#ifndef kern_displaychangedelay_hpp
#define kern_displaychangedelay_hpp

#include <Headers/kern_patcher.hpp>
#include <IOKit/IOService.h>
#include <IOKit/graphics/IOGraphicsTypes.h>
#include <IOKit/graphics/IOFramebuffer.h>

class IOAccelDisplayPipe;

class DISPLAYCHANGEDELAY {
public:
    bool init();
    void deinit();

private:
    /**
     *  Private self instance for callbacks
     */
    static DISPLAYCHANGEDELAY *callbackDisplayChangeDelay;
    
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

    /**
     * display_change_handler wrapper
     */
    static IOReturn wrapDisplayChangeHandler(IOAccelDisplayPipe *that, void *param_1, IOFramebuffer *param_2, IOIndex param_3, void *param_4);
    
    /**
     * Original display_change_handler
     */
    mach_vm_address_t orgDisplayChangeHandler {};

    /**
     * Current progress mask
     */
    struct ProcessingState {
        enum {
            NothingReady = 0,
            DisplayChangeDelayPatched = 2,
            EverythingDone = DisplayChangeDelayPatched,
        };
    };

    int progressState {ProcessingState::NothingReady};
};

#endif /* kern_displaychangedelay_hpp */
