# Retail corpus validation — 2026-10-11

This follow-up uses locally owned IGI1 files, existing IGI2 samples and a licensed
GOG retail IGI2 installation. The installer was downloaded using the user's
existing signed-in Chrome window and installed through CrossOver. Proprietary
assets, installer files, detailed path inventories and raw reports remain local.

## Scope and evidence

The inventory includes loose assets and independently decoded RES payloads. It
contains approximately 83,000 distinct asset hashes across both games; inventory
size does **not** mean all assets have been converted or inspected in the GUI.
Format representatives are grouped by observed container tags, versions, texture
modes and dimensions, model chunk sets and types, animation skeleton flags, audio
codecs, and lightmap layer layouts. Other formats use a representative rather
than exhaustive layout classification. Installer uninstallation data and a
standard WAV inside a derived OpenIGI app are excluded from the game corpus.

| Check | Native macOS ARM64 | Windows x64 through CrossOver |
| --- | --- | --- |
| Selected IGI1 corpus variants | 242 passed | 242 passed |
| Selected IGI2 corpus variants | 1,149 passed | 1,149 passed |
| Discovered IGI2 MINA animation files | 1,244 passed | Not exhaustively parsed |
| Selected distinct IGI2 MINA container variants | 22/22 parsed | 22/22 parsed |
| All discovered IGI2 shadow and FORM/TDOB models | 76 passed | 76 passed |
| Fresh Qt package IGI1 CLI matrix | 33/33 passed | 33/33 passed |
| Fresh Qt package IGI2 CLI matrix | 38/38 passed | 38/38 passed |
| Retail extended-payload QVM edit/regression | 4 checks passed | 4 checks passed |
| IGI1 collision models with secondary XTVC/ECFC pairs | 6/6 passed | 6/6 passed |

Selected layout/format variant counts by extension are recorded below. The
larger IGI1 corpus pass exercised 35,533 unique files across the listed formats;
the six collision-model exports that initially failed are separately rerun
after the fix on both platforms. IGI2 also had 1,275 loose script and mapping
assets checked natively on macOS (801 QVM, 442 DAT and 32 MTP files).

| Extension | IGI1 selected cases (macOS / Windows) | IGI2 selected cases (macOS / Windows) |
| --- | ---: | ---: |
| `.afp` | 3 / 3 | — |
| `.anm` | — | 1 / 1 |
| `.avi` | 1 / 1 | — |
| `.bit` | 13 / 13 | — |
| `.cmd` | 1 / 1 | — |
| `.ctr` | 1 / 1 | — |
| `.dat` | 9 / 9 | 69 / 69 |
| `.fnt` | 1 / 1 | 1 / 1 |
| `.hmp` | 6 / 6 | — |
| `.iff` | 1 / 1 | 22 / 22 |
| `.jpg` | — | 2 / 2 |
| `.lmp` | 3 / 3 | — |
| `.mef` | 7 / 7 | 16 / 16 |
| `.mp3` | — | 2 / 2 |
| `.mtp` | 1 / 1 | 1 / 1 |
| `.olm` | 38 / 38 | 838 / 838 |
| `.pic` | 3 / 3 | 5 / 5 |
| `.qsc` | 1 / 1 | 4 / 4 |
| `.qvm` | 1 / 1 | 1 / 1 |
| `.res` | 2 / 2 | 1 / 1 |
| `.spr` | 81 / 81 | 111 / 111 |
| `.syn` | — | 1 / 1 |
| `.tex` | 56 / 56 | 65 / 65 |
| `.thm` | — | 1 / 1 |
| `.tlm` | — | 1 / 1 |
| `.tmm` | — | 1 / 1 |
| `.wav` | 13 / 13 | 6 / 6 |

The corpus harness checks source hashes before and after conversion, native
round-trip fidelity, resource payload and filename preservation, decoded audio
parameters, comment-free scripts and nonempty model exports. ILSF ADPCM is
re-encoded and checked using a stated sample-error bound; lossy re-encoding is
not described as byte-identical. Navigation coordinate edits and lightmap pixel
edits must survive a rebuild without changing untouched metadata.

## Fixes discovered by the broader corpus

- Older IGI1 graphs use one-byte property tags and a trailing adjacency table.
  Native JSON export/import now preserves that structure and supports edits.
- SPR/ANM item descriptors can specify different frame sizes and pixel offsets.
  Native conversion and GUI decoding now honor each frame's descriptor.
- Some multilayer IGI1 OLM files contain extra descriptor bytes. Validate every
  layer and the full payload when choosing the layout. GUI lightmap decoding
  now uses the same validator rather than reading pixels after one descriptor.
- DAT/MTP conversion preserves the complete texture manifest. MTP INST aliases
  can be interleaved with primary model mappings; aliases are retained explicitly.
  JSON diagnostics expose aliases and other mapping metadata for comparison.
- IGI2 shadow MEF files can omit HSEM. Static geometry export resolves local CAFS
  indices through SEMS groups, including the unbound-bone sentinel. FORM/TDOB
  menu models retain every chunk in JSON and export VPOS/CFAC collision geometry.

## Remaining validation and limits

All four fresh Qt package CLI matrices passed: IGI1 33/33 and IGI2 38/38 on
each test platform. Windows results were run sequentially through the existing
CrossOver bottle using the packaged executables and Qt runtime dependencies.
All 22 selected Windows MINA container variants parsed successfully; the full
1,244-file animation parse sweep was run on macOS only.
The macOS bundles passed strict code-signature and embedded-framework checks;
verbose CLI logs were confirmed for both native macOS executables.

The 2026-10-12 interactive follow-up verified native IGI1 right-click animation
playback against a retail soldier MEF and matching 33-bone IFF, including changing
clips, pausing and resuming. A normal exit initially exposed shared OpenGL texture
references surviving context teardown; clearing those references while the context
is current fixed the shutdown warning, and a repeat animated-texture exit passed.

Retail assets installed through Wine carry macOS hidden-file attributes. The
shared GUI browser and asset directory scans now include hidden files. The fresh
IGI1 browser displays all 21 representative files, and IGI2 displays all 25.
IGI1's native GUI sweep exercised every representative: graph/MEF in 3D, IFF in
animation, QVM/DAT in text, OLM/PIC/SPR/TEX in image, WAV in audio, and remaining
formats in the hex viewer. WAV playback advanced the transport and stopped before
a normal exit. The QVM editor contains decompiled source without an injected
version comment. Hex display alone does not certify conversion or editing; those
operations are covered by the CLI matrices above.

Both games' rebuilt package CLI matrices passed again on native macOS and
CrossOver Windows (33/33 and 38/38). Both macOS bundles passed strict signature
checks; both Windows packages contain runtime DLLs and qwindows.dll. Both native format viewer sweeps completed (21 IGI1 and 25 IGI2 samples).
IGI2 MP3 playback advanced its transport, and editor font size changes persisted.
Windows IGI1 displayed graph/model previews and comment-free QVM source; a
repeated IFF selection displayed an advancing skeleton animation. It exited
normally with code 0. The Windows IGI2 25-file viewer sweep completed; inspected graph, MINA skeleton,
MEF and retail QVM views rendered correctly. MP3 playback changed to Pause and
advanced the timeline after a foreground-input retry. The app exited normally
with code 0. Both Windows GUI apps created their default verbose log files.

A scratch copy of the reported IGI1 security building was textured from the
retail DAT/TEX mapping through the GUI context menu. Its surfaces retained visible
texture detail when rotated, rather than the almost-black preview in the bug
report. The scratch preview excludes sibling models, so this is a brightness
regression check, not proof of complete assembled-building material coverage.
Normal textured shutdown passed without the earlier GL texture destruction
warning. Native IGI1 and IGI2 image painting/save passed; independently extracted
TEX bitmaps retain four painted pixels. IGI2 preserves all nine bitmap levels.
Windows IGI1 and IGI2 Flip H/save passed with independently verified mirrored pixel
positions. Both Windows GUI apps compiled and saved retail QVM successfully
and exited normally.

An additional native QVM-save test exposed Qt 6.11.2 Cocoa accessibility using
a stale cell's negative row index after atomic file replacement. The app aborted
with NSRangeException. The matching Cocoa plugin now guards absent tables and
invalid row/column indices, including synthetic-cell parent lookup, while
keeping accessibility enabled. The final r9 macOS bundles include the plugin
and corresponding source, dependency headers, licenses and rebuild instructions.
Both native apps passed a fresh accessibility scan, retail QVM save and normal
exit with code 0. Both bundles pass strict deep signature verification.
Remaining GUI controls and original-game acceptance checks are not fully
certified. No new release is published by this report yet.

Native Windows-host execution is not claimed. Full original GConv recipe/API
parity, Dreamcast/VQ processing, skeleton and weight authoring, unrestricted
mesh topology editing, and textured FORM/TDOB PRIM reconstruction remain outside
these results. Rebuilt assets have not all been loaded into the original games.
