# Project IGI Converter

**Project IGI Converter** provides desktop GUI and command-line tools for inspecting, extracting, converting, and editing assets from **Project I.G.I.** and **Project I.G.I. 2: Covert Strike**.

- **IGI1Conv** — tools for Project IGI (2000).
- **IGI2Conv** — tools for IGI 2: Covert Strike.
- **[Download the latest release](https://github.com/heaven-hm/project-igi-converter/releases/latest)**

The two games use some similarly named formats with different internal layouts. Choose the converter for the game the asset comes from.

## What it can do

- Browse game files and inspect assets in the desktop interface.
- Preview and edit textures; export supported images and rebuild native texture files.
- View 3D models, inspect wireframes, and export supported geometry to OBJ/MTL.
- Decompile QVM scripts to readable QSC source and compile supported edits back to QVM.
- Inspect animation, navigation graphs, terrain, lightmaps, fonts, mappings, and audio.
- Extract and repack resource archives, and export supported structured data to JSON or text for editing.

## Supported file formats

| Format | Used for | What you can do |
| --- | --- | --- |
| `.res` | Resource archives | List contents, extract files, and repack supported resources. |
| `.qvm`, `.qsc` | Game scripts and bytecode | Decompile scripts, edit QSC, and compile supported changes to QVM. |
| `.tex`, `.spr`, `.pic`, `.anm` | Textures, sprites, pictures, and IGI2 animation/image data | Preview or export images, edit supported bitmaps, and rebuild supported containers. IGI1 supports TEX, SPR, and PIC. |
| `.mef` | 3D models | Inspect model geometry, view wireframes, export OBJ/MTL, and use supported model rebuild workflows. |
| `.iff`, `.bef` | Animation | Inspect supported animation data; IGI1 also converts between IFF and BEF text. IGI2 supports IFF animation layouts including ILFF/MINA. |
| `.mtp`, `.dat` | Model/texture mappings and game data | Inspect mappings, export supported data, and rebuild supported structures. DAT use depends on the data type. |
| Graph `.dat` | Navigation and graph data | View navigation graphs and edit supported graph fields; IGI2 also supports graph-cover and forest data. |
| `.olm` | Object lightmaps | Extract image layers and rebuild supported lightmap data. |
| `.hmp`, `.bit`, `.cmd`, `.ctr`, `.lmp` | IGI1 terrain | Export structured terrain data and rebuild supported assets. |
| `.thm`, `.tlm`, `.tmm` | IGI2 terrain | Inspect, export, and rebuild supported terrain data. |
| `.fnt` | Game fonts | Export font atlases as PNG/TGA and repack using the original font template. |
| `.wav`, ILSF | Game audio | Extract audio data and rebuild supported audio templates. |
| `.mp3` | IGI2 audio | Decode MP3 to WAV and perform supported audio conversion. |
| `.syn` | IGI2 speech synchronization | Export and rebuild supported synchronization data. |
| `.png`, `.tga`, `.bmp`, `.jpg` | Image interchange | Use supported image types for preview, conversion, and asset-editing workflows. |
| `.avi`, `.afp` | IGI1 preserved media | Copy files unchanged; video transcoding and editing are not supported. |

**Note:** Support varies by format and game. Some rebuild operations use an original file as a template to preserve data the converter does not edit. OBJ, MTL, JSON, PNG, TGA, and text files are commonly used as export or editing intermediates.

## Screenshots

### Main interface and model search

![Main View](assets/01_main_view.png)

![Model Search](assets/02_model_search.png)

### Texture viewing and editing

![Texture Renderer](assets/03a_tex_000_01.png)

![High-Resolution Texture Inspection](assets/03b_tex_000_04.png)

![Texture Context Menu](assets/03c_tex_context_menu.png)

<p align="center">
  <img src="assets/05a_tex_paint.png" width="32%" alt="Texture paint tools" />
  <img src="assets/05aa_tex_paint.png" width="32%" alt="Texture paint canvas" />
  <img src="assets/05b_tex_paint.png" width="32%" alt="Texture paint brush" />
</p>
<p align="center">
  <img src="assets/05c_tex_paint.png" width="49%" alt="Editing a texture asset" />
  <img src="assets/05d_tex_paint.png" width="49%" alt="Saving a modified texture asset" />
</p>

### 3D model inspection

![Raw 3D Mesh Viewer](assets/04a_mef_3d_raw.png)

![3D Model Zoom Inspection](assets/04_mef_003_zoomed.png)

![3D Wireframe Rendering](assets/04c_3d_wireframe.png)

![Mesh Context Menu](assets/04c_mef_context_menu.png)

### Script and binary viewers

![Integrated QSC Text Editor](assets/07a_qvm_text_view.png)

![Hexadecimal Inspector](assets/07a_qvm_hex_view.png)

### Themes and application information

<p align="center">
  <img src="assets/08c_theme_dark.png" width="32%" alt="Dark theme" />
  <img src="assets/08c_theme_military.png" width="32%" alt="Military theme" />
  <img src="assets/08c_theme_solarized.png" width="32%" alt="Solarized theme" />
</p>

![About Dialog](assets/09b_about_dialog.png)

## License

Heaven-HM-owned material is available under the [Heaven-HM Community Modding License](LICENSE). Review the license for noncommercial use, modification, redistribution, attribution, and permission requirements. Game assets remain subject to their respective owners' rights.
