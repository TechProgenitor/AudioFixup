AudioFixup
===================

A Lilu plugin for macOS that fixes various audio-related issues.

### Features
- Route HDMI and DisplayPort audio outputs to the correct nodes.
- Delay and defer D0 power state transitions for the AFG node.
- Delay display change handling to give GPU audio codecs time to initialize.

### Compatibility
Designed for macOS Catalina (10.15) through macOS Tahoe (26).

### Installation

1) Copy `AudioFixup.kext` to the `Kexts` folder on your bootloader's EFI partition.
2) Add `AudioFixup.kext` to the `Kernel -> Add` section of your `config.plist` after Lilu.

Example OpenCore entry:

```xml
<dict>
    <key>Arch</key>
    <string>x86_64</string>
    <key>BundlePath</key>
    <string>AudioFixup.kext</string>
    <key>Comment</key>
    <string>AudioFixup</string>
    <key>Enabled</key>
    <true/>
    <key>ExecutablePath</key>
    <string>Contents/MacOS/AudioFixup</string>
    <key>MaxKernel</key>
    <string>25.99.99</string>
    <key>MinKernel</key>
    <string>19.0.0</string>
    <key>PlistPath</key>
    <string>Contents/Info.plist</string>
</dict>
```

### Device Properties
The following properties are added to the `IGPU` and/or `GFX#` device(s).

| Property | Description |
|---------|-------------|
| `audio-patch-enable` | Enable the audio patching mechanism. |
| `audio-port#-info` | Set the `audio-codec-info` device property for a given `port-number` when hotplugged to a specified value. |
| `audio-port#-adr` | Set the `Codec Address` byte for a given `port-number` when hotplugged to a specified value. |
| `audio-port#-afg` | Set the `Audio Function Group` byte for a given `port-number` when hotplugged to a specified value. |
| `audio-port#-pin` | Set the `Digital Pin` byte for a given `port-number` when hotplugged to a specified value. |
| `audio-port#-end` | Set the `Active Display Endpoint` byte for a given `port-number` when hotplugged to a specified value. |

### Boot Arguments
The AFG-related arguments only apply to codecs that use AppleHDA.

| Argument | Description |
|---------|-------------|
| `audio-codec-info` | Apple boot-arg which sets the `audio-codec-info` device property on all hotplugged ports to a specified value. |
| `afgondelay` | Configure the delay in microseconds before the AFG power state is set to D0. |
| `afgrecovery=1` | Set the AFG power state back to D0 after being set to D1, D2, D3, or D4. |
| `afgcodec` | Limit the `afgondelay` and `afgrecovery` boot-args to the specified `VendorProductID`. <br><br> `VendorProductID` values are stored in the `CodecList` property of `AppleHDAController` in the IORegistry. |
| `afgnode` | Select the AFG node ID used for the `afgondelay` and `afgrecovery` boot-args (default: 1). |
| `displaychangedelay` | Configure the delay in microseconds after a hotplug event before the display change is handled. |
| `-afbeta` | Enable AudioFixup on unsupported macOS versions (26 and below are enabled by default) |
| `-afdbg` | Enable debug printing (available in DEBUG binaries) |
| `-afoff` | Disable AudioFixup |

### Downloads
Available on the [releases](https://github.com/TechProgenitor/AudioFixup/releases) page.

### Credits
- [Apple](https://www.apple.com) for macOS  
- [vit9696](https://github.com/vit9696) for [Lilu](https://github.com/acidanthera/Lilu)
- [TechProgenitor](https://github.com/TechProgenitor/) for writing the software and maintaining it
