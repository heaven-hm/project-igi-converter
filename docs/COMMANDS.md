# igi1conv — Command Reference

`igi1conv` is a standalone CLI for reading, converting, and inspecting *Project IGI
1* game assets. It has no OpenGL or editor dependency.

```powershell
igi1conv --help
igi1conv --version
igi1conv <command> --help
```

**Exit codes:** `0` success · `1` bad args · `2` file not found · `3` parse error · `4` write error

## Game selection (IGI 1 / IGI 2)

```powershell
igi1conv --game=igi1 <command> ...   # default: all existing IGI 1 behaviour
igi1conv --game=igi2 <command> ...   # route to the IGI 2 backend
```

`--game igi2` is accepted as an alias. Commands not yet ported to IGI 2 are
rejected with a clear message rather than being silently handled by the IGI 1
parser. See [IGI 2 format matrix](igi2/FORMAT_MATRIX.md).

## `forest` — IGI 2 DAT Forest (`forest_*.dat`)

Requires `--game=igi2`.

```powershell
igi1conv --game=igi2 forest info   <forest_*.dat>
igi1conv --game=igi2 forest export <forest_*.dat> [-o <out.json>]
igi1conv --game=igi2 forest compile <in.json> -o <out.dat>
```

`export` writes an `igi2-forest/1` JSON document (positions, rotations, scales
and per-instance colours); `compile` rebuilds the native 14-byte-header /
40-byte-record binary. The round trip is byte-exact.

## Test corpus location (v1.9.1+)

The test suite (`igi1conv_tests.exe`) is driven against a real IGI corpus.
The corpus location is **not** hard-coded. Provide it at runtime via:

| Method | Example |
|---|---|
| `--game-path=PATH` | `igi1conv_tests.exe --game-path=D:\IGI1\igi1conv_test_suite` |
| `--corpus=PATH` (alias) | `igi1conv_tests.exe --corpus=D:\IGI1\igi1conv_test_suite` |
| `IGI_GAME_PATH` env var | `$env:IGI_GAME_PATH = "D:\IGI1\igi1conv_test_suite"` |

When none of the above is set, `CorpusDir()` returns "" and any test that
calls `IGI1CONV_NEED()` will `GTEST_SKIP()` so the suite stays green on
machines without a corpus.

---

## `tex` — Textures (TEX / SPR / PIC)

```powershell
igi1conv tex info   <input.tex|.spr|.pic>
igi1conv tex decode <input.tex|.spr|.pic> -o <output_dir>
igi1conv tex decode <folder/> -o <output_dir> --batch
igi1conv tex to-png <input> [-o <out.png>] [--resize <W> <H>]
igi1conv tex to-tga <input> [-o <out.tga>] [--resize <W> <H>]
igi1conv tex to-spr <input> [-o <out.spr]                    # PNG/TGA/BMP/JPG -> .spr (ARGB8888)
```

`to-png` / `to-tga` / `to-spr` accept `.tex .spr .pic .tga .png .bmp .jpg .jpeg` as input.
`-o` is optional (defaults to the input path with the new extension).

## `mef` — 3D Meshes

```powershell
igi1conv mef info   <input.mef>
igi1conv mef dump   <input.mef> [-o <output.txt>]
igi1conv mef export <input.mef> -o <output.obj> [--geometry-only]
igi1conv mef export <folder/> -o <output_dir> --batch [--geometry-only]
igi1conv mef export-rigged <input.mef> -o <outdir> [--rig-archetype <name>] [--skip-obj]
igi1conv mef bundle <input.mef> -o <outdir> --dat <file.dat> --texdir <dir> [--geometry-only]
igi1conv mef to-text <input.mef> -o <output.txt>
igi1conv mef compile <input.txt> -o <output.mef>
igi1conv mef build-rigid <input.mef> [-o <output.mef>]
```

### Rigged (modelType 1) exports

OBJ cannot store a skeleton, per-vertex weights or keyframes.  `mef export`
and `mef bundle` therefore **fail closed** for rigged MEFs: they refuse to
silently turn a character into a static mesh.  Pass `--geometry-only` when a
static mesh is genuinely what you want.

`mef export-rigged` is the lossless path.  It writes the geometry OBJ/MTL plus
`<outdir>/<stem>.rig.json` (format `igi1-rig-v1`) containing the restored bone
hierarchy (`name`, `parent`, `pivot`) and every vertex's `boneIndex`, `weight`
and `localVertexId`.  IGI1 binary MEFs omit the skeleton hierarchy entirely;
the converter restores it from the hardcoded skeletal archetypes
(`JonesCinematic`, `StandardSoldier`, `HeavySoldier`, `AdvancedFingerRig`) by
matching the model stem and maximum bone index, exactly like the level editor.
If no archetype can be identified, the export fails: supply one explicitly with
`--rig-archetype <name>` (or `--rig-archetype 48`).  Bone indices outside the
selected archetype, or non-finite/out-of-range weights, also fail the export.

## `qsc` — Q Script (source)

```powershell
igi1conv qsc validate <file.qsc>
igi1conv qsc compile  <file.qsc> -o <out.qvm>
```

## `qvm` — Q VM (bytecode)

```powershell
igi1conv qvm info      <file.qvm>
igi1conv qvm disasm    <file.qvm> [-o <output.txt>]
igi1conv qvm decompile <file.qvm> -o <out.qsc>
```

## `res` — RES Archives

```powershell
igi1conv res list    <input.res>
igi1conv res extract <input.res> -o <output_dir> [--file <name>]
igi1conv res compile <file.qsc>
igi1conv res pack    <dir> <out.res>
igi1conv res unpack  <file.res> <dir>
igi1conv res append  <input.res> <file1> [file2 ...] -o <out.res> [--prefix LOCAL:textures/]
```

## `mtp` — Model-Texture Package (binary FORM/IFF)

```powershell
igi1conv mtp info   <input.mtp>
igi1conv mtp dump   <input.mtp> [-o <output.json>]
igi1conv mtp to-dat <input.mtp> [-o <out.dat>]
igi1conv mtp repair <input.mtp>                 # sync VNAM/GTT counts
igi1conv mtp sync   <input.mtp> <input.dat>     # add DAT models missing from MTP
```

## `dat` — Model-Texture Data (text)

```powershell
igi1conv dat info   <file.dat>
igi1conv dat export <file.dat> [-o <out.json>] [--filter <model>] [--text]
igi1conv dat to-mtp <file.dat> [-o <out.mtp>]
```

`mtp` and `dat` describe the same model→texture mapping in two formats; `mtp to-dat`
and `dat to-mtp` convert between them.

## `iff` — Skeletal Animations (binary IFF)

The IFF format is Project IGI 1's binary animation container: a
skeleton (bones, parents, rest translations) plus any number of
clips, each with a root translation track, a per-bone rotation track
and a list of trigger events.  igi1conv reads and writes IFF entirely
in native C++ - no Python, no 3DS Max plugin.

```powershell
# Inspect / validate
igi1conv iff info  <file.iff>                 # human-readable structure dump
igi1conv iff test  <file.iff>                 # parse + skeleton evaluation

# IFF -> text + per-anim IFFs (decompile)
igi1conv iff decompile <file.iff> <out_dir>
# writes <out_dir>/<basename>.IFF            (skeleton + anim list)
# writes <out_dir>/anims_<id>/anim_NNN.IFF   (per-clip text)

# IFF -> .BEF text scripts (one .BEF per animation)
igi1conv iff convert <file.iff>     <out_dir>
igi1conv iff convert <folder/>       <out_dir>     # batch

# .BEF -> IFF (or decompile text -> IFF)
igi1conv iff create   <dir_with_befs> <out.iff>    # accepts both layouts

# One-shot IFF -> BEF -> IFF round trip (uses a temp dir)
igi1conv iff rebuild  <src.iff> <out.iff>

# Generate Anims.qsc for a folder of .BEF scripts
igi1conv iff emit-qsc <dir_with_befs> <out.qsc>

# Render the animation to a looping animated GIF (no OpenGL needed)
igi1conv iff export-gif <file.iff> <out.gif> [width] [height] [fps]
# default size 640x480, default 30 fps
```

The `create` command auto-detects the input layout: if the directory
contains `.BEF` files (the `convert` output) it parses them; otherwise
it falls back to parsing the `decompile` text format.  This means
`decompile` and `create` are exact inverses.

The `export-gif` renderer is a pure-software orthographic projection
(no Qt, no OpenGL) and works headlessly from any terminal.

## `fnt` — Bitmap Fonts

```powershell
igi1conv fnt info   <file.fnt>
igi1conv fnt export <file.fnt> -o <out.png>
```

## `wav` — IGI Audio (ILSF → `.wav` only)

The `.wav` files inside the IGI game directory are **not** normal PCM `.wav`
files. They use a proprietary InnerLoop container (signature `"ILSF"`,
20-byte header) that wraps either 16-bit PCM samples (methods 0 and 1)
or IMA-ADPCM frames (methods 2 and 3).  `igi1conv wav` decodes **all
four** methods to standard PCM `.wav` that plays in every Windows
media player (Windows Media Player, VLC, foobar2000, ...).

```powershell
# Inspect an IGI .wav (all four methods are decoded).
igi1conv wav info        <file.wav>

# Single-file conversion.  Output is always standard PCM .wav.
igi1conv wav convert     <file.wav> -o <out.wav>
# -o may also be an existing directory (or have no extension): the
# output filename is auto-derived from the input basename as
# `<src-stem>.wav` inside the directory.

# Batch conversion - walks a tree, decodes every *.wav it finds.
igi1conv wav convert-dir <src_dir> -o <dst_dir> [--no-recursive]
```

This build is a **single binary with zero external dependencies**.
There is no MP3 support - asking for an `.mp3` output returns exit
code 1 with a clear error message and produces no file.  Standard
PCM `.wav` plays in every Windows media player; if you need MP3
elsewhere, point a tool like `ffmpeg` at the decoded `.wav`.

**Supported methods (decoded):**
- `0` RAW
- `1` RAW_RESIDENT (identical payload to RAW)
- `2` ADPCM         (4-bit IMA / Intel ADPCM, initial state `0,0`)
- `3` ADPCM_RESIDENT (identical payload to method 2)

**Output format:** only `.wav` is supported.  Exit code 1 with a
clear error if you ask for anything else.

### GUI: audio cache + playback + Save As

Right-click on any `.wav` in the file tree:
- **Play in default media player** — runs `wav convert` into a
  content-addressed cache (SHA-256 prefix + mtime) under the audio
  cache directory, then hands the URL to the OS so the user's
  default media player picks it up.  Re-playing the same file is
  instant: the GUI checks for an existing cached output first and
  skips the conversion step.
- **Convert to .wav (Windows PCM)...** — opens a standard
  `Save As` dialog.  Pick the destination (the dialog defaults
  to `<src-dir>/<src-stem>.wav`).  The file lands wherever the
  user pointed, no cache.
- **Info** — runs `wav info` in the text viewer.

The **audio cache directory** is resolved in this order (first
non-empty wins):
1. `Settings > Audio Cache Folder...` (per-setting, persisted in
   `igi1conv.ini` as `AudioCacheDir`).
2. `Settings > Cache Folder...` (the existing temp dir shared
   with textures / models, persisted as `CacheDir`).
3. `<QDir::tempPath()>/igi_audio_cache` — last-resort default,
   pre-created at startup so the very first playback works.

Use `Settings > Clear Audio Cache` to wipe the directory.

Clicking a `.wav` also auto-routes to **Audio mode** (Mode 6) in the
unified viewer toolbar, which shows the music-player transport
(play/pause/resume/stop/±5s), scrubber, time readout, and volume.
The audio bar in-process-decodes the ILSF container via MCI; the
underlying conversion also lands in the audio cache, so jumping
back and forth between Audio mode and the right-click actions
never re-decodes.

### GUI: Audio mode

The Mode combo has an `Audio` entry (index 6).  Clicking any `.wav`
file in the file tree auto-routes to Audio mode: the GUI transparently
runs `igi1conv wav convert` to produce a sibling `<name>.playback.wav`
(Windows PCM) and loads it into an in-process MCI music player with
the standard transport controls:

- `⏮` restart / `⏪` -5s / `▶ / ⏸` play-pause-resume / `⏹` stop / `⏩` +5s / `⏭` end
- scrubber (click / drag to seek) and time readout (`m.mmm / m.mmm s`)
- volume slider

Already-standard `.wav` files (anything that starts with `RIFF`) are
loaded directly without re-conversion.  MCI playback runs in the GUI
process so there is no need for Qt6::Multimedia or any external player.

### File format: ILSF `.wav` (InnerLoop Sound File)

The IGI game ships audio as a non-standard WAV dialect with the
following 20-byte header on top of either raw 16-bit PCM or
4-bit IMA/Intel ADPCM nibbles:

```text
  off  0 : 4s  signature            "ILSF"
  off  4 : H   sound_pack_method    0=RAW, 1=RAW_RESIDENT, 2=ADPCM, 3=ADPCM_RESIDENT
  off  6 : H   sample_width_bits    always 16
  off  8 : H   channels             1 or 2
  off 10 : H   unknown_04          (ignored, typically 0)
  off 12 : I   frame_rate_hz        11025 / 22050 / 44100
  off 16 : I   frame_count          decoded PCM samples per channel
  off 20 : ... audio frames         RAW: int16 LE samples (interleaved if stereo)
                                   ADPCM: 4-bit IMA nibbles (interleaved by nibble
                                          if stereo).  state (0, 0) is used as the
                                          initial decoder state.
```

The data size rule of thumb:
- mono:  `ceil(frame_count * channels / 2)` bytes of nibbles (or
  raw int16 samples for methods 0/1).
- stereo ADPCM: same number of bytes as one channel's frame
  count, with each byte holding `ch0 nibble` (high) and
  `ch1 nibble` (low) so the resulting PCM is already interleaved
  `L, R, L, R, ...`.

Methods 0 and 1 carry raw 16-bit little-endian PCM samples
(byte-for-byte identical payloads).  Methods 2 and 3 carry 4-bit
IMA / Intel ADPCM nibbles (state = `(predictor=0, step_index=0)`)
using the standard 89-entry step table and 16-entry index table.
The first 16 decoded samples are dropped in `igi1conv` because
the ILSF ADPCM payload's opening `0xF / 0xF / 0x7 / 0xF` nibble
run otherwise produces an audible click at the start.

## `terrain` — Lightmaps & Quad-Tree

```powershell
igi1conv terrain info       <file.lmp|.ctr>
igi1conv terrain export-lmp <file.lmp> -o <output.pgm>   # 16-bit PGM(s)
igi1conv terrain export-ctr <file.ctr> -o <output.json>
```

## `graph` — AI Navigation Graph

```powershell
igi1conv graph info   <file.dat>
igi1conv graph export <file.dat> -o <out.json>
```

---

For the binary layouts behind each format, see
[`game_file_formats.md`](game_file_formats.md) and
[`game_parsers.md`](game_parsers.md).
