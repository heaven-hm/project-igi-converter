# macos-gconv — native C++17 CLI

Native ARM64 macOS command-line executable: `bin/gconv`.
There is no GUI. The executable does not require Python, Mono, Wine, Windows DLLs, or an emulator. libpng is linked statically in the supplied binary; its other dependencies are macOS system libraries.

## Run

From the workbench root:

```sh
./macos-gconv/bin/gconv --help
./macos-gconv/bin/gconv res list /Users/haseeb-mir/Downloads/sprites.res
./macos-gconv/bin/gconv res extract /Users/haseeb-mir/Downloads/sprites.res /tmp/igi2-sprites
./macos-gconv/bin/gconv res pack /tmp/igi2-sprites /tmp/rebuilt-sprites.res
./macos-gconv/bin/gconv fnt extract editor.fnt editor.png
./macos-gconv/bin/gconv fnt pack editor.fnt editor.png rebuilt-editor.fnt
./macos-gconv/bin/gconv tex extract logo.pic /tmp/logo-bitmaps
./macos-gconv/bin/gconv tex pack logo.pic /tmp/logo-bitmaps rebuilt-logo.pic
./macos-gconv/bin/gconv tex make image.tga image.tex
./macos-gconv/bin/gconv qsc compile mission.qsc mission.qvm
```

Resource extraction refuses existing output files, duplicate names, path traversal and symlinks that lead outside the destination. Font and texture packing use an original template and preserve its metadata, additional font atlases, texture tables, mipmaps, tile structure and trailing data. Bitmap dimensions cannot change. Texture packing expects the numbered `.tga` files created by `tex extract`.

`tex make` creates a LOOP 11 texture with one base bitmap. It does not generate mipmaps. Use template packing to retain an existing mipmap chain; supply every extracted bitmap when packing.

## IGI 2 support and verification

- ILFF/IRES resource listing, extraction and packing.
- Native ILFF/FONT, 24-byte TEXH, 16-bit ARGB1555 and 32-bit BGRA atlases; PNG/TGA extraction and template packing.
- Uncompressed LOOP 2/7/9/11 texture decoding and template packing, including IGI 2 SPR/PIC tiles.
- PNG and raw/RLE true-color TGA input; base-level LOOP 11 TEX creation.
- Native QSC compiler with **IGI 2 LOOP 8.7 opcode order and 64-byte header**, adapted from the local IGI1 converter's C++ parser/compiler. These changes are explicit; IGI1 version 8.5 output is not used.

Checked against the supplied real IGI 2 files and the official IGI 2 demo:

| Check | Result |
| --- | --- |
| 24 original fonts, PNG and TGA round trips | 48/48 complete FNT files byte-identical |
| 137 TEX/SPR/PIC files, extract and template pack | 137/137 complete files byte-identical |
| sprites.res and both menusystem.res archives | All 47 contained payloads preserved |
| 102 decompiled LEVEL5 scripts, native QSC compilation | 102/102 QVMs byte-identical to game originals |
| TEX creation compared with original gconvapi.dll | Base-level pixels and applicable header fields match |

Machine-readable evidence: `verification/results.json`, `verification/qsc-results.json`.
`verification/check_real_files.py` invokes the native executable only; Python is used for the optional verification harness, not conversion. Fixture locations are configurable through its arguments.

## Current limits

This is a native CLI port of the operations listed above, **not complete GConv 3.5 API parity**. GConv QSC conversion recipes are not executed; `qsc compile` compiles game scripts. New SPR/PIC layout creation, animation/model conversion, VQ encoding/decoding, sound conversion, mipmap generation and the remaining GConv script functions are not implemented. Unsupported commands fail with a nonzero status. No Windows DLL is loaded as a fallback.

The earlier Python/DLL investigation files (`engine.py`, `gconv.py`, `native_formats.py`, `originals/`) are research/oracle material only and are not linked or packaged into the native CLI. Ghidra and radare2 evidence is in `research/`. Latest downloaded Ghidra MCP source used for that analysis: commit `307b461cdbbedfbafb3771333f21875b745f940e` from mrphrazer/ghidra-headless-mcp.

## Build

Requires a C++17 compiler, CMake, zlib and libpng 1.6 or newer:

```sh
cmake -S macos-gconv -B macos-gconv/build -DCMAKE_BUILD_TYPE=Release
cmake --build macos-gconv/build -j 4
cmake --install macos-gconv/build --prefix "$PWD/macos-gconv/install"
```

The macOS Homebrew ARM64 configuration selects current libpng headers explicitly to avoid Mono's obsolete headers, and statically links libpng. Building for another architecture requires libraries for that architecture; the supplied executable is ARM64.

## Attribution

`src/qsc/` is adapted from `/Users/haseeb-mir/Documents/Code/project-igi-converter/source/parsers/`, Copyright 2026 Heaven-HM, under the included Heaven-HM Community Modding License. Changes: IGI 2 opcode emission, version 8.7, extra header word and corresponding section offsets. Retained license and scope notices are in `licenses/`. libpng's license is also included there. Proprietary DLL research files and game fixtures are not part of the native release archive.
