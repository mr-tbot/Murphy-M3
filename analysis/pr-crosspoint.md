# Murphy M3 support, small-panel UI scaling, and native MOBI / PDF / audio

This brings CrossPoint up on a second class of hardware — the **Murphy M3**
(HamGeek M3 / corogoo 墨菲), an ESP32-S3R8 e-reader with a 3.7" 240×416 UC8253
panel, capacitive touch, a frontlight, an ES8388 audio codec and 8 MB of PSRAM —
and adds the format and feature work that device makes possible.

The hardware-level work (SD, touch, waveforms, battery, charging) is a companion
PR against `freeink-sdk`. This PR is the application side.

It is deliberately reviewable in pieces. Several parts stand alone and are
useful on existing devices; the three large features at the end are independent
of each other and of the port. **Take any subset** — I'd rather land the useful
half than argue about the whole.

## Part 1 — Small-panel support (useful beyond this device)

### UI density scaling

The theme metric sets were hand-tuned for the 480×800-logical X4/X3 class. The
M3's logical canvas is 240×416 at ~130 PPI, so the same pixel values render
physically larger *and* out of proportion to a canvas with a quarter of the area.
Buttons overflowed the panel and their labels were unreadable.

`uiScale` existed in settings but was dead code. Rather than revive a runtime
scale (which would multiply every layout calculation at render time), each
theme's `constexpr` metric set is passed through `scaledMetrics()` at compile
time. Touch-relevant heights run through a floor so scaling can never produce a
sub-finger target (~28 px ≈ 5.5 mm at this PPI). One knob, `kUiDensityScale`,
currently 0.6 for this device and 1.0 everywhere else — so no existing device
changes by a single pixel.

The classic theme's home menu also now refuses to paint a row the panel cannot
show, instead of drawing half-tiles off the bottom edge, and caps the cover tile
on small panels so the full menu fits.

### A list hit-test bug worth having on every device

A list band is rarely an exact multiple of the row height, so the leftover pixels
below the last row belonged to no row, and taps there were silently discarded.
Whether a given screen had that dead strip depended on its band height happening
to divide evenly by the row height — which is why touch appeared to work in some
menus and not in others. It is worst on a short panel (28 px rows in a 354 px
band leaves an 18 px strip, over half a row), but the same dead zone exists on
the larger devices.

The last drawn row now extends to the bottom of the band, both in the shared
`MappedInputManager` hit-test and in `SettingsActivity`'s own copy of it. Taps
genuinely below the list content are still ignored.

### Reader font sizes down to 5 pt

Murphy's reader sizes are 5/6/7/8/9/10 pt (default 7), with 12–18 pt compiled
out on that device to keep the image within the app partition. 32 new cuts
(NotoSerif and NotoSans, four styles each, 5–8 pt) plus 7/8 pt UI cuts.

### A quantization fix that matters on any 1-bit panel

`fontconvert.py` reduces 4-bit coverage to 2 bits and treats coverage 1–3 as
white. On a grayscale panel that is invisible either way. On a 1-bit panel —
where *every* ink level is painted black — those pixels are not "faint grey",
they are **missing ink**: I measured ~20 % of all stroke ink in a 7 pt cut being
discarded, which reads as patchy, uneven, washed-out text with visible gaps in
thin strokes.

New `--ink-floor` option (default 4, so every existing cut is byte-identical)
sets that cutoff. Murphy's small cuts are generated with `--ink-floor 3`, which
was picked by measurement and then confirmed on the panel: of the stroke ink in a
7 pt glyph the default cut paints 79 %, floor 3 paints 85 %, and floor 2 paints
90 % and reads too thick. Glyph metrics are untouched, so layout and existing
caches stay valid.

### Grayscale capability gating

This panel has no usable grayscale (asymmetric VSH/VSL rails — see the SDK PR).
`PanelDriver::supportsGrayscale()` from the SDK is plumbed through
`HalDisplay` → `GfxRenderer`, and the four call sites that build or display gray
planes are gated on it: `EpubReaderActivity`'s text/any-grayscale decisions,
`ReaderUtils::renderAntiAliased`, `XtcReaderActivity`'s gray passes, and
`SleepActivity`. Without this, the `displayGray` default re-displays the raw
gray plane as a B/W frame and every EPUB page renders near-solid black. It is
also a page-turn speed win on such panels, since the second render is skipped.

### Frontlight

No CrossPoint device had one, so there was no support at all. Added as
Settings → Display → Frontlight (0–100 % in steps), applied live, persisted,
restored at boot, exposed through the web settings API, and guarded with
`__has_include` so builds without the SDK's `FrontlightManager` are unaffected.

## Part 2 — Native MOBI / AZW support

CrossPoint can now open `.mobi`, `.azw` and the MOBI6 part of `.azw3` directly.

**Approach.** The reader's pipeline boundary is *a well-formed XHTML file on the
SD card* — `Section` inflates a spine entry to a cache file and `expat` parses
it. So rather than build a parallel reader, a MOBI is **converted to an EPUB
once, on device**, into `/.crosspoint/mobi_<hash>/book.epub`; from then on the
existing stack handles it with TOC, pagination cache, progress, bookmarks, font
choices and everything else working unchanged.

`lib/Mobi` contains:

- **`MobiParser`** — PDB container walk, MOBI and EXTH headers, and text-record
  decompression for all three schemes: none, PalmDOC LZ77, and HUFF/CDIC
  (Huffman-compressed books, including the multi-dictionary phrase expansion).
  Handles the trailing-entry stripping described by `extraDataFlags`. DRM'd books
  and pure-KF8 `.azw3` files are rejected with a message the UI can show, rather
  than producing garbage.
- **`MobiToEpub`** — the markup tidier. MOBI is HTML-3.2-ish and routinely not
  XML-well-formed, while `expat` downstream is strict, so this is where the real
  work is: a tag whitelist with automatic closing of dangling blocks, void
  elements self-closed, named entities mapped to numeric ones, cp1252 → UTF-8
  transcoding for books that declare it, `<mbp:pagebreak>` honoured as a chapter
  split with a size-based fallback, `recindex` images extracted, and a TOC built
  from each chapter's first heading. Unknown tags are stripped but their text is
  always kept — no book loses content to an unrecognised element.
- **`lib/ZipWriter`** — a small stored-entry ZIP writer (EPUB needs a container;
  deflate buys nothing on an SD card and stored entries keep the reader's
  inflate path untouched).
- **`lib/ImageTranscode`** — a PNG encoder, plus GIF decoding. MOBI books
  frequently store illustrations as GIF, which the reader cannot decode, so GIFs
  are transcoded to PNG during conversion. The encoder emits stored deflate
  blocks because the vendored miniz has its compressor compiled out; the output
  is a valid PNG that costs SD space rather than CPU, paid once per image at
  conversion time.

Conversions carry a converter version, so a firmware upgrade regenerates them
rather than silently reusing output from an older build.

## Part 3 — Native PDF support (text reflow)

`.pdf` files open as reflowable books, converted the same way into
`/.crosspoint/pdf_<hash>/book.epub`.

`lib/Pdf` implements enough of ISO 32000-1 to get text and images out:
classic cross-reference tables *and* 1.5+ cross-reference streams (with PNG and
TIFF predictors), object streams, `FlateDecode`/`ASCIIHexDecode`/`RunLengthDecode`,
indirect `/Length` resolution, the page tree with attribute inheritance, and a
content-stream interpreter that tracks the CTM and text matrices, handles `TJ`
kerning arrays (using large negative adjustments as word breaks), and recurses
into Form XObjects — which is where many producers put all the text.

Text decoding goes through each font's `/ToUnicode` CMap when present, and
otherwise through WinAnsi/MacRoman/Standard encodings with `/Differences`
resolved against a glyph-name table; Identity-H CID fonts are handled. Paragraph
structure is reconstructed from glyph positions: vertical jumps beyond a
multiple of the font size break paragraphs, smaller ones join lines, and
line-ending hyphens are removed when joining.

Embedded images are extracted — JPEG streams pass through untouched, and raw
rasters are re-encoded as PNG — so illustrated and scanned PDFs are readable
rather than empty.

**Honest limitations**, all of which degrade a page rather than fail a document:
JPX/CCITT/LZW-filtered content is skipped, and PDFs with a fully corrupt
cross-reference table are not reconstructed. A PDF with neither extractable text
nor usable images fails with a clear "no extractable text (scanned PDF?)"
message instead of producing an empty book.

## Part 4 — Audio player

The M3 has an ES8388 codec and speaker that the stock firmware barely used.

`AudioPlayerActivity` plays MP3, WAV and FLAC — and gets AAC, M4A, Opus and
Vorbis for free from the same decoder. Decoding is `ESP32-audioI2S` (the library
the stock firmware itself used on this hardware); the SDK's `AudioManager`
handles only codec bring-up over I²C and analog volume, and its I²S path is
unused, so the two stacks don't overlap.

Decoding runs on its own pinned FreeRTOS task; every access to the decoder —
from that task and from the UI — is serialised behind one mutex, since the
library is not thread-safe. End-of-track is detected by polling rather than
callbacks, which keeps the integration stable across library versions.

The UI shows track name, position within the folder playlist, elapsed and total
time, a tap-to-seek progress bar, and prev/pause/next and volume rows. Folder
playlists auto-advance, volume persists (Settings and web API), and the device
will not auto-sleep mid-track. A new **Audio** section on the home screen opens
an audio-only browser, starting at `/Audio`, `/Audiobooks` or `/Music` when one
exists.

Gated behind `CROSSPOINT_AUDIO_PLAYER`, so it costs nothing on devices without
audio hardware.

## Correctness work

I don't have the hardware in a lab, so most of this was verified off-device
before it ever reached the panel:

- **Host harnesses** for both converters, built against the real
  `MobiParser`/`MobiToEpub`/`PdfToEpub`/`ZipWriter` sources with thin
  filesystem/logging shims, compiled with AddressSanitizer and
  UndefinedBehaviorSanitizer.
- **Real books, checked properly.** A Gutenberg MOBI converts to 35 chapters
  with valid output; an illustrated 56-image MOBI yields 33 images where *every*
  extracted image's decoded pixel dimensions match the dimensions the MOBI
  markup declares for it — which verifies the record mapping, not merely that
  bytes came out. Every generated EPUB is checked for ZIP and per-entry CRC
  integrity and re-parsed with the same strict XML parser the device uses. Real
  PDFs (19-page and 93-page) extract 37 K and 125 K characters of correct text.
- **Mutation fuzzing** of the MOBI path (hundreds of ASan iterations over
  corrupted files, treating a hang as a failure).
- **An adversarial multi-agent review** of the new code, which surfaced and
  confirmed 14 real defects — all fixed. The most serious was a parser loop that
  hung the device until the watchdog rebooted it, reachable from something as
  ordinary as a URL inside an HTML comment; the fuzzer had found the same bug
  independently. The rest were integer-overflow bounds checks (`offset + length`
  wrapping past a size check, now written as subtraction), unbounded
  file-controlled allocations, and unbounded loops over corrupt structures.

Untrusted-input hardening is deliberate throughout: these parsers read files
users download from anywhere, so bounds checks avoid wrapping arithmetic, every
loop is bounded, allocations derived from file fields are capped, and a corrupt
file fails with a message rather than crashing or hanging.

## Testing on hardware

Verified on the device, iteratively, with a human at the panel: SD access, touch
and gestures across menus and lists, UI legibility at the new density, reading at
5–10 pt, text weight, page-turn quality, battery and charging indicators,
frontlight, sleep behaviour, EPUB rendering, MOBI conversion and reading (both
plain and illustrated), PDF conversion and reading, and audio playback.

Several fixes in this PR exist *because* of that loop rather than in spite of it:
the waveform work, the touch mapping, the UI density, the reader font sizes and
the ink floor were each tuned against what the panel actually showed, and the
list hit-test bug was found from a report that touch worked "in some menus but
not others".

## Companion PR

The hardware layer this depends on is
[Free-Ink/freeink-sdk#25](https://github.com/Free-Ink/freeink-sdk/pull/25).
This branch's `freeink-sdk` submodule points at that work.

## Prebuilt firmware

A built image with checksums and flashing instructions, for anyone who owns this
device and wants to try it before the port lands:
https://github.com/mr-tbot/crosspoint-reader/releases/tag/murphy-m3-v1
