AudioFixup Changelog
=============================

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
