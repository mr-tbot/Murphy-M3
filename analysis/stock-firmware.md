# Stock Firmware Analysis — TBOT's Murphy M3 unit

*Dumped 2026-07-29 over native USB-Serial/JTAG (`/dev/ttyACM0`), esptool v5.2.0.*

## Chip / security state (probed)

| Item | Value |
|---|---|
| SoC | ESP32-S3 (QFN56) rev v0.2, dual-core 240 MHz, 40 MHz crystal |
| PSRAM | 8 MB embedded (AP_3v3 → ESP32-S3R8, bare chip) |
| Flash | 16 MB quad-SPI, GigaDevice (mfr `c8`, dev `4018` = GD25Q128) |
| USB | Native USB-Serial/JTAG (`303a:1001`) |
| MAC | `48:ca:43:a4:63:38` |
| Flash encryption | **Disabled** (`SPI_BOOT_CRYPT_CNT = 0b000`) |
| Secure boot | **Disabled** (`SECURE_BOOT_EN = 0`) |
| USB-JTAG | Enabled (`DIS_USB_JTAG = 0`) |

No security fuses burned → the device can always be recovered over USB. Effectively unbrickable as long as no one burns eFuses.

## Partition table (at `0x8000`)

| Label | Type | Offset | Size | Content |
|---|---|---|---|---|
| nvs | data/nvs | `0x9000` | 20 KB | WiFi creds (⚠ contains TBOT's SSID/PSK), weather API host, `eeprom` namespace |
| otadata | data/ota | `0xe000` | 8 KB | OTA selector |
| app0 | app/ota_0 | `0x10000` | 6976 KB | **Active stock app** |
| app1 | app/ota_1 | `0x6e0000` | 6976 KB | Blank (0xFF) — OTA slot never used |
| spiffs | data/spiffs | `0xdb0000` | 2 MB | 100 % blank — actually mounted as LittleFS by the app, never written |
| coredump | data/coredump | `0xff0000` | 64 KB | Has data (device has crashed at least once) |

Books/music live on the **microSD card** (4-bit SD_MMC), not internal flash.

## App image (`app0`)

- Arduino framework via `arduino-lib-builder`, **ESP-IDF v4.4.7-dirty**, PlatformIO env `esp32-s3-devkitc-1`, compiled **Mar 5 2024 12:12:53** on Windows (user `HZW`).
- Image: 7,143,424 bytes, DIO @ 80 MHz, entry `0x40377b70`, valid SHA footer.
- **App0 SHA256 `33cceddd…`** — differs from the dump recorded in `crosspoint-reader/Murphy`
  (`a6f205d9…`): TBOT's unit runs a *different stock build* than the one analyzed upstream.
  Worth contributing this dump/version string upstream.

## What the strings reveal

- **Branding**: web UI titled **"MoFei"** (墨菲 = "Murphy"), NVS contains `COROGOO`, BLE name `E-Paper Reader`. Vendor = corogoo DIY team; OTA source is their Gitee repo.
- **Reader**: custom `src/epub_parser.cpp` (EPUB + TXT), read-marks per chapter/file ID.
- **Audio**: bundled **ESP32-audioI2S 3.0.12** (schreibfaul1) — Helix MP3, AAC, WAV, FLAC, Opus, Vorbis software decode over I2S. `Audiobooks` + music-task strings present. The `/v1/audio/speech` OpenAI TTS path is dead vendored library code (never referenced).
- **Storage**: SD_MMC (with `setPins` — custom pin map) + LittleFS; USB MSC ("MOFEI Storage").
- **Network**: WiFi web UI (file manager `/upload`, OTA `/update`, weather config, "Universal Input"), mDNS `mofei.local`, NTP via Tencent, weather via `qweatherapi.com`; BLE GATT services.
- **No display-controller name in plaintext** — the UC8253 identification comes from the
  upstream `crosspoint-reader/Murphy` Ghidra work, not from strings.

## Files

- `backup/stock-firmware/murphy-m3-full-flash-16MB.bin` — full dump, SHA256 in `SHA256SUMS`
- `backup/stock-firmware/extracted/` — bootloader + each partition split out (`partitions.json`)

### Restore stock (full)

```bash
esptool --port /dev/ttyACM0 write-flash 0x0 backup/stock-firmware/murphy-m3-full-flash-16MB.bin
```

> ⚠️ **Never publish the raw dump or the `nvs` partition** — it contains WiFi credentials.
> Zero `0x9000`–`0xE000` first if sharing.
