# Conversion matrix and issue status (issues #8, #12, #13, #22)

This document records **what is actually present in this tree**, what is only
proposed elsewhere, and the exact blocker for every unimplemented item. It is
intentionally written so that no unimplemented conversion can be mistaken for
a passing one. "Shipped" means code in this branch plus an executed test.

## Issue #8 — consolidated missing conversions

| # | Conversion | Direction | Status here | Evidence / exact blocker |
|---|---|---|---|---|
| 1 | WAV decode (RAW / RAW_RESIDENT / ADPCM / ADPCM_RESIDENT) | IGI WAV → PCM WAV | **Shipped (pre-existing)** | `igi1conv/cmd_wav.cpp`, `tests/test_igi1conv_wav.cpp`; all four methods decoded, no external DLL. |
| 2 | WAV encode | PCM WAV → IGI ADPCM | **Not shipped** | Open PR #16 (`cpp-parsers`) adds `wav_encoder.cpp/.h` but it is not merged into this tree. No encoder is reachable from `igi1conv`. |
| 3 | QAS decompile / compile | binary `.qas` ↔ text | **Not shipped** | Open PR #16 adds `qas_parser.cpp/.h`, not merged. No CLI entry point, no fixtures. |
| 4 | ILFF extract / pack | ILFF container ↔ chunks | **Not shipped** | Open PR #16 adds `ilff_tool.cpp/.h`, not merged. Standalone extract/pack CLI options do not exist here. |
| 5 | 3D mesh write-back | `.obj` → `.mef` | **Not shipped** | `mef_exporter.cpp` (MEF→OBJ) and `mef_compiler.cpp` (text MEF→binary) exist, but there is no OBJ face/UV/material → MEF topology mapper. Blocker: no documented mapping from OBJ polygons to MEF face chunks, and no round-trip fixture. |
| 6 | Font write-back | PNG → `.fnt` | **Not shipped** | `fnt_parser.cpp` is read-only. Blocker: font atlas packing order and glyph→codepoint mapping are not specified in this repository; no writer exists. |
| 7 | Lightmap write-back | PGM → `.lmp` | **Shipped in this change** | `igi1conv terrain import-lmp`; 4 corpus-free tests in `tests/test_igi1conv_terrain_import.cpp` pass on macOS (118/118 total). Layout: `[uint32 LE size][size² uint8 luma]`, rows bottom-up, one chunk per PGM. Supports 8-bit and 16-bit P5 maxval; rejects non-square/truncated input. |
| 8 | Terrain cube write-back | JSON → `.ctr` | **Not shipped** | `ctr_item_s` is a 32-byte struct (`children_[8]`, `cmd_transform_[8]`, `children_mask_`, `cmd_offset_`), but the semantics of `cmd_offset_` and the child ordering are not documented, and there are no CTR fixtures. Export-only (`terrain export-ctr`) remains. |
| 9 | AI navigation write-back | JSON → `graph*.dat` | **Not shipped** | `graph_parser.cpp` is read-only. Blocker: binary graph record layout is not specified in `docs/game_file_formats.md`; strict round-trip tests cannot be authored without a real graph fixture or a ratified layout. |

### Row 7 supported subset (precise)

* Input: Netpbm **binary PGM (P5)**, magic `P5`, square dimensions, `maxval` 1–65535.
  Comments between header tokens are accepted; exactly one whitespace byte
  separates the header from the raster.
* Output: one LMP chunk. Multi-chunk LMP files are exported as `name_0.pgm`,
  `name_1.pgm`, …; each PGM must be imported back separately and the chunks
  concatenated externally.
* Row order is mirrored on import, which makes
  `export-lmp → import-lmp` byte-identical. 8-bit PGM samples are normalised
  to `[0,255]`.
* Out of scope: PGM→CTR, multi-chunk auto-detection, any lossy resampling.

## Issue #12 — IGI1 ↔ IGI2 cross-game converter

There is **no** `cross_convert/`, `game_selector.py`, `--src-game`/`--dst-game`
flag, conversion registry, IGI2 fixture set, or `tests/test_cross_convert.py`
in this tree. None of the issue's eight acceptance criteria are met; this
change does not claim any of them.

| Source → target | Type | Status | Blocker |
|---|---|---|---|
| `.hmp` → `.thm` | heightmap upgrade | Not implemented | IGI2 `.thm` binary layout not defined in this repo; no IGI2 fixture. |
| `.lmp` → `.tlm` | lightmap upgrade | Partial primitive only | PGM→LMP write-back now exists, but no `.tlm` writer/spec. |
| `.ctr` → `.tmm` | material upgrade | Not implemented | CTR write semantics undocumented (see #8 row 8); `.tmm` layout absent. |
| `.thm`/`.tlm`/`.tmm` → IGI1 | downgrade | Not implemented | Same missing IGI2 layouts; "best effort with warnings" needs a data-loss contract. |
| `.tex` v2 → v7/9 | version adapter | Not implemented | `tex_parser.cpp` reads known versions; no version-upgrade transform. |
| `.mef` → `.mef` | semantic adapter | Not implemented | IGI2-only render chunk set not enumerated in code. |
| `.qvm` both ways | opcode validation | Not implemented | PR #16 QAS work is not merged; no IGI2 opcode table. |
| `dat_graph` → `dat_graph` + `dat_graphcover` | cover split | Not implemented | `dat_graphcover` layout absent. |

**Why no synthetic "strict" tests were added:** the issue explicitly requires
strict tests with real binary round-trips and *no logic mocks*. Without an IGI2
fixture corpus or a signed-off binary layout for the IGI2-only formats,
inventing fixtures would produce tests that pass while proving nothing. The
minimum unblock is (a) a small licensed IGI2 fixture set per pair and (b) a
layout spec for `.thm`, `.tlm`, `.tmm`, `dat_graphcover`, `.syn`, `.olm`. Game
auto-detection by extension alone is already possible (`igi1conv` dispatches
on extension), but magic-byte detection needs verified signatures.

## Issue #13 — Blender BEF plugin

A minimal addon is delivered under `blender/bef_io/` (import + export of
armatures and animation clips). Its pure-Python BEF format layer is tested and
passes (`python3 blender/tests/test_bef_format.py`, 2 tests). The `bpy` path
has a ready-to-run headless script (`blender/tests/blender_roundtrip.py`) but
**was not executed**: `/opt/homebrew/bin/blender` is a shim whose target
`/Applications/Blender.app/Contents/MacOS/Blender` does not exist. That missing
Blender executable is the exact blocker for claiming the addon works in
Blender; no such claim is made. See `blender/README.md` for the supported
subset and limitations. The linked `3DSExporter.zip` is a 3ds Max 4/5 user
attachment, is not in this repository, and was not used as a reference.

## Issue #22 — Linux/macOS build and CI

| Platform | Command | Result |
|---|---|---|
| macOS (AppleClang 17, GUI off) | `cmake -S . -B build-cli -G Ninja -DIGI1CONV_BUILD_GUI=OFF && cmake --build build-cli && ctest --test-dir build-cli --output-on-failure` | Green; 118 tests after this change (114 before), 0 failures. |
| macOS converters CLI | `cmake -S converters -B build-conv -G Ninja -DIGICONV_BUILD_GUI=OFF && cmake --build build-conv` | Green; `igi1conv` and `igi2conv` both built. |
| Linux (GCC 15, GUI off) | same commands in `ubuntu:26.04` container | Green; see comment evidence on issue #22 for live log counts. |
| Windows Qt GUI | `.github/workflows/build-and-test.yml`, `windows-qt` job | CI job defined; not locally executed on Windows. |

Portability fixes in this change:

1. Qt is found via `CMAKE_PREFIX_PATH`; the legacy `D:/Qt/...` and
   `5.15.2/msvc2019` paths are appended only when they exist.
2. GCC/Clang force-include `igi1conv/pch.h`, matching MSVC `/FI`.
3. Tests link `Threads::Threads` on non-Windows.
4. `Mem_Alloc()` rounds the allocation size up to the 16-byte alignment
   required by C11 `aligned_alloc()` (macOS/Linux `aligned_alloc` returns
   `nullptr` for non-multiple sizes; Windows `_aligned_malloc` does not).
   This was found and reproduced by the new terrain test on macOS.
