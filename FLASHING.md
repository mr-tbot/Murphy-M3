# CrossPoint firmware for the Murphy M3

Custom firmware for the **Murphy M3** e-reader — also sold as the **HamGeek M3**
and the corogoo **墨菲 / "MoFei"** — built from [CrossPoint](https://github.com/crosspoint-reader/crosspoint-reader)
with a full hardware port plus MOBI, PDF and audio support added.

**This is unofficial.** It is not from CrossPoint upstream and not from the
device vendor. It works well on my unit; it has not been through anyone else's
QA. Read the "before you flash" section.

## What you get

- **EPUB** reading with the full CrossPoint reader: pagination cache, TOC,
  bookmarks, progress, dictionary, OPDS, WiFi transfer, KOReader sync.
- **MOBI / AZW** — converted on device, once per book, then read natively.
  Includes Huffman-compressed books and GIF illustrations.
- **PDF** — text reflowed into a real book, with embedded images. Text PDFs work
  well; scanned PDFs work when the page images are JPEG.
- **Audio player** — MP3, WAV, FLAC, plus AAC, M4A, Opus and Vorbis. Folder
  playlists, tap-to-seek, volume control. Its own **Audio** section on the home
  screen.
- Working touch and gestures, correct e-ink waveforms (no ghosting), battery
  percentage, charging indicator, frontlight control, 5–10 pt reader fonts, and
  a UI scaled for the 240×416 panel.

## Before you flash

- **Back up your stock firmware first.** One command, and it is the only way
  back to exactly what you had:
  ```bash
  esptool --port /dev/ttyACM0 read-flash 0x0 0x1000000 stock-backup-16MB.bin
  ```
  Keep that file. To restore: `esptool --port /dev/ttyACM0 write-flash 0x0 stock-backup-16MB.bin`
- **Your backup contains your WiFi password** in its NVS partition. Do not post
  it anywhere. (This is also why no dump is included here.)
- The M3 cannot be permanently bricked by flashing: it has native USB-Serial/JTAG
  with no security eFuses burned, so the ROM bootloader is always reachable.
- Books live on the microSD card, not in flash, so flashing does not touch them.

## Flashing

You need [esptool](https://github.com/espressif/esptool) (`pip install esptool`).
Connect over USB-C; the device shows up as `/dev/ttyACM0` on Linux, `/dev/cu.usbmodem*`
on macOS, or a COM port on Windows.

If the device does not appear, hold the **bottom** button while plugging in to
force the ROM bootloader.

### Option A — keep your settings (app only)

Preserves the WiFi credentials and settings already stored on the device.

```bash
esptool --port /dev/ttyACM0 --chip esp32s3 write-flash 0x10000 firmware.bin
```

### Option B — clean install (full image)

Writes bootloader, partition table and app. Overwrites stored WiFi credentials.

```bash
esptool --port /dev/ttyACM0 --chip esp32s3 write-flash 0x0 firmware-full-16MB.bin
```

Verify what you downloaded against `FIRMWARE-SHA256.txt` before flashing.

After flashing, press a button to wake the device.

## Notes and known limits

- **First open of a MOBI or PDF takes a few seconds** while it converts. It is
  cached afterwards, so later opens are instant. A firmware update re-converts
  once.
- **DRM'd books do not work** and never will — the converter rejects them with a
  message rather than producing garbage. Pure KF8 `.azw3` files are also
  rejected; re-export as MOBI or EPUB.
- **Scanned PDFs**: pages stored as JPEG convert fine. Pages stored as
  full-resolution Flate-compressed rasters are skipped (they exceed the decode
  budget). PDFs needing JPEG-2000, CCITT fax or LZW image decoding skip those
  images.
- **Illustrations larger than 2048×3072** transcode but will not display — a
  pre-existing limit in the reader's image decoder.
- The device sleeps on idle, and USB disappears when it does. Press a button
  before flashing.
- Confirm is a long-ish press for sleep (1.5 s) because that button is shared
  with power.

## Building it yourself

```bash
git clone --recurse-submodules <this fork>
cd crosspoint
pio run -e murphy
```

The `murphy` environment is committed, so a clean checkout builds. It needs the
`freeink-sdk` submodule branch carrying the Murphy board profile.

## Credit

[CrossPoint](https://github.com/crosspoint-reader/crosspoint-reader) and the
[FreeInk SDK](https://github.com/Free-Ink/freeink-sdk) are the upstream projects
doing the hard work here — this is a port and a set of features on top of them.
The hardware findings and the port are offered back upstream as pull requests.
