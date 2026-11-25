"""min Bef IO — minimal IGI 1 BEF armature/animation import and export.

Install by symlinking or copying this ``bef_io`` directory into Blender's
``scripts/addons`` directory, then enabling "Minimal BEF IO" in preferences.
See README.md for the supported subset and known limitations.
"""

bl_info = {
    "name": "Minimal BEF IO",
    "description": "Import/export IGI 1 BEF armatures and animation clips",
    "version": (0, 1, 0),
    "blender": (3, 0, 0),
    "location": "File > Import/Export > IGI BEF",
    "category": "Import-Export",
}

from .import_bef import import_bef
from .export_bef import export_bef


def _register_operators():
    import bpy

    class BEF_OT_import(bpy.types.Operator):
        bl_idname = "bef.import_file"
        bl_label = "Import IGI BEF"
        filepath: bpy.props.StringProperty(subtype="FILE_PATH")

        def execute(self, context):
            import_bef(self.filepath)
            self.report({"INFO"}, "Imported BEF armature")
            return {"FINISHED"}

        def invoke(self, context, event):
            context.window_manager.fileselect_add(self)
            return {"RUNNING_MODAL"}

    class BEF_OT_export(bpy.types.Operator):
        bl_idname = "bef.export_file"
        bl_label = "Export IGI BEF"
        filepath: bpy.props.StringProperty(subtype="FILE_PATH")

        def execute(self, context):
            armature = context.view_layer.objects.active
            if not armature or armature.type != "ARMATURE":
                self.report({"ERROR"}, "Select an armature first")
                return {"CANCELLED"}
            export_bef(self.filepath, armature)
            self.report({"INFO"}, "Exported BEF armature")
            return {"FINISHED"}

        def invoke(self, context, event):
            context.window_manager.fileselect_add(self)
            return {"RUNNING_MODAL"}

    def menu_import(self, context):
        self.layout.operator(BEF_OT_import.bl_idname, text="IGI BEF (.bef)")

    def menu_export(self, context):
        self.layout.operator(BEF_OT_export.bl_idname, text="IGI BEF (.bef)")

    return BEF_OT_import, BEF_OT_export, menu_import, menu_export


_classes = None


def register():
    global _classes
    _classes = _register_operators()
    import bpy
    for cls in _classes[:2]:
        bpy.utils.register_class(cls)
    bpy.types.TOPBAR_MT_file_import.append(_classes[2])
    bpy.types.TOPBAR_MT_file_export.append(_classes[3])


def unregister():
    import bpy
    if _classes is None:
        return
    bpy.types.TOPBAR_MT_file_import.remove(_classes[2])
    bpy.types.TOPBAR_MT_file_export.remove(_classes[3])
    for cls in reversed(_classes[:2]):
        bpy.utils.unregister_class(cls)
