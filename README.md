# Project IGI Converter

**IGI1Conv and IGI2Conv provide desktop and command-line tools for Project I.G.I. and I.G.I. 2: Covert Strike assets.** Extract resources, convert textures, inspect models and animations, edit scripts and metadata, and rebuild supported native containers.

The desktop distribution contains two separate applications built from shared C++ parsers, codecs and Qt interface code. Use **IGI1Conv** for IGI1 files and **IGI2Conv** for IGI2 files; formats with the same extension can have different layouts between games.

[Download the latest release](https://github.com/heaven-hm/project-igi-converter/releases/latest) · [Desktop source](https://github.com/heaven-hm/project-igi-converter/tree/feature/unified-igi-desktop-qt/converters) · [Validation reports](https://github.com/heaven-hm/project-igi-converter/tree/feature/unified-igi-desktop-qt/converters/docs)

## Project background

**IGI1Conv** is a research-driven converter for Project I.G.I. assets. Its implementation is based on analysis of retail files, recovered format layouts and conversion round-trip tests. It provides inspection and editing workflows for the first game rather than relying on the original game editor.

**IGI2Conv** is a native C++ reimplementation of supported asset-conversion workflows from **GConv.exe**, the original IGI2 developer conversion tool. Work uses the original executable and `gconvapi.dll` as references, together with recovered file structures, the existing C++ GConv implementation and differential comparisons of original-tool output. The released converter runs its own native code; it does not require GConv.exe or the original Windows DLL at runtime.

This reconstruction is incremental. Supported conversions are documented and tested individually; complete equivalence with every original GConv recipe, option, callback and legacy output platform remains unfinished. Both applications now share common parsers, codecs and desktop components while retaining game-specific behavior.

## Downloads

| Application | macOS Apple Silicon | Windows x64 |
| --- | --- | --- |
| IGI1Conv | `igi1conv-macos-arm64-qt.zip` | `igi1conv-windows-x64-qt.zip` |
| IGI2Conv | `igi2conv-macos-arm64-qt.zip` | `igi2conv-windows-x64-qt.zip` |

Each ZIP includes the **GUI and CLI**, Qt frameworks or DLLs, plugins and dependency notices. Extract the complete folder before running the app; moving only the executable can break its runtime dependencies. Verify downloads against the release's `SHA256SUMS.txt`.

On macOS, open the `.app` bundle or use the accompanying launcher from Terminal. On Windows, launch `igi1conv.exe` or `igi2conv.exe`; use the same executable in a terminal for CLI commands. Current desktop packages target macOS ARM64 and Windows x64. macOS bundles are ad-hoc signed, not notarized.

## Supported formats

Support describes the operations below, rather than unrestricted authoring of every possible format variant. Rebuild operations commonly use the original asset as a template to preserve its structure and unknown data.

| Asset family | IGI1 formats | IGI2 formats | Available workflows |
| --- | --- | --- | --- |
| Resource archives | `.res` | `.res` | List, extract and repack resource names and payloads. |
| Scripts | `.qvm`, `.qsc` | `.qvm`, `.qsc` | Decompile bytecode, edit source and compile back to QVM. |
| Textures, sprites and pictures | `.tex`, `.spr`, `.pic` | `.tex`, `.spr`, `.pic` | Decode images, edit bitmaps and rebuild supported existing containers. Create uncompressed TEX, including optional ARGB8888 mipmaps. |
| Models | `.mef` | `.mef` | Inspect geometry, export OBJ/MTL and rebuild supported native metadata or text representations. IGI1 also bundles attached models and mapped textures. |
| Animation | `.iff`, `.bef` | `.iff` (ILFF/MINA) | Inspect and rebuild supported animation data; IGI1 converts between IFF and text BEF. Desktop playback supports the validated skeletal layouts. |
| Model and texture mappings | `.mtp`, `.dat` | `.mtp`, `.dat` | Convert mappings, export metadata and rebuild supported structures. |
| Navigation data | graph `.dat` | graph and graph-cover `.dat` | Inspect graphs and edit supported JSON fields. IGI2 also supports forest `.dat` records. |
| Object lightmaps | `.olm` | `.olm` | Extract image layers and rebuild edited lightmap data. |
| Terrain | `.hmp`, `.bit`, `.cmd`, `.ctr`, `.lmp` | `.thm`, `.tlm`, `.tmm` | Export structured data and rebuild supported terrain assets; selected fields are editable. |
| Fonts | `.fnt` | `.fnt` | Extract the atlas as PNG/TGA and repack it using the original font template. |
| Audio | IGI `.wav` / ILSF | IGI `.wav` / ILSF, `.mp3` | Extract PCM, rebuild supported audio templates, normalize MP3 and decode MP3 to WAV. |
| Speech synchronization | — | `.syn` | Export and rebuild synchronization data, including supported edited fields. |
| Standard images | `.png`, `.tga` intermediates | `.png`, `.tga`, `.bmp`, `.jpg` | Decode and convert images for asset editing workflows. |
| Preserved media | `.avi`, `.afp` | — | Copy files byte-for-byte; no AVI/AFP transcoding or editing is claimed. |

OBJ, MTL, JSON, PNG, TGA and text representations are editing/export intermediates. Exporting an intermediate does not imply that every external edit can be compiled back into a game-ready asset.

## Desktop workflows

- Browse game folders and use file context menus to inspect, export or convert assets.
- View MEF/OBJ models with rotation, zoom, wireframe and available texture mappings.
- Preview and edit textures with painting, clipboard, flip and undo tools; save supported native containers.
- View navigation graphs, script source and hexadecimal data.
- Play supported animations and audio, with transport, seeking and volume controls.
- Set editor text size through **Settings → Editor Font Size…**; preferences are saved independently for each app.

Select the active level and texture/model folders in **Settings** before applying mapped textures. IGI1 animation selection uses `objects.qsc`, the ANIMS folder and the configured models folder, including nested extracted directories.

<details>
<summary>Interface screenshots</summary>

These screenshots illustrate the desktop interface; individual controls vary by game and release.

![Desktop workspace](assets/01_main_view.png)
![Model wireframe](assets/04c_3d_wireframe.png)
![Texture editor](assets/05a_tex_paint.png)

</details>

## Command-line examples

Run `igi1conv --help` or `igi2conv --help` for the commands implemented by that executable. The examples use IGI2; shared operations are also available in IGI1Conv where appropriate.

```sh
# Extract a resource archive.
igi2conv res extract level5.res extracted-assets

# Decompile and rebuild a script.
igi2conv qvm decompile objects.qvm -o objects.qsc
igi2conv qsc compile objects.qsc rebuilt.qvm

# Extract texture bitmaps and rebuild the original container.
igi2conv tex extract watertex.tex watertex-bitmaps
igi2conv tex pack watertex.tex watertex-bitmaps rebuilt-watertex.tex

# Create an uncompressed texture with mipmaps from a power-of-two image.
igi2conv tex make input.tga output.tex --mipmaps

# Export native metadata, then rebuild from an edited JSON document.
igi2conv asset to-json graph4.dat graph4.json
igi2conv asset from-json graph4.json rebuilt-graph4.dat

# Export model geometry and decode audio.
igi2conv mef export model.mef -o model.obj
igi2conv mp3 to-wav music.mp3 music.wav
```

IGI1 textured model bundle:

```sh
igi1conv mef bundle model.mef -o exported-models \
  --dat LEVEL1.DAT --texdir TEXTURES
```

QVM decompilation produces **comment-free QSC**. IGI2 saves the original QVM version in a neighboring `.qsc.qvm-version` file; keep that file for CLI round trips. Alternatively, select a version explicitly:

```sh
igi2conv qsc compile objects.qsc rebuilt.qvm --qvm-version 8.6
```

IGI1 uses QVM 8.5; IGI2 supports the validated 8.6 and 8.7 layouts. GUI saves preserve the opened QVM's version. Work on copies of assets when preparing changes for a game installation.

## Building from source

The shared desktop implementation is on [`feature/unified-igi-desktop-qt`](https://github.com/heaven-hm/project-igi-converter/tree/feature/unified-igi-desktop-qt), tracked in [PR #21](https://github.com/heaven-hm/project-igi-converter/pull/21). Use that branch for the two-application build:

```sh
git clone --branch feature/unified-igi-desktop-qt \
  https://github.com/heaven-hm/project-igi-converter.git
cd project-igi-converter
cmake -S converters -B build-unified \
  -DCMAKE_BUILD_TYPE=Release -DIGICONV_BUILD_GUI=ON
cmake --build build-unified --parallel
./build-unified/bin/igi1conv --help
./build-unified/bin/igi2conv --help
```

The GUI build requires Qt 6 Widgets, OpenGL and Multimedia, together with a C++17 toolchain and CMake. Set `-DIGICONV_BUILD_GUI=OFF` for a CLI-only build. The scripts in [`converters/packaging`](https://github.com/heaven-hm/project-igi-converter/tree/feature/unified-igi-desktop-qt/converters/packaging) deploy the desktop runtimes. The repository root retains the earlier IGI1 build configuration.

## Diagnostics

Verbose GUI logging is enabled by default on first launch or upgrade; subsequent user log preferences are respected. The startup message identifies the GUI log path.

| Platform | GUI log | CLI log |
| --- | --- | --- |
| macOS | `~/Library/Preferences/igiNconv/igiNconv.log` | `~/Library/Logs/igiNconv/igiNconv-cli.log` |
| Windows | `%LOCALAPPDATA%/igiNconv/igiNconv.log` | `%LOCALAPPDATA%/igiNconv/logs/igiNconv-cli.log` |

Replace `N` with `1` or `2`. Set `IGICONV_LOG_DIR` to choose another CLI log folder, or `IGICONV_LOG_ENABLED=0` to disable CLI file logging. Normal terminal output remains available.

## Validation and current limits

Published Qt packages have been checked on **native macOS ARM64** and **Windows x64 through CrossOver on macOS**. Native Windows-host testing and Intel macOS desktop packages are not claimed. Format checks exercise representative extraction, conversion, rebuild, edited-value persistence and input-preservation paths; they do not cover every asset or every layout variant.

Fresh extracted-release CLI checks pass **33/33 for IGI1Conv on both platforms**. IGI2Conv passes **37 checks on each platform**; its remaining animation check needs a valid independent IGI2 ILFF/MINA fixture. Full end-to-end certification is therefore incomplete.

Detailed evidence and limitations are recorded in the [desktop validation reports](https://github.com/heaven-hm/project-igi-converter/tree/feature/unified-igi-desktop-qt/converters/docs). Proprietary game fixtures are not included in the release packages.

Remaining work includes complete original GConv recipe/API equivalence, Dreamcast/VQ behavior, font creation/scanning, full skeleton and weight export, and unrestricted mesh topology authoring. Some native rebuilds require the original template and reject structural changes. The tools do not guarantee in-game acceptance of arbitrary edited assets or reproduction of a level's complete scene lighting.

## License

Heaven-HM-owned material is **source-available** under the [Heaven-HM Community Modding License](LICENSE). It permits lawful, free, noncommercial modification and mod distribution under its terms; selling covered software or mods, or charging for access, requires separate written permission. Preserve attribution and follow third-party licenses.

[License scope and prior permissions](LICENSE_SCOPE.md) document separately licensed material and rights retained for earlier versions. Game assets remain subject to their respective owners' rights.
