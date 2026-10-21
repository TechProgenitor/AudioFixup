//
//  kern_setup.hpp
//  AudioFixup
//
//  Copyright © 2026 TechProgenitor. All rights reserved.
//

#ifndef kern_setup_hpp
#define kern_setup_hpp

#include "kern_codecinfo.hpp"
#include "kern_displaychangedelay.hpp"
#include "kern_hdawake.hpp"

class AUDIOFIXUP {
public:
	void init();
	void deinit();

private:
	/**
	 *  Private self instance for callbacks
	 */
	static AUDIOFIXUP *callbackAudio;

	/**
	 *  Codec Info fixes instance
	 */
	CodecInfo cdi;
    
    /**
     *  Display Change Delay fixes instance
     */
    DISPLAYCHANGEDELAY dcd;
    
    /**
     *  Power state fixes instance
     */
    HDAWAKE wak;
};

#endif /* kern_setup_hpp */
