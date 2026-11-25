"""Minimal IGI 1 BEF importer for Blender.

Creates an armature (bone names, hierarchy and positions) and inserts the
rotation/translation keyframes into an Action. IGI-specific integers that
Blender cannot represent natively (flags, q1/q2 cubic control points,
trigger events) are preserved verbatim in the armature's ``bef_meta`` JSON
property so the exporter can write them back unchanged.

Coordinate convention: BEF text stores px/py/pz divided by SC (40.96). The
armature uses those same text-space numbers, so no unit conversion happens
here. IGI and Blender axis handedness is *not* reconciled in this minimal
slice; see blender/README.md.
"""

import json

from .bef_format import SC, parse_bef

DEFAULT_FPS = 30.0


def import_bef(filepath, fps=DEFAULT_FPS):
    """Import one BEF file into the active Blender scene. Returns the armature."""
    import bpy

    with open(filepath, "r", encoding="utf-8", errors="replace") as handle:
        data = parse_bef(handle.read())

    name = data["anim_name"] or "BEF_animation"
    armature_data = bpy.data.armatures.new(name + "_skeleton")
    armature = bpy.data.objects.new(name, armature_data)
    bpy.context.collection.objects.link(armature)
    bpy.ops.object.select_all(action="DESELECT")
    armature.select_set(True)
    bpy.context.view_layer.objects.active = armature

    bpy.ops.object.mode_set(mode="EDIT")
    for bone in data["bones"]:
        edit_bone = armature_data.edit_bones.new(bone["name"])
        edit_bone.head = (bone["px"], bone["py"], bone["pz"])
        edit_bone.tail = (bone["px"], bone["py"], bone["pz"] + 0.25)
    for index, bone in enumerate(data["bones"]):
        parent = bone["parent"]
        if 0 <= parent < len(data["bones"]):
            child = armature_data.edit_bones.get(bone["name"])
            parent_bone = armature_data.edit_bones.get(data["bones"][parent]["name"])
            if child and parent_bone and child != parent_bone:
                child.parent = parent_bone
    bpy.ops.object.mode_set(mode="OBJECT")

    for track in data["translations"]:
        frame = 1.0 + track["time_ms"] * fps / 1000.0
        armature.location = (track["px"], track["py"], track["pz"])
        armature.keyframe_insert(data_path="location", frame=frame)

    for rotation in data["rotations"]:
        frame = 1.0 + rotation["time_ms"] * fps / 1000.0
        bone_index = rotation["bone"]
        if 0 <= bone_index < len(data["bones"]):
            pose_bone = armature.pose.bones.get(data["bones"][bone_index]["name"])
        else:
            pose_bone = None
        if pose_bone is not None:
            pose_bone.rotation_mode = "QUATERNION"
            x, y, z, w = rotation["q0"]
            pose_bone.rotation_quaternion = (w, x, y, z)
            pose_bone.keyframe_insert(data_path="rotation_quaternion", frame=frame)

    if armature.animation_data and armature.animation_data.action:
        armature.animation_data.action.name = name + "_action"
    armature["bef_meta"] = json.dumps(data)
    armature["bef_fps"] = fps
    return armature
