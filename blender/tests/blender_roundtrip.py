"""Headless Blender round-trip check for the BEF addon.

Run when a Blender executable is available:
    blender --background --factory-startup --python blender/tests/blender_roundtrip.py

This script exercises the real bpy import/export path (armature bones, pose
keyframes and the bef_meta property). It is intentionally NOT run by CI until
a Blender binary is installed on the runner.
"""

import os
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, ROOT)

from bef_io.bef_format import parse_bef  # noqa: E402
from bef_io.import_bef import import_bef  # noqa: E402
from bef_io.export_bef import export_bef  # noqa: E402

SAMPLE = """AnimInit("soldier_anim_004",7,1234,1);
BreakScript();
Bone(0,"root",-1,0,0,0);
Bone(1,"spine",0,0,10.24,0);
Bone(2,"head",1,0,20.48,0);
BuildHierarchy();
BreakScript();
TranslationKeyFrameData(0,1,0,0,0,0);
RotationKeyFrameData(1,0,0,0,0,0,1,0.1,0.2,0.3,1,0.4,0.5,0.6,1);
TriggerData(0,9,250,2,1,2,3);
"""


def main():
    import bpy

    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)

    with tempfile.TemporaryDirectory() as tmp:
        source = os.path.join(tmp, "sample.bef")
        exported = os.path.join(tmp, "roundtrip.bef")
        with open(source, "w", encoding="utf-8") as handle:
            handle.write(SAMPLE)

        armature = import_bef(source)
        assert len(armature.data.bones) == 3
        assert [b.name for b in armature.data.bones] == ["root", "spine", "head"]
        assert armature.data.bones[2].parent.name == "spine"
        assert armature.animation_data and armature.animation_data.action
        assert "bef_meta" in armature

        result = export_bef(exported, armature)
        assert [b["name"] for b in result["bones"]] == ["root", "spine", "head"]
        with open(exported, "r", encoding="utf-8") as handle:
            reparsed = parse_bef(handle.read())
        assert len(reparsed["rotations"]) == 1
        assert len(reparsed["events"]) == 1
        assert reparsed["anim_name"] == "soldier_anim_004"

    print("blender_roundtrip: PASS")


if __name__ == "__main__":
    main()
