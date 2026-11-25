"""Minimal IGI 1 BEF exporter for Blender.

Reads the active armature: bone names, hierarchy and head positions are taken
from the armature itself, q0 rotation values from the evaluated pose at each
keyed frame, and translation values from the object. IGI-only values that
have no Blender equivalent (flags, q1/q2, events) are read back from the
``bef_meta`` property written by the importer.
"""

import json

from .bef_format import write_bef
from .import_bef import DEFAULT_FPS


def export_bef(filepath, armature, fps=None):
    """Write ``armature`` to ``filepath`` as BEF text. Returns the dict written."""
    import bpy

    meta = json.loads(armature.get("bef_meta", "{}"))
    if fps is None:
        fps = float(armature.get("bef_fps", DEFAULT_FPS))

    data = {
        "anim_name": armature.name,
        "flags": int(meta.get("flags", 0)),
        "length_ms": int(meta.get("length_ms", 0)),
        "tp_flag": int(meta.get("tp_flag", 0)),
        "bones": [],
        "translations": list(meta.get("translations", [])),
        "rotations": [],
        "events": list(meta.get("events", [])),
    }

    bones = list(armature.data.bones)
    name_to_index = {bone.name: index for index, bone in enumerate(bones)}
    for index, bone in enumerate(bones):
        parent_index = name_to_index.get(bone.parent.name, -1) if bone.parent else -1
        head = bone.head_local
        data["bones"].append({
            "index": index,
            "name": bone.name,
            "parent": parent_index,
            "px": float(head.x), "py": float(head.y), "pz": float(head.z),
        })

    scene = bpy.context.scene
    for source in meta.get("rotations", []):
        frame = 1.0 + source["time_ms"] * fps / 1000.0
        scene.frame_set(int(round(frame)))
        bone_index = int(source["bone"])
        pose_bone = None
        if 0 <= bone_index < len(bones):
            pose_bone = armature.pose.bones.get(bones[bone_index].name)
        q = [float(x) for x in source.get("q0", [0.0, 0.0, 0.0, 1.0])]
        if pose_bone is not None:
            bq = pose_bone.rotation_quaternion
            # Blender stores (w, x, y, z); BEF stores (x, y, z, w).
            q = [bq.x, bq.y, bq.z, bq.w]
        data["rotations"].append({
            "bone": bone_index,
            "flag": int(source["flag"]),
            "time_ms": int(source["time_ms"]),
            "q0": q,
            "q1": list(source.get("q1", [0.0, 0.0, 0.0, 1.0])),
            "q2": list(source.get("q2", [0.0, 0.0, 0.0, 1.0])),
        })

    with open(filepath, "w", encoding="utf-8") as handle:
        handle.write(write_bef(data))
    return data
