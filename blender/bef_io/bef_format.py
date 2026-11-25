"""Pure-Python reader/writer for the IGI 1 BEF text format.

This module has no Blender dependency so it can be tested with plain Python.
The grammar mirrors source/parsers/iff_bef.cpp in this repository:

    AnimInit("name", flags, length_ms, tp_flag);
    Bone(idx, "name", parent, px, py, pz);
    BuildHierarchy();
    TranslationKeyFrameData(track, flag, time_ms, px, py, pz);
    RotationKeyFrameData(bone, flag, time_ms, q0xyzw, q1xyzw, q2xyzw);
    TriggerData(idx, event_id, time_ms, bone_id, px, py, pz);

Positions in the text file are divided by SC (40.96); the engine multiplies
them back. Blender stores the text-space values directly (see import_bef.py).
"""

import re

SC = 40.96
_NUMBER = re.compile(r"[-+]?(?:\d+\.?\d*|\.\d+)(?:[eE][-+]?\d+)?")
_QUOTED = re.compile(r'"([^"]*)"')


def _values(line):
    # Quoted names may contain digits ("soldier_anim_004"); strip them first.
    line = _QUOTED.sub('""', line)
    return [float(v) for v in _NUMBER.findall(line)]


def _int(values, index, default=0):
    try:
        return int(values[index])
    except (IndexError, ValueError):
        return default


def _float(values, index, default=0.0):
    try:
        return float(values[index])
    except (IndexError, ValueError):
        return default


def parse_bef(text):
    """Parse BEF text into a plain dict. Unknown statements are ignored."""
    data = {
        "anim_name": "",
        "flags": 0,
        "length_ms": 0,
        "tp_flag": 0,
        "bones": [],
        "translations": [],
        "rotations": [],
        "events": [],
    }
    for raw in text.splitlines():
        line = raw.split("//", 1)[0].strip()
        if not line:
            continue
        if line.startswith("AnimInit"):
            quoted = _QUOTED.search(line)
            values = _values(line)
            data["anim_name"] = quoted.group(1) if quoted else ""
            data["flags"] = _int(values, 0)
            data["length_ms"] = _int(values, 1)
            data["tp_flag"] = _int(values, 2)
        elif line.startswith("Bone("):
            quoted = _QUOTED.search(line)
            values = _values(line)
            data["bones"].append({
                "index": _int(values, 0),
                "name": quoted.group(1) if quoted else "bone",
                "parent": _int(values, 1, -1),
                "px": _float(values, 2),
                "py": _float(values, 3),
                "pz": _float(values, 4),
            })
        elif line.startswith("TranslationKeyFrameData"):
            v = _values(line)
            data["translations"].append({
                "track": _int(v, 0), "flag": _int(v, 1), "time_ms": _int(v, 2),
                "px": _float(v, 3), "py": _float(v, 4), "pz": _float(v, 5),
            })
        elif line.startswith("RotationKeyFrameData"):
            v = _values(line)
            data["rotations"].append({
                "bone": _int(v, 0), "flag": _int(v, 1), "time_ms": _int(v, 2),
                "q0": [float(x) for x in v[3:7]],
                "q1": [float(x) for x in v[7:11]],
                "q2": [float(x) for x in v[11:15]],
            })
        elif line.startswith("TriggerData"):
            v = _values(line)
            data["events"].append({
                "index": _int(v, 0), "event_id": _int(v, 1),
                "time_ms": _int(v, 2), "bone_id": _int(v, 3),
                "px": _float(v, 4), "py": _float(v, 5), "pz": _float(v, 6),
            })
    return data


def _f(value):
    """Render a float with enough precision for a lossless text round-trip."""
    return repr(float(value))


def write_bef(data):
    """Serialise a dict produced by parse_bef() back to BEF text."""
    out = [
        "// Script for converting common models //////////////////////////////////////",
        "",
        'AnimInit("{}",{},{},{});'.format(
            data.get("anim_name", ""), int(data.get("flags", 0)),
            int(data.get("length_ms", 0)), int(data.get("tp_flag", 0))),
        "BreakScript();",
    ]
    for b in data.get("bones", []):
        out.append('Bone({},"{}",{},{},{},{});'.format(
            int(b["index"]), b["name"], int(b["parent"]),
            _f(b["px"]), _f(b["py"]), _f(b["pz"])))
    out.extend(["BuildHierarchy();", "BreakScript();"])
    for t in data.get("translations", []):
        out.append("TranslationKeyFrameData({},{},{},{},{},{});".format(
            int(t["track"]), int(t["flag"]), int(t["time_ms"]),
            _f(t["px"]), _f(t["py"]), _f(t["pz"])))
    for r in data.get("rotations", []):
        quats = [r["q0"], r["q1"], r["q2"]]
        values = ",".join(_f(v) for q in quats for v in q)
        out.append("RotationKeyFrameData({},{},{},{});".format(
            int(r["bone"]), int(r["flag"]), int(r["time_ms"]), values))
    for e in data.get("events", []):
        out.append("TriggerData({},{},{},{},{},{},{});".format(
            int(e["index"]), int(e["event_id"]), int(e["time_ms"]),
            int(e["bone_id"]), _f(e["px"]), _f(e["py"]), _f(e["pz"])))
    return "\n".join(out) + "\n"
