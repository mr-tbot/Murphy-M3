# Murphy-M3 — Agent Guide

Project: port the open-source **Crosspoint** e-reader firmware to the **Murphy M3**
(ESP32-S3R8, 16MB flash, e-ink, built-in audio). Owner: TBOT (personal, public-grade docs).

## Repo layout
- `backup/` — gitignored. Stock flash dump + extracted partitions (NVS holds WiFi creds, never commit). NEVER delete;
  this is the only path back to stock.
- `crosspoint/` — clone of upstream Crosspoint firmware (git clone, treated read-mostly;
  M3 work happens on a branch or fork).
- `analysis/` — firmware dump analysis + port deep-dive docs.
- `docs/` — public-grade documentation.
- `.agent/` — gitignored agent memory/scratch/backups.

## Hardware facts (probed, trust these)
- ESP32-S3 QFN56 rev0.2, 8MB embedded PSRAM (S3R8), 16MB quad flash (GigaDevice), native
  USB-Serial/JTAG on /dev/ttyACM0, no flash encryption, no secure boot.
- Full restore: `esptool --port /dev/ttyACM0 write-flash 0x0 <your-stock-backup>.bin`

## Conventions
- esptool v5 CLI (dash commands: `read-flash`, `write-flash`, `flash-id`).
- Never write to device flash without an up-to-date backup and TBOT's go-ahead.
- Analysis docs in `analysis/` are full prose (user-facing), not caveman-compressed.
