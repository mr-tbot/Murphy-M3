# Murphy M3 × CrossPoint

Everything needed to run the open-source
[CrossPoint](https://github.com/crosspoint-reader/crosspoint-reader) e-reader firmware on the
**Murphy M3** (HamGeek M3 / 墨菲) ESP32-S3 e-reader, in one place: a full stock-firmware
notes, the CrossPoint source tree + FreeInk SDK (which already carries a Murphy M3 board
profile), the community reverse-engineering work, and a deep-dive port analysis.

**Start here → [analysis/crosspoint-port-deep-dive.md](analysis/crosspoint-port-deep-dive.md)**


## Prebuilt firmware

If you own this device and just want to try it:
**[Download the firmware release](https://github.com/mr-tbot/crosspoint-reader/releases/tag/murphy-m3-v1)**
— flashing instructions, checksums and known limits are in [FLASHING.md](FLASHING.md).
Take your own stock backup first; it is the only way back to factory firmware.

## Upstream pull requests

- Hardware support: [Free-Ink/freeink-sdk#25](https://github.com/Free-Ink/freeink-sdk/pull/25)
- Reader, formats and audio: [crosspoint-reader/crosspoint-reader#2794](https://github.com/crosspoint-reader/crosspoint-reader/pull/2794)

## Device (probed over USB)

| Component | Detail |
|---|---|
| SoC | ESP32-S3 (QFN56) rev v0.2, dual-core 240 MHz |
| PSRAM | 8 MB embedded (ESP32-S3R8) |
| Flash | 16 MB quad-SPI (GigaDevice) |
| USB | Native USB-Serial/JTAG (`303a:1001`) |
| Security | No flash encryption, no secure boot — freely dumpable/reflashable |

## Repo layout

- `backup/stock-firmware/` — *not in this repository.* A stock flash dump is the way back to
  factory firmware, it is best practice to take your own backup before flashing this firmware.  The crosspoint official repo has backups for this device available.
- [`crosspoint/`](crosspoint/) — CrossPoint firmware, `develop` branch (submodule, includes freeink-sdk)
- [`upstream/freeink-sdk/`](upstream/freeink-sdk/) — FreeInk SDK: `MurphyM3` board profile, `Uc8253MurphyDriver`, ES8388 AudioManager, `[env:murphy]` (submodule)
- [`upstream/murphy/`](upstream/murphy/) — crosspoint-reader/Murphy: M3/M4 dumps, Ghidra findings, probe sketches (submodule; local branches `pr1-teardown`, `pr2-schematic`)
- [`upstream/community-sdk/`](upstream/community-sdk/) — branch `feat-support-for-m3`, the original working M3 display/input/SD code (submodule)
- [`upstream/freeink-reader/`](upstream/freeink-reader/) — minimal FreeInk SDK reader app, likely home of a public M3 build (submodule)
- [`analysis/`](analysis/) — [stock firmware analysis](analysis/stock-firmware.md), [the port deep dive](analysis/crosspoint-port-deep-dive.md), [research sources](analysis/research-sources.md)
- [`docs/hardware/`](docs/hardware/) — community reverse-engineered schematic PDF

> **⚠️ Privacy note:** the full-flash dump includes the device's NVS partition, which stores
> previously-configured WiFi credentials, so it is never committed here. Keep yours private,
> and treat it as sensitive if you share it.
> the NVS region (`0x9000`–`0xE000`) first.

## Restore stock firmware

```bash
# Take your own backup first -- this is the only way back to factory firmware:
esptool --port /dev/ttyACM0 read-flash 0x0 0x1000000 stock-backup-16MB.bin
# ...and to restore it later:
esptool --port /dev/ttyACM0 write-flash 0x0 stock-backup-16MB.bin
```

*(This README is updated as the port progresses.)*
