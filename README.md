# Project IGI Converter

**IGI1Conv and IGI2Conv provide desktop and command-line tools for Project I.G.I. and I.G.I. 2: Covert Strike assets.** Extract resources, convert textures, inspect models and animations, edit scripts and metadata, and rebuild supported native containers.

The desktop distribution contains two separate applications built from shared C++ parsers, codecs and Qt interface code. Use **IGI1Conv** for IGI1 files and **IGI2Conv** for IGI2 files; formats with the same extension can have different layouts between games.

[Download the latest release](https://github.com/heaven-hm/project-igi-converter/releases/latest) · [Desktop source](https://github.com/heaven-hm/project-igi-converter/tree/feature/unified-igi-desktop-qt/converters) · [Validation reports](https://github.com/heaven-hm/project-igi-converter/tree/feature/unified-igi-desktop-qt/converters/docs)

## Project background

**IGI1Conv** is a research-driven converter for Project I.G.I. assets. Its implementation is based on analysis of retail files, recovered format layouts and conversion round-trip tests. It provides inspection and editing workflows for the first game rather than relying on the original game editor.

> [!NOTE]
> **Latest: v1.11.0-rc.3 (August 2026) — Game-facing MCP release candidate**:
> - **Follow-up hardening**: every MCP output-producing operation rejects an input/output collision, including an input file contained by an output directory; HTTP sessions use bounded retention with expiry, and oversized `Content-Length` values are classified as `413 Payload Too Large` even when they exceed 32-bit integer range.
> - **Windows release bundle**: the downloadable x64 ZIP includes the Qt runtime and `vc_redist.x64.exe`; install the redistributable when the host does not already provide the MSVC runtime.
> - **Validation scope**: the deployed live matrix covers all 58 operations registered by `tools/list` (58/58, 100% operation coverage), plus four CLI-only cases; this is operation coverage, not source-code line coverage.
> - **Animation mode (Mode 6)**: New GUI mode that plays IFF bone animations on textured 3D MEF models. Toggle via **Settings > Animation**. Includes a Model dropdown, Animations listbox, Play button, Loop checkbox, and a configurable **FPS input textbox (1–120)**.
> - **Skeletal skinning**: The textured 3D MEF mesh is deformed each frame using the IFF bone transforms — you see the actual animated character, not skeleton dots. Press `P` to toggle rest-pose skinning for debugging; press `B` to toggle the bone skeleton overlay (now depth-test disabled so it renders on top of the model at the correct scale).
> - **Auto-setup**: Selecting a level from **Settings > Level** auto-detects `objects.qsc`, the `common/ANIMS` folder, and the level `models/` folder, so Animation mode is one click away.
> - **Root-offset fix**: The MEF↔IFF coordinate mismatch (root at `(0,0,3990.4)` vs `(0,0,0)`) is handled, so body parts no longer detach during animation.
> - **All hardcoded paths removed** from the source tree — GUI falls back to the user's home directory, and the corpus is resolved via the `IGI_GAME_PATH` env var or `--game-path` flag.

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
| Textures, sprites and pictures | `.tex`, `.spr`, `.pic` | `.tex`, `.spr`, `.pic`, `.anm` | Decode images, including sprite frames with different dimensions, edit bitmaps and rebuild supported existing containers. Create uncompressed TEX, including optional ARGB8888 mipmaps. |
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

## MCP server — game effects only

`igi1conv` includes a Model Context Protocol server for automation against the
same supported game-data operations as the CLI. It exposes conversions,
inspection, validation, packing, lightmap work, and QSC task edits that affect
files consumed by Project IGI. It does not expose GUI preferences, themes,
cache paths, viewer/camera state, animation playback controls, layout, or any
other editor-only state.

Start the default newline-delimited stdio transport:

```bash
igi1conv mcp --transport stdio
```

Or start Streamable HTTP on localhost:

```bash
igi1conv mcp --transport http --host 127.0.0.1 --port 8765
```

HTTP validates `Origin` and serves plain HTTP on loopback only. Non-loopback
binds are refused; use an HTTPS reverse proxy in front of a loopback listener
for remote access. Use `--origin` to provide an explicit Origin allowlist.
Browser clients can send an unauthenticated `OPTIONS` preflight; allowed
Origins receive only the documented POST/OPTIONS and MCP/auth/session header
permissions. The MCP endpoint is `/mcp` by default.

The server advertises two game-facing tools: `igi_game_command` (all entries
discovered from `tools/list`, such as `tex.info`, `mef.compile`, `res.repack`,
and `qsc.compile`) and `igi_game_object_edit`. The latter
can select a QSC `Task_New` by task id, class, or object name and update game
position, rotation/gamma, model id, team, bone hierarchy, stand animation, or
an arbitrary validated direct argument for task-specific enemy, weapon, AI,
and trigger data. Named placement fields are restricted to the known
`HumanSoldier` layout; other task classes use explicit indexed updates. MCP
file writes require distinct input/output paths. Stdio frames, HTTP request
deadlines, and command output capture are bounded; the MCP lifecycle is
session-scoped; and inherently in-place CLI operations are not exposed through
MCP. OLM creation rejects PNG dimensions that cannot be represented by the
game format, and `res repack` refuses unmatched directory files before writing
an archive. HTTP sessions expire and are retained in a bounded cache. Every
MCP output-producing path, including implicit/default exporter outputs, rejects
an output path that resolves to an input path.

### MCP examples

For stdio, send one JSON-RPC object per line. Initialize first, then discover
the tools and call a registered operation. The server prints only JSON-RPC
responses on stdout, so it can be connected directly to an MCP client:

```text
{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-11-25","capabilities":{},"clientInfo":{"name":"igi-example","version":"1.0"}}}
{"jsonrpc":"2.0","id":2,"method":"tools/list","params":{}}
{"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"igi_game_command","arguments":{"command":"qsc.list-objects","args":["missions/location0/level1/objects.qsc","--json"],"working_directory":"D:/IGI1"}}}
```

The generic command tool uses the same operation names returned by
`tools/list`. For example, `tex.info` reads a texture without changing it:

```json
{
  "name": "igi_game_command",
  "arguments": {
    "command": "tex.info",
    "args": ["missions/location0/level1/textures/FLARE00.TEX"],
    "working_directory": "D:/IGI1"
  }
}
```

To edit a QSC placement, always use a different output path. Selectors can be
combined, and named placement fields use the known `HumanSoldier` layout:

```json
{
  "name": "igi_game_object_edit",
  "arguments": {
    "input_file": "missions/location0/level1/objects.qsc",
    "output_file": "D:/IGI1/tests_temp/objects-edited.qsc",
    "selector": {"class_name": "HumanSoldier", "object_name": "Guard01"},
    "position": [125.0, 42.0, 900.0],
    "rotation": 1.57,
    "model_id": "013_01_1",
    "team": 2
  }
}
```

For Streamable HTTP, retain the `Mcp-Session-Id` returned by `initialize` and
send it on every subsequent request. A minimal PowerShell session looks like:

```powershell
$body = '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-11-25","capabilities":{},"clientInfo":{"name":"igi-http-example","version":"1.0"}}}'
$init = Invoke-WebRequest http://127.0.0.1:8765/mcp -Method Post `
  -Headers @{ Accept = 'application/json, text/event-stream' } `
  -ContentType 'application/json' -Body $body
$session = $init.Headers['Mcp-Session-Id']
$call = '{"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"igi_game_command","arguments":{"command":"tex.info","args":["missions/location0/level1/textures/FLARE00.TEX"]}}}'
Invoke-RestMethod http://127.0.0.1:8765/mcp -Method Post `
  -Headers @{ Accept = 'application/json, text/event-stream'; 'Mcp-Session-Id' = $session } `
  -ContentType 'application/json' -Body $call
```

For a protected loopback listener, set the token in the environment rather
than placing it in a shared command line or source file:

```powershell
$env:IGI1CONV_MCP_TOKEN = 'use-a-local-secret'
igi1conv mcp --transport http --host 127.0.0.1 --port 8765
```

Send `Authorization: Bearer use-a-local-secret` with the HTTP requests. HTTP
request bodies are bounded; an oversized body is rejected with `413` before
dispatch. Successful tool results preserve `exit_code`, `stdout`, `stderr`,
and `output_paths`; a nonzero converter exit code sets `isError`.

For the complete MCP contract, operation registry, JSON examples, result
format, and excluded editor-only surfaces, see [docs/mcp.md](docs/mcp.md).

To assemble the Windows prerelease bundle from a deployed Release directory,
run the tracked packaging script. It creates the binary-only ZIP, versioned
standalone executables, and a checksum manifest after the final archive is
written:

```powershell
pwsh -File ./tests/package_mcp_release.ps1 `
  -SourceRoot D:\IGI1\mcp-tests `
  -OutputRoot D:\IGI1\tests_temp\release-rc2 `
  -Version 1.11.0-rc.3
```

#### 2. Exporting 3D Meshes (`.mef`)
Extract weapons, characters, or level geometry into `.obj` format.
```bash
# Dump the structural data of a mesh to a text file for inspection
igi1conv mef dump models/model1.mef -o model1_struct.txt
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

The published r4 packages passed **33/33 IGI1 checks on both platforms** and **37 IGI2 checks**, with the IGI2 animation check blocked by an invalid fixture at that time. Subsequent testing uses an independently installed, licensed GOG retail copy of IGI2. Native macOS checks now pass **38/38 for IGI2**, including a valid MINA animation, and all **1,244 discovered IGI2 animation clips** pass lossless metadata round trips.

The current development build passes **242 selected IGI1 format variants** and **1,149 selected IGI2 format variants** on both macOS and Windows through CrossOver. The Windows build also passes the extended retail QVM payload regression and all **76 IGI2 shadow and legacy FORM/TDOB menu models**. On macOS, all **1,244 discovered IGI2 animation clips** and the same 76 models pass, and the expanded IGI1 corpus round-trip passed for 35,527 of 35,533 unique assets before a targeted fix; all six affected collision models now pass on both platforms. Interactive testing has resumed: native IGI1 animation playback and its representative-format viewer sweep passed, and hidden retail-file discovery was fixed for both apps. Both native format viewer sweeps completed; IGI1 WAV and IGI2 MP3 playback advanced their transports. Windows IGI1 rendered graph/model previews, decompiled QVM source, and IFF skeleton animation. Windows IGI2 also rendered graph/model/MINA and retail QVM previews, and MP3 playback advanced. Retail QVM GUI saves passed in both apps on both platforms. Native texture painting/save and Windows texture flip/save in both apps passed with independently extracted pixel verification. A macOS Qt Cocoa accessibility crash discovered during saving was fixed with a matching rebuilt plugin; the package includes corresponding source and rebuild instructions. Remaining GUI controls and original-game acceptance are not fully certified. These results describe the development build, not the published r4 application bytes. See the [retail corpus report](converters/docs/validation-2026-10-11-retail-corpus.md) for scope and remaining checks.

Detailed evidence and limitations are recorded in the [desktop validation reports](https://github.com/heaven-hm/project-igi-converter/tree/feature/unified-igi-desktop-qt/converters/docs). Proprietary game fixtures are not included in the release packages.

Remaining work includes complete original GConv recipe/API equivalence, Dreamcast/VQ behavior, font creation/scanning, full skeleton and weight export, and unrestricted mesh topology authoring. Legacy FORM/TDOB menu models currently export collision geometry; textured render primitives are not reconstructed. Shadow meshes preserve their group and bone references but do not establish full skeleton export. Some native rebuilds require the original template and reject structural changes. The tools do not guarantee in-game acceptance of arbitrary edited assets or reproduction of a level's complete scene lighting.

## License

Heaven-HM-owned material is **source-available** under the [Heaven-HM Community Modding License](LICENSE). It permits lawful, free, noncommercial modification and mod distribution under its terms; selling covered software or mods, or charging for access, requires separate written permission. Preserve attribution and follow third-party licenses.

[License scope and prior permissions](LICENSE_SCOPE.md) document separately licensed material and rights retained for earlier versions. Game assets remain subject to their respective owners' rights.
