# Murphy M3 × CrossPoint

Everything needed to run the open-source
[CrossPoint](https://github.com/crosspoint-reader/crosspoint-reader) e-reader firmware on the
**Murphy M3** (HamGeek M3 / 墨菲) ESP32-S3 e-reader, in one place: a full stock-firmware
backup, the CrossPoint source tree + FreeInk SDK (which already carries a Murphy M3 board
profile), the community reverse-engineering work, and a deep-dive port analysis.

**Start here → [analysis/crosspoint-port-deep-dive.md](analysis/crosspoint-port-deep-dive.md)**

## Device (probed over USB)

| Component | Detail |
|---|---|
| SoC | ESP32-S3 (QFN56) rev v0.2, dual-core 240 MHz |
| PSRAM | 8 MB embedded (ESP32-S3R8) |
| Flash | 16 MB quad-SPI (GigaDevice) |
| USB | Native USB-Serial/JTAG (`303a:1001`) |
| Security | No flash encryption, no secure boot — freely dumpable/reflashable |

## Repo layout

- [`backup/stock-firmware/`](backup/stock-firmware/) — full 16 MB stock flash dump + extracted partitions (the way back to stock; **contains WiFi creds in NVS — never publish raw**)
- [`crosspoint/`](crosspoint/) — CrossPoint firmware, `develop` branch (submodule, includes freeink-sdk)
- [`upstream/freeink-sdk/`](upstream/freeink-sdk/) — FreeInk SDK: `MurphyM3` board profile, `Uc8253MurphyDriver`, ES8388 AudioManager, `[env:murphy]` (submodule)
- [`upstream/murphy/`](upstream/murphy/) — crosspoint-reader/Murphy: M3/M4 dumps, Ghidra findings, probe sketches (submodule; local branches `pr1-teardown`, `pr2-schematic`)
- [`upstream/community-sdk/`](upstream/community-sdk/) — branch `feat-support-for-m3`, the original working M3 display/input/SD code (submodule)
- [`upstream/freeink-reader/`](upstream/freeink-reader/) — minimal FreeInk SDK reader app, likely home of a public M3 build (submodule)
- [`analysis/`](analysis/) — [stock firmware analysis](analysis/stock-firmware.md), [the port deep dive](analysis/crosspoint-port-deep-dive.md), [research sources](analysis/research-sources.md)
- [`docs/hardware/`](docs/hardware/) — community reverse-engineered schematic PDF

> **⚠️ Privacy note:** the full-flash dump includes the device's NVS partition, which stores
> previously-configured WiFi credentials. Do not publish `backup/` contents without zeroing
> the NVS region (`0x9000`–`0xE000`) first.

## Restore stock firmware

```bash
esptool --port /dev/ttyACM0 write-flash 0x0 backup/stock-firmware/murphy-m3-full-flash-16MB.bin
```

*(This README is updated as the port progresses.)*
