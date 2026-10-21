//
//  kern_setup.cpp
//  AudioFixup
//
//  Copyright © 2026 TechProgenitor. All rights reserved.
//

#include "kern_setup.hpp"

AUDIOFIXUP *AUDIOFIXUP::callbackAudio;

void AUDIOFIXUP::init() {
	callbackAudio = this;
	cdi.init();
    dcd.init();
    wak.init();
}

void AUDIOFIXUP::deinit() {
	cdi.deinit();
    dcd.deinit();
    wak.deinit();
}
