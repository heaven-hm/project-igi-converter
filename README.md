# Project IGI native converters

Two C++17 command-line converters live in this **one repository**, on separate branches:

| Game | Branch | Native executable |
| --- | --- | --- |
| Project IGI (IGI 1) | `feature/igi1conv-cross-platform` | `igi1conv` / `igi1conv.exe` |
| IGI 2: Covert Strike | `feature/igi2conv-cross-platform` | `igi2conv` / `igi2conv.exe` |

The combined release contains both converters for Windows x86_64 and macOS. macOS packages include a plain CLI and a `.app` bundle containing that CLI. Run it from Terminal; there is no graphical interface. The native IGI 2 implementation does not require `gconv.exe`, `gconvapi.dll`, Wine or Rosetta.

## Build

Use CMake 3.16+, a C++17 compiler and Python 3.9+ for verification. Image codecs and the pinned nlohmann/json 3.11.3 header are vendored; the CLI has no runtime dependency on libpng or Qt.

IGI 1 branch:

```sh
cmake -S . -B build -DIGI1CONV_BUILD_GUI=OFF -DIGI1CONV_BUILD_TESTS=OFF
cmake --build build --config Release --parallel
./build/bin/igi1conv --help
```

IGI 2 branch:

```sh
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build --config Release --parallel
./build/igi2conv --help
ctest --test-dir build --output-on-failure
```

On Windows use the corresponding `Release/igi2conv.exe` or `bin/Release/igi1conv.exe` path with a Visual Studio multi-configuration generator. CLI releases use a static C++ runtime.

## Conversion commands

Use the executable for the game that supplied the input. Examples below use IGI 2; the shared asset commands also work with `igi1conv`.

```sh
igi2conv qvm decompile mission.qvm -o mission.qsc
igi2conv qsc compile mission.qsc mission.qvm
igi2conv res extract level.res extracted
igi2conv res pack extracted rebuilt.res
igi2conv tex extract texture.tex bitmaps
igi2conv tex pack texture.tex bitmaps rebuilt.tex
igi2conv tex make image.png new.tex
igi2conv fnt extract font.fnt font.png
igi2conv fnt pack font.fnt font.png rebuilt.fnt
igi2conv image convert image.bmp image.png
igi2conv wav extract sound.wav pcm.wav
igi2conv wav pack sound.wav pcm.wav rebuilt.wav
igi2conv mp3 normalize speech.mp3 normalized.mp3
igi2conv mef export model.mef -o model.obj
igi2conv mef to-text model.mef -o model.json
igi2conv mef compile model.json -o rebuilt.mef
igi2conv asset to-json model.mef model.json
igi2conv asset from-json model.json rebuilt.mef
igi2conv olm extract object.olm layers
igi2conv olm pack object.olm layers rebuilt.olm
igi2conv asset to-json heightmap.thm heightmap.json
igi2conv asset from-json heightmap.json rebuilt.thm
igi2conv terrain extract lightmap.tlm lightmap.png
igi2conv terrain pack lightmap.tlm lightmap.png rebuilt.tlm
igi2conv mtp to-dat level.mtp -o level.dat
igi2conv dat to-mtp level.dat -o rebuilt.mtp
```

IGI 2 `mef to-text` and `mef compile` use its native JSON template representation. Rigid model authoring and VQ texture codecs are currently unavailable.

IGI 1 QSC compilation uses `igi1conv qsc compile input.qsc -o output.qvm`. Its IFF animation text path is `igi1conv iff convert animation.iff befs`, then `igi1conv iff create befs rebuilt.iff`. The separate JSON animation path preserves the original binary layout exactly.

## Format coverage

| Format | IGI 1 | IGI 2 | Conversion / fidelity |
| --- | --- | --- | --- |
| QVM / QSC | LOOP 8.5, 60-byte header | Demo 8.6 and retail 8.7, 64-byte header | Decompile / compile. IGI 2 version comment preserves the original opcode dialect. |
| RES | IRES, including aligned COMMON archives | IRES | Extract / pack. Resource names and payloads preserved; container layout may change. |
| TEX / SPR / PIC | Supported native image containers | Supported native image containers | Extract / template pack / create TEX. Template headers and untouched pixel payloads retained. |
| FNT | ILFF font atlases | ILFF font atlases | PNG/TGA export / template pack, preserving glyph metadata. |
| PNG / TGA / BMP / JPEG | Image codecs | Image codecs | Native image conversion; JPEG encoding is lossy. |
| MEF / MEX | Existing mesh tools and ILFF JSON | Static, skeletal and lightmapped render mesh export; ILFF JSON | OBJ geometry export. JSON preserves all chunks and permits fixed-size payload edits. |
| IFF / BEF | FORM animation / BEF / JSON | ILFF/MINA animation / JSON | IGI 1 BEF preserves decoded animation content within float tolerance; JSON preserves exact bytes. IGI 2 exposes header/skeleton fields and raw keyframe chunks. |
| WAV / ILSF | PCM and IMA ADPCM | PCM and IMA ADPCM | Standard PCM16 export / template pack. ADPCM re-encoding is lossy. |
| MTP / DAT mapping | Model / texture mappings | Model / texture mappings | MTP↔DAT, JSON export. |
| DAT navigation graph | IGI 1 adjacency header and TLV records | IGI 2 header and TLV records | Editable typed JSON / exact rebuild. |
| OLM | 16-byte layer descriptors | 24-byte layer descriptors and intervening metadata | All layers to images / template pack; editable RGBA JSON. |
| HMP / BIT / CMD / CTR / LMP | Game-specific terrain formats | — | Editable JSON / exact rebuild. |
| THM / TLM / TMM | — | Height floats / RGBA lightmap / material indices | Editable JSON / exact rebuild. TLM/TMM image template pack; THM image export is a normalized preview. |
| SYN | — | Float32 lip-sync samples | Editable JSON / exact rebuild. |
| DAT forest / graphcover | — | Forest transforms/colors / AICC cover data | Editable JSON / exact rebuild. |
| MP3 | — | MPEG audio frames | Frame-preserving metadata normalization; no PCM↔MP3 transcoder. |
| AVI / AFP | Existing video / opaque game key | — | Byte-preserving copy; AVI transcoding and AFP decryption are not implemented. |

`asset to-json INPUT OUTPUT` / `asset from-json INPUT OUTPUT` handles terrain, navigation graphs, OLM, animation and model containers. Keep the JSON `game`, `format`, headers and structural counts consistent. Template commands preserve dimensions and fixed chunk sizes; they are not a general-purpose model authoring pipeline.

## Verification

[Verification documentation](docs/verification.md) records the scope and reproducible commands. The representative harness requires **one real, nonempty sample per format**, plus distinct script/model variants. Missing files fail explicitly. It checks binary or decoded-content equality and edited-value persistence, not merely successful exit codes.

```sh
python verification/check_format_coverage.py --binary PATH_TO_CLI \
  --game 2 --samples PATH_TO_IGI2_FIXTURES --report format-report.json
```

The official IGI 2 single-player demo supplies animation and other missing fixtures. LEVEL5 and the provided sprite/font/texture archives supply retail examples. The full game is unnecessary for the current format matrix. Proprietary game executables, PDBs, installer archives and test fixtures are excluded from converter source and deliverable packages.

A passing representative test does not establish compatibility with every asset ever shipped, every original GConv QScript recipe, or in-game engine behavior. Unsupported operations fail rather than being reported as converted.

## License

See [LICENSE_SCOPE.md](LICENSE_SCOPE.md). Preserve prior contributor notices and the separate licenses for vendored dependencies. The tools do not distribute the games.
