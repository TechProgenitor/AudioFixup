//
//  kern_start.cpp
//  AudioFixup
//
//  Copyright © 2026 TechProgenitor. All rights reserved.
//

#include <Headers/plugin_start.hpp>
#include <Headers/kern_api.hpp>

#include "kern_setup.hpp"

static AUDIOFIXUP audio;

static const char *bootargOff[] {
	"-afoff"
};

static const char *bootargDebug[] {
	"-afdbg"
};

static const char *bootargBeta[] {
	"-afbeta"
};

PluginConfiguration ADDPR(config) {
	xStringify(PRODUCT_NAME),
    parseModuleVersion(xStringify(MODULE_VERSION)),
    LiluAPI::AllowNormal | LiluAPI::AllowInstallerRecovery | LiluAPI::AllowSafeMode,
	bootargOff,
	arrsize(bootargOff),
	bootargDebug,
	arrsize(bootargDebug),
	bootargBeta,
	arrsize(bootargBeta),
	KernelVersion::Catalina,
	KernelVersion::Tahoe,
	[]() {
        audio.init();
	}
};
