# Working Instructions — Murphy-M3

## Mission
Get Crosspoint firmware fully working on the Murphy M3: EPUB + MOBI reading, PDF support
(stretch goal), and a usable audio player replacing the limited stock one.

## Ground rules
1. Stock firmware backup in `backup/stock-firmware/` is sacred. Verify it exists and its
   SHA256 matches `SHA256SUMS` before any flash write.
2. Flash writes to the device require TBOT's explicit go-ahead each session.
3. Device is on native USB-Serial/JTAG (`/dev/ttyACM0`) — it cannot be soft-bricked while
   eFuses stay untouched. NEVER burn eFuses.
4. Upstream Crosspoint stays pristine in `crosspoint/`; M3 changes go on a branch/fork.
5. Keep `analysis/` docs current as facts land; they are the shared brain of the port.

## Quick commands
- Probe: `esptool --port /dev/ttyACM0 flash-id`
- Backup: `esptool --port /dev/ttyACM0 read-flash 0x0 0x1000000 out.bin`
- Restore stock: `esptool --port /dev/ttyACM0 write-flash 0x0 backup/stock-firmware/murphy-m3-full-flash-16MB.bin`
