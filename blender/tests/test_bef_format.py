"""Corpus-free test of the pure-Python BEF format layer.

Runs with plain python3 (no Blender required):
    python3 blender/tests/test_bef_format.py
"""

import os
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from bef_io.bef_format import parse_bef, write_bef  # noqa: E402

SAMPLE = """// Script for converting common models
AnimInit("soldier_anim_004",7,1234,1);
BreakScript();
Bone(0,"root",-1,0,0,0);
Bone(1,"spine",0,0,10.24,0);
Bone(2,"head",1,0,20.48,0);
BuildHierarchy();
BreakScript();
TranslationKeyFrameData(0,1,0,0,0,0);
TranslationKeyFrameData(0,1,500,1.5,2.5,3.5);
RotationKeyFrameData(1,0,0,0,0,0,1,0.1,0.2,0.3,1,0.4,0.5,0.6,1);
RotationKeyFrameData(2,0,250,0,0,0,1,0.1,0.2,0.3,1,0.4,0.5,0.6,1);
TriggerData(0,9,250,2,1,2,3);
"""


def test_parse_and_write_roundtrip():
    first = parse_bef(SAMPLE)
    assert first["anim_name"] == "soldier_anim_004"
    assert (first["flags"], first["length_ms"], first["tp_flag"]) == (7, 1234, 1)
    assert [b["name"] for b in first["bones"]] == ["root", "spine", "head"]
    assert [b["parent"] for b in first["bones"]] == [-1, 0, 1]
    assert len(first["rotations"]) == 2
    assert first["events"][0]["event_id"] == 9

    text = write_bef(first)
    second = parse_bef(text)
    assert second == first


def test_file_roundtrip():
    with tempfile.TemporaryDirectory() as tmp:
        path = os.path.join(tmp, "sample.bef")
        with open(path, "w", encoding="utf-8") as handle:
            handle.write(SAMPLE)
        with open(path, "r", encoding="utf-8") as handle:
            data = parse_bef(handle.read())
        with open(path + ".out", "w", encoding="utf-8") as handle:
            handle.write(write_bef(data))
        with open(path + ".out", "r", encoding="utf-8") as handle:
            assert parse_bef(handle.read()) == data


if __name__ == "__main__":
    test_parse_and_write_roundtrip()
    test_file_roundtrip()
    print("bef_format: 2 tests passed")
