# Project IGI 1 Game Converter — Open Issues & Unimplemented Conversions

This document tracks unimplemented features, formats, and missing reverse-engineering conversions for future development.

## Missing Conversions (Formats & Inbound Compilers/Injectors)

- [x] **WAV (Audio) - decode (RAW)**: IGI's `ILSF` `.wav` files (methods 0 RAW and 1 RAW_RESIDENT) are now decoded to standard PCM `.wav` (or `.mp3` via LAME) by `igi1conv wav convert`.  See the `wav` entry in [COMMANDS.md](COMMANDS.md).
- [x] **WAV (Audio) - decode (ADPCM)**: Methods 2 (ADPCM) and 3 (ADPCM_RESIDENT) are decoded via 4-bit IMA/Intel ADPCM (initial decoder state `predictor=0, step_index=0` - matching the `audioop.adpcm2lin(data, 2, None)` call from the Python dconv reference at `D:\IGI-Tools\GM_123\IGI MEF CONV\tools\dconv\format\wav.py`).  Stereo files are decoded with nibble-interleaved channels.  All four methods are supported.
- [x] **WAV (Audio) - single-binary / zero-deps** (1.9.6): The earlier LAME-based MP3 path (both `lame.exe` spawn and bundled `lame_enc.dll`) was removed so `igi1conv.exe` ships as a single binary with no external DLLs.  The `wav` command now outputs only standard PCM `.wav`; asking for `.mp3` exits with code 1 and a clear error.
- [ ] **WAV (Audio) - encode (WAV → IGI-ADPCM)**: Compiling standard PCM WAV files back into the game's compressed ADPCM `.wav` is still unimplemented.
- [ ] **QAS (AI Scripts)**: Decompile binary AI pathing and action script structures (`.qas`) to human-readable text and compile them back.
- [ ] **ILFF (Containers)**: Implement direct standalone CLI options to extract/pack the InnerLoop File Format (ILFF) container structure.
- [ ] **WAV encode / QAS / ILFF status**: C++ parsers for these three items are proposed in open PR #16 (`cpp-parsers`) and are **not merged here**; this repository does not claim them as shipped.
- [x] **Terrain (PGM → LMP write-back)**: `igi1conv terrain import-lmp <input.pgm> -o <output.lmp>` rebuilds a square LMP chunk from a binary P5 PGM (8- or 16-bit maxval). Round-trip against `terrain export-lmp` is byte-exact and covered by 4 corpus-free tests (`tests/test_igi1conv_terrain_import.cpp`). JSON → CTR remains unimplemented.

