# Add Murphy M3 (HamGeek M3 / 墨菲) hardware support

This adds working hardware support for the **Murphy M3** — an ESP32-S3R8 e-reader
sold as the HamGeek M3 / corogoo 墨菲 ("MoFei"), with a 3.7" 240×416 UC8253
panel, CHSC6x capacitive touch, ES8388 audio codec, RX8010 RTC, AHT30
temperature/humidity sensor, a frontlight, and a 4-bit SDMMC microSD slot.

The SDK already carried a `MurphyM3` board profile and a `Uc8253MurphyDriver`,
but several subsystems were stubbed, mis-wired, or wired against guessed values.
Every change here was diagnosed on real hardware and confirmed working on real
hardware. Where a value came out of the OEM firmware I say so and give the
provenance, because a few of them are not guessable.

## Why this is worth taking even if you don't have the device

Four of these changes are not Murphy-specific:

- `PanelDriver::supportsGrayscale()` — a capability flag that lets consumers
  skip building grayscale planes for panels that can't show them. Without it,
  the `displayGray` default silently renders a raw gray plane as if it were a
  B/W frame.
- `freeink::usbHostPresent()` — USB host detection with no dedicated pin, by
  watching the USB-Serial/JTAG start-of-frame counter. Any board whose charger
  IC doesn't expose a status line can use it.
- Gesture thresholds that scale with the digitizer's coordinate span instead of
  being fixed pixel counts, so small panels aren't forced into an unusable
  swipe distance.
- Touch power/rail sequencing separated from controller-specific init, which was
  previously entangled with the GT911 path.

## The work, subsystem by subsystem

### 1. microSD: real 4-bit SDMMC wiring

The profile's SD pins were placeholders, so the card never mounted. Correct
wiring, verified with a 32 GB card:

```
CLK 16   CMD 17   D0 15   D1 14   D2 21   D3 18   powerEnable 10 (active LOW)
```

`powerEnable` being active-low matters: driving it the usual way keeps the card
powered off, which presents exactly like a dead slot.

### 2. Touch: three stacked faults

Touch reported nothing at all. There were three independent problems:

1. **GPIO45 was recorded as the touch controller's reset line.** It is not — it
   is an active-LOW rail gate, and with it high both the touch controller *and*
   the ES8388 disappear from the I²C bus. It is now `powerEnable` with
   `activeHigh = false`.
2. **GPIO43 gates the entire I²C rail** and nothing asserted it. It is now the
   board profile's `power.latch0`, so `holdPowerRails()` raises it before any
   I²C user runs.
3. **CHSC6x reads were issued as repeated-start transactions**, which this
   controller NAKs. Reads are now stop-separated, with a 10 ms Wire timeout.

The power/rail block was also hoisted out of `beginGt911()` into `beginTouch()`,
since it is not GT911-specific.

**Axis mapping.** The CHSC6x decoder ignored the profile's `swapXY`/`flip`
flags (they were only honoured on the GT911 path), and this digitizer reports
its short axis as X while the framebuffer is landscape. The transform is now
applied in `decodeChsc6xFrame`, with `swapXY = true`, `flipY = true`, and
post-swap ranges. Verified by tapping all four corners and reading back the
logical coordinates: top-left (0,0), top-right (239,0), bottom-left (0,413),
bottom-right (239,410).

**Gestures.** Tap-slop and swipe-minimum were fixed pixel values tuned for a
480×800 panel; on a 240×416 canvas a swipe was nearly impossible. They now
scale with the digitizer span. Separately, held-time was inflated by a 120 ms
holdover, which made short taps read as long presses.

### 3. Display: the OEM waveforms, and why the previous ones couldn't work

The LUT tables in `Uc8253MurphyLuts.h` were the OEM's **voltage-configuration
blocks** (`0f8f4f…`), not waveforms. They contain no kick phase and no
DC balance, so the panel ghosted badly and never latched cleanly.

Replaced with the genuine OEM sets, recovered from the stock firmware image and
re-verified byte-for-byte against the OEM touch v525 build:

- **Full refresh (`DEFAULT`)**: the OEM three-phase set — inversion kick,
  settle, destination drive. DC-balanced; this is what actually clears ghosting.
- **Fast refresh**: the OEM's *alternate* package. This is a package deal and
  only works as a whole: the data-driven init variant (`0x82 = 0x07`,
  `0x50 = 0xD7`, plus pointer-fed `0x00/0x01/0x06/0x61` payloads), the alternate
  LUT bank, and the `0x17`/`0xA5` auto refresh trigger. Loaded under the mode-0
  init instead, these same tables flash without latching — a trap worth
  documenting, since it looks like a bad LUT rather than a mismatched package.
  Build with `-DFREEINK_MURPHY_OEM_PARTIAL=0` to fall back to a synthetic
  destination-drive bank under the mode-0 init.

Fast refreshes are differential (previous frame to DTM1, new frame to DTM2) and
every Nth is promoted to a full refresh, since partial kicks accumulate residue.

**Grayscale.** 4-level grayscale was attempted on this panel and abandoned: the
VSH/VSL rails are asymmetric and the result is unusable. That made the
`PanelDriver::displayGray` default actively harmful here — it re-displays the
renderer's gray plane as a B/W frame, producing a near-solid black page. The
driver now overrides it as a no-op and reports `supportsGrayscale() == false`,
which is plumbed through so consumers skip building the planes at all (also a
straight speed win on page turns).

### 4. Battery: the divider is 2.0, not 3.03

VBAT is read on GPIO9 through a 680 kΩ/680 kΩ divider — a factor of 2.0. The
profile carried 3.0303, which reported a charging LiPo as 6.3 V. Measured
2.07 V at the pin, i.e. 4.15 V at the cell.

### 5. Charging indicator without a charge pin

The TP4054's `CHRG` output only drives its own LED; it is not exposed to the
SoC. I confirmed this by probing the one unaccounted GPIO (47) across plug and
unplug cycles — static in both.

Instead, host presence is detected from the USB-Serial/JTAG frame counter
(`USB_SERIAL_JTAG_FRAM_NUM_REG`): if start-of-frame packets are arriving, a host
is attached. Exposed as `freeink::usbHostPresent()` in `BoardConfig.h` and
selected per-board with `FREEINK_USB_PRESENCE_AS_CHARGING`. `BatteryMonitor`
falls back to it when `chargeStatusPin` is unassigned. This is a
"USB connected", not a true "charging" signal, and is commented as such.

### 6. Sleep hold time on a shared button

GPIO0 is both a boot strap and the M3's primary confirm button, and it doubles
as the power button. At the shared 400 ms hold, ordinary selects tripped sleep.
`CONFIRM_POWER_HOLD_MS` is now 1500 ms on this device only.

## Hardware findings that may be useful regardless

Probed and confirmed on the device:

| Address | Device | Notes |
| --- | --- | --- |
| 0x10 | ES8388 audio codec | behind the GPIO45 gate |
| 0x2e | CHSC6x touch | behind the GPIO45 gate |
| 0x32 | RX8010 RTC | present and ticking; no SDK driver yet |
| 0x38 | AHT30 temp/humidity | present; no SDK driver yet |

- GPIO43 gates the whole I²C rail.
- GPIO45 is an active-LOW gate for touch *and* the codec.
- GPIO47 is unaccounted for and is **not** a charge-status pin.
- GPIO33–37 are the octal PSRAM lines.
- The USB-Serial/JTAG bootloader is always reachable and no security eFuses are
  burned, so the device is effectively unbrickable — worth knowing before
  anyone hesitates to flash one.

One trap for anyone else developing on this board: after an esptool or
PlatformIO session, the reset leaves the download strap latched, so the device
comes back in ROM download mode rather than running the app. A physical replug
clears it, as does an RTC-WDT reset issued over JTAG.

## Testing

All of the above was verified on hardware, iteratively, with a human at the
device reporting results and serial captures for the parts that log. Concretely
confirmed working: SD mount and file listing from a 32 GB card, touch across the
full panel with all four corners landing exactly, swipe and tap gestures,
full-refresh and partial-refresh quality (no visible ghosting or afterimage),
battery percentage tracking a real charge, the charging indicator following
plug/unplug, frontlight control, sleep only on a deliberate long press, and
books rendering correctly rather than as dark pages.

## Not included here

Audio, MOBI, PDF and the reader-side UI work live in the application layer and
are proposed separately. The RX8010 RTC and AHT30 drivers are not written yet.
