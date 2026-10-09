# Native IGI 2 converter source

The former `gconv` CLI is now **`igi2conv` 0.2.0**, a native C++17 Windows/macOS command-line tool. This directory holds its native format modules. The repository root is the canonical build entry point:

```sh
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build --config Release --parallel
./build/igi2conv --help
ctest --test-dir build --output-on-failure
```

The old `cmake -S macos-gconv` entry point forwards to the same complete target; its executable is under `build/native/`. No libpng, Wine or Windows DLL is needed. PNG/BMP/JPEG codecs use vendored stb; TGA is handled natively.

See the [current repository README](../README.md) for commands, game-specific headers, coverage and limitations, and [verification documentation](../docs/verification.md) for the representative test matrix. Current evidence is under `../verification/evidence/`; older reports here are supplemental research history.

## Modules

- `common`, `asset_json`: checked binary access, file alias protection, typed JSON.
- `resources`, `fonts`, `textures`, `images`: archive and bitmap conversion.
- `qsc`, `qsc/`: IGI 2 demo 8.6 and retail 8.7 compiler dialects.
- `metadata_data`, `graph_data`, `terrain_data`, `form_data`: game-specific asset templates.
- `audio_data`: ILSF/PCM16/IMA ADPCM and MP3 frame normalization.
- `asset_dispatch`, `extended`: native routes and selected legacy parser commands.

IGI 2 model rebuilding uses native JSON templates; inherited IGI 1 model authoring is rejected. The `.clang-format` uses four spaces, Allman braces and a 100-column limit. The CLI `.app` bundle has no graphical interface.

Earlier Python/DLL investigation material is not part of the native executable or release package. Current Ghidra MCP and radare2 evidence is under [docs/research](../docs/research/README.md). This is not complete original GConv recipe/API parity; unsupported codecs and operations are listed in the root README.
