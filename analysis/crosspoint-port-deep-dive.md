# CrossPoint on the Murphy M3 — Deep Dive

*Researched and written 2026-07-29. Sources: crosspoint-reader org repos, Free-Ink org repos,
firmware dump analysis of TBOT's unit, and fresh MuPDF memory measurements. All repos referenced
here are vendored in this repo (see [README](../README.md)).*

---

## 1. The cast of characters

| Repo (vendored here) | What it is | Why it matters for the M3 |
|---|---|---|
| [`crosspoint/`](../crosspoint/) — [crosspoint-reader/crosspoint-reader](https://github.com/crosspoint-reader/crosspoint-reader) | The main firmware (6.6k★, MIT, ~1,089 commits, 108 in July 2026 alone). EPUB 2/3, TXT, XTC, BMP; StarDict dictionaries; KOReader sync. v1.4.1 stable, v1.5.0 RC (adds ESP32-S3 support). | The application we want. **No M3 code in it yet** — S3 support landed in v1.5.0 via the Sticky device, which proves the app runs on our MCU family. |
| [`upstream/freeink-sdk/`](../upstream/freeink-sdk/) — [Free-Ink/freeink-sdk](https://github.com/Free-Ink/freeink-sdk) | Hardware SDK (CrossPoint's `develop` uses it as a submodule). Panel drivers (SSD1677/UC8253/UC8279/UC8179/IT8951…), BoardConfig profiles, audio, touch. | **Already has a full `MurphyM3` board profile**: `Uc8253MurphyDriver`, hand-extracted LUTs, `[env:murphy]` build env, touch/frontlight/audio capabilities auto-enabled. This is where the port lives. |
| [`upstream/murphy/`](../upstream/murphy/) — [crosspoint-reader/Murphy](https://github.com/crosspoint-reader/Murphy) | Justin Mitchell's (itsthisjustin) M3/M4 reverse-engineering repo: flash dumps, Ghidra analysis, extracted UC8253 LUTs, probe sketches, findings docs. | The knowledge base. Local branches `pr1-teardown` (16 teardown photos) and `pr2-schematic` (reverse schematic, extracted to [docs/hardware/](../docs/hardware/)) are fetched. |
| [`upstream/community-sdk/`](../upstream/community-sdk/) — branch `feat-support-for-m3` | The original OpenX4 community SDK; this branch holds the first working M3 display/input/SD code (6 commits, May 2026). | Historical origin of the port; freeink-sdk's Murphy driver was ported from here. |
| [`upstream/freeink-reader/`](../upstream/freeink-reader/) — [Free-Ink/freeink-reader](https://github.com/Free-Ink/freeink-reader) | Minimal e-reader app built on freeink-sdk. | The most likely place a public M3 *application* build lands first. No Murphy code yet. |

Key people: **Dave Allie** (CrossPoint author), **itsthisjustin / Justin Mitchell** (CrossPoint
maintainer AND the person doing the M3 port), **corogoo** (the vendor — engaged! They opened
issue #2311 offering "complete hardware schematics" to the team, moved to private email).

## 2. The device (fully reverse-engineered)

Murphy M3 = HamGeek M3, brand 墨菲/MoFei ("Murphy"), by corogoo, ~$69.

| Subsystem | Hardware | Pins / details |
|---|---|---|
| SoC | ESP32-S3R8 (bare chip), 8 MB PSRAM, 16 MB GD25Q128 flash | Same MCU class as Sticky/X4 Pro → CrossPoint v1.5.0 already targets it |
| Display | Good Display **GDEY037T03** 3.7" 416×240, **UC8253** controller | **Bit-banged SPI**: MOSI=3, SCK=4, CS=5, DC=6, RST=7, BUSY=8 (ready-high). 12,480-byte plane. 4-level grayscale possible (hand-tuned LUT exists), OEM shipped B/W only |
| Touch | CHSC6x-class cap touch @ I2C `0x2e` (exact part unconfirmed) | SDA=13, SCL=12 (100 kHz), TS_INT=44, Touch_POWER=45 (power gate) |
| Buttons | 3 physical, active-low | KEY3=GPIO0 (strapping pin!), KEY1=1, KEY2=2 |
| Frontlight | 9 LEDs, PWM 25 kHz, 10 levels | BL_POWER=48 |
| SD | microSD, 4-bit SD_MMC (16 GB card shipped) | D1/D0/CLK/CMD/D3/D2 = 14/15/16/17/18/21; SD_POWER=10 (active-low) |
| Audio | **Everest ES8388 stereo codec** + PJ-342 3.5 mm jack (headset mic + inline-remote on GPIO11 ADC) | I2S: MCLK=42, BCLK=40, LRCK=39, DSDIN=41, ASDOUT=38; codec I2C `0x10`; power via GPIO43→LDO. No speaker, no BT audio |
| RTC | Epson RX8010SJ @ I2C `0x32` | No backup cell |
| Sensors | AHT30 temp/humidity @ I2C `0x38`; 4 kHz buzzer on GPIO46 | |
| Power | TP4054 charger (~196 mA), VBAT ADC on GPIO9 (÷2, scale 3.0303) | Battery: unmarked pouch, ~1200–1500 mAh (guess); charge-status pin unknown |
| Security | No secure boot, no flash encryption (verified on TBOT's unit) | Unbrickable over native USB |

Full reverse schematic: [docs/hardware/Murphy_m3_reverse_schematic_PR2.pdf](../docs/hardware/Murphy_m3_reverse_schematic_PR2.pdf) (community PR, "might contain errors" — audio/RTC/battery pins not yet probe-verified; display/buttons/SD/touch pins match firmware-confirmed values).

Gotchas: GPIO0 is both the bottom button and an S3 strapping pin (hold-at-reset enters
bootloader); GPIO10 must be driven low to power the SD bus; display/SD/touch/audio power
gates must be off in deep sleep.

## 3. Current port status (July 2026)

**Working today** (per `upstream/murphy/m3/README.md` + freeink-sdk code):

- ✅ CrossPoint **boots on the M3** with display, SD, buttons, and frontlight working
- ✅ SDK layer public: `Uc8253MurphyDriver` + LUTs + full `MurphyM3` BoardProfile + `[env:murphy]` in freeink-sdk
- ✅ Battery ADC wired in (commit `56fbd21`)
- ✅ OEM display init sequence documented (five 42-byte LUTs → plane → refresh `0x12`)

**Not done / stubbed:**

- ❌ Touch as an input event source (driver exists at SDK level, not wired into app UI)
- ❌ Audio (SDK `AudioManager` supports "ES8388 (Murphy M3, OEM-recovered register sequence)" **WAV-only**, in a FreeRTOS task — but nothing in the app uses it; upstream scope excludes audio entirely)
- ❌ RX8010SJ RTC (stubbed; upstream scanned wrong I2C addresses — it's `0x32`)
- ❌ UI relayout for 240×416 (CrossPoint UI targets 800×480 / 792×528 — this is the big app-side gap)
- ❌ Partial refresh (commands documented, untested) and grayscale quality tuning
- ❌ **No public application build**: the app-side integration lives in itsthisjustin's local tree. CrossPoint's repo has zero "murphy" hits; no murphy env in its `platformio.ini`.

**Translation:** the hard reverse-engineering is *done*. What remains is app-layer integration
work — exactly the kind of thing an outside contributor can do, since every piece needed is
public across freeink-sdk + community-sdk + the Murphy repo.

## 4. Gap analysis vs TBOT's goals

### 4.1 EPUB — free

CrossPoint's core competency (EPUB 2/3, CSS cache, hyphenation, RTL, dictionaries). Works the
moment the M3 app build exists. The 240×416 layout work is the only real cost.

### 4.2 MOBI — two paths

CrossPoint does **not** support MOBI (and upstream won't take it; format additions historically
go to forks like papyrix-reader).

1. **Convert at sync time (recommended, works day one).** Calibre converts MOBI→EPUB
   losslessly for typical books; the org even maintains `crosspoint-reader/calibre-plugins`.
   Zero firmware work.
2. **On-device MOBI parser (fork-layer feature).** MOBI/PalmDoc is a much simpler format than
   EPUB (LZ77 text records + HTML-ish markup). A `lib/Mobi` sibling to `lib/Epub` feeding the
   same section/renderer pipeline is a realistic ~1–2 week project, best done after the base
   port. KF8/AZW3 = repackaged EPUB internals, also tractable. DRM'd files out of scope.

### 4.3 PDF — the interesting one

Upstream verdict: **permanently out of scope** (issue #626, SCOPE.md) — and *no PDF rasterizer
has ever publicly run on an ESP32*. But the research turned up genuinely new numbers
(measured this session with mutool 1.27.0 under a tracing/capped allocator):

- 292-page text PDF, rendered 480×800 grayscale: **peak heap 3.9–4.1 MB** — completes under a
  hard 4 MB allocator cap (MuPDF's store scavenging works; fails at 3 MB)
- 300-dpi **JPEG scan** page: peak **5.6 MB** (libjpeg scaled-DCT decode) — fits 8 MB
- Same page **Flate-encoded** (lossless): peak **27.7 MB** with one irreducible 25 MB malloc — does **not** fit
- MuPDF low-memory mode (`-L`) is a trap: peak *rises* to 22.9 MB — use the store with a small cap instead
- Precedent: kindlepdfviewer ran MuPDF on the 32 MB-RAM Kindle 2; KOReader ran an 8 MB store for years
- Deps (freetype, zlib) have official Espressif ESP-IDF components; MuPDF vendors all thirdparty in-tree; grayscale-only plotters + format stripping via `config.h`; a 416×240 1-bit page buffer is ~12 KB

**Verdict: a MuPDF port to the M3's 8 MB PSRAM is plausible for text-based and JPEG-scan PDFs**
— it would be a first in the ESP32 world. Unknowns: Xtensa stack depth vs recursive parser,
PSRAM fragmentation under ~450k allocations/book, render time at 240 MHz (likely seconds/page —
acceptable on e-ink).

Pragmatic tiering:

1. **Now (zero firmware):** convert PDFs off-device to XTC page bitmaps — `xtcjs.app`
   (browser-local, pdf.js) or `chazeon/xtctool`. CrossPoint reads `.xtc`/`.xtch` natively.
   On a 416×240 3.7" panel, reflowed conversion beats raw PDF pages anyway.
2. **Then (the real goal):** experimental `lib/Pdf` on a fork using stripped MuPDF
   (grayscale plotters, no XPS/SVG/JS/EPUB, 4–6 MB store cap in PSRAM, banded rendering,
   pre-render page cache to SD like CrossPoint's existing `/.crosspoint/` section cache).
   Render-to-SD-cache also amortizes slow pages: first open renders in background, then reading is instant.
3. A small-screen reality check: 416×240 shows a letter-size PDF page at ~0.2× — usable for
   text PDFs via 2-column/zoom modes or MuPDF's reflow (`fz_new_stext_page` → re-typeset),
   which is effectively "PDF→text extraction" and much nicer on this panel.

### 4.4 Audio — hardware is great, software is fork-layer

Upstream is categorical: no audio, ever (SCOPE.md, discussion #222, roadmap #1680). So audio
lands in *our* build on top of freeink-sdk, which already has: ES8388 init with the
OEM-recovered register sequence, I2S output task, WAV playback, buzzer. The stock firmware
proves the full hardware path with ESP32-audioI2S 3.0.12 (MP3/AAC/FLAC/WAV/Opus/Vorbis — the
same library TBOT can reuse wholesale, it's Arduino-framework compatible with CrossPoint's build).

Plan: an "Audio player" activity in the fork (CrossPoint's ActivityManager makes this clean),
ESP32-audioI2S or Helix-MP3+dr_flac decoders feeding the SDK's I2S/ES8388 backend, files from
`/Music` on SD, headset inline-remote via GPIO11 ADC for play/pause. Audiobook mode
(resume-position per file, sleep timer) is a natural CrossPoint-style feature. Dual-core S3:
audio task on core 1, UI/render on core 0 — reading while listening is feasible.

Only open hardware question: ES8388 I2C address strap (0x10 vs 0x11) — one I2C scan with
GPIO43 high settles it.

## 5. Recommended plan of attack

**Phase 0 — safety (DONE).** Full 16 MB dump + eFuse check. Restore path verified-safe.

**Phase 1 — run what exists (a weekend).**
1. Build freeink-sdk `[env:murphy]` demo/`freeink-reader` and flash to app0 (`0x10000`) — confirms display/SD/buttons/frontlight on TBOT's exact unit (our stock build differs from the one upstream analyzed).
2. Probe-verify audio/RTC pins from the PR #2 schematic (I2C scan with power gates up: expect ES8388 @0x10/0x11, RX8010 @0x32, AHT30 @0x38, touch @0x2e).
3. Email/DM itsthisjustin (he closed #2311 with "responded over email" — he may already have the M3 app build + corogoo's real schematics). Offer TBOT's dump (different app0 SHA) to the Murphy repo.

**Phase 2 — CrossPoint app on M3.**
Fork `crosspoint-reader` (develop, v1.5.0 S3 base), point its freeink-sdk submodule at the
Murphy profile, add `[env:murphy]`, then do the 240×416 UI layout pass (the Sticky work in
v1.5.0 shows exactly which knobs exist). Wire touch (CHSC6x driver exists) + RX8010 RTC.
Upstream anything device-generic to freeink-sdk (they've merged Murphy code before).

**Phase 3 — audio fork feature.** As §4.4. Keep it a clean fork feature; upstream won't take it.

**Phase 4 — MOBI.** Calibre auto-convert immediately; optional native `lib/Mobi` later.

**Phase 5 — PDF.** XTC conversion pipeline immediately; then the MuPDF-on-S3 experiment
(§4.3) as its own standalone spike (`mupdf-esp32` PoC: render one page to BMP on-device)
before integrating. If it works, it's publishable — nobody has done it.

**Flashing safety rails (apply every time):**
- Stock backup exists and SHA-matches before any write (`SHA256SUMS`).
- Only ever write `app0`/`app1`/`spiffs` regions during development; never touch bootloader/partition table/nvs unless restoring the full image.
- Never burn eFuses. GPIO0-at-boot quirk: holding the bottom button during reset enters the ROM bootloader (this is also the manual recovery path).
- OTA slot `app1` is blank — CrossPoint dev builds can even live beside stock via otadata switching, but plain full-restore is simple enough that this is optional.

## 6. Risks & open questions

| Risk | Severity | Mitigation |
|---|---|---|
| PR #2 schematic errors (audio/RTC pins unverified) | Low | Probe-verify before relying; display/SD/buttons already cross-confirmed |
| App-side M3 code never lands publicly | Low | All SDK pieces are public; worst case we do the app integration ourselves (Phase 2 is exactly that) |
| 4-level grayscale quality on this panel | Low | B/W path proven; grayscale is a bonus |
| MuPDF port fails on stack depth / fragmentation | Medium | It's Phase 5 and a spike; XTC conversion already covers PDFs meanwhile |
| Battery capacity unknown | Cosmetic | Discharge test someday |
| Charge-status pin unidentified | Cosmetic | Probe candidates while charging |

Full source list from the research sweep: [analysis/research-sources.md](research-sources.md).

## 7. Bottom line

The M3 is arguably the **best-positioned CrossPoint target after the Xteinks themselves**: the
same maintainer who ports devices owns the M3 reverse-engineering repo, the vendor is
cooperative, the SDK already contains a complete board profile with working display/SD/
buttons/frontlight, the chip is the same S3 class CrossPoint v1.5.0 just added, and the device
is unbrickable. EPUB is essentially free once the app build exists; MOBI is a Calibre plugin
away (native parser optional); audio hardware is the best in the whole ecosystem and the SDK
already speaks ES8388; PDF has a proven conversion path today and a genuinely novel—but
measured-feasible—on-device MuPDF route if we want to be first.
