AudioFixup Changelog
=============================

#### v1.2.1
- Remove the `afg-codec` configuration property.
- Automatically identify analog codecs using PCI vendor IDs.
- Avoid unnecessary AFG operations on digital codecs.

#### v1.2.0
- Added `afg-force` device property.
- Migrated AFG and display_change_handler configuration from boot arguments to device properties:
  - `afgondelay` -> `afg-delay`
  - `afgrecovery` -> `afg-recovery`
  - `afgcodec` -> `afg-codec`
  - `afgnode` -> `afg-node`
  - `displaychangedelay` (global) -> `audio-port#-delay` (per-port)

#### v1.1.0
- Renamed `audio-port#-codec-info` to `audio-port#-info`.
- Added byte-offset configuration properties for `audio-codec-info`:
  - `audio-port#-adr` — configure the `Codec Address` byte.
  - `audio-port#-afg` — configure the `Audio Function Group` byte.
  - `audio-port#-pin` — configure the `Digital Pin` byte.
  - `audio-port#-end` — configure the `Active Display Endpoint` byte.
- Added `afgnode` to select the AFG node ID used by `afgondelay` and `afgrecovery` (default: 1).

#### v1.0.0
- Initial release
