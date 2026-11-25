# Minimal BEF import/export for Blender

This directory adds a small Blender addon (`bef_io/`) for the plain-text IGI 1
BEF animation format described in `source/parsers/iff_bef.h`. It is a
**minimal vertical slice**, not a port of the proprietary 3ds Max 4/5 plugin
attached to issue #13.

## Install

Copy or symlink `bef_io/` into Blender's `scripts/addons/` directory, then
enable **Minimal BEF IO** in Preferences → Add-ons. The operators appear under
File → Import/Export → IGI BEF and are also callable from Python:

```python
from bef_io.import_bef import import_bef
from bef_io.export_bef import export_bef

arm = import_bef("soldier_anim_004.bef")
export_bef("edited.bef", arm)
```

## Supported subset

* `AnimInit`, `Bone`, `BuildHierarchy`, `TranslationKeyFrameData`,
  `RotationKeyFrameData`, `TriggerData`.
* Import builds an armature (bone names, parent chain, head positions) and
  inserts rotation keyframes on pose bones and translation keyframes on the
  armature object at `1 + time_ms * fps / 1000` (default fps = 30).
* Export writes bone names/hierarchy/heads from the armature, q0 rotations
  from the evaluated pose at each keyed frame, and translations/events/flags
  from the stored `bef_meta` JSON property.
* Positions stay in BEF text space (the `/40.96` scale is not applied), so
  importing an unedited BEF and exporting it back reproduces its positions.

## Limitations / exact verification blocker

* The three quaternions per BEF rotation key are an IGI cubic-interpolation
  representation. Blender stores only q0; **q1/q2 and all IGI-only integers
  are preserved verbatim from `bef_meta` and are not editable in Blender**.
* IGI-to-Blender axis handedness and unit scale are not reconciled. Do not
  assume exported clips reproduce in-game orientation without a visual check
  against retail footage.
* Only one BEF clip per file/armature; no IFF container round-trip, no
  `BreakScript` editing, no material/mesh import.
* The linked `3DSExporter.zip` (3ds Max 4/5) is a user attachment and is not
  part of this repository. It was not used as a behavioural reference.
* **Verification blocker:** the Blender executable is absent in this
  environment. `/opt/homebrew/bin/blender` is a shim whose target
  `/Applications/Blender.app/Contents/MacOS/Blender` does not exist, so the
  `bpy` code path could not be executed here. The pure-Python format layer is
  tested with `python3 blender/tests/test_bef_format.py`; the bpy path has a
  ready-to-run script at `blender/tests/blender_roundtrip.py` that has **not**
  been executed and is not claimed as passing.

## Tests

```sh
python3 blender/tests/test_bef_format.py   # executed, passes
blender --background --factory-startup --python blender/tests/blender_roundtrip.py  # not executed here
```
