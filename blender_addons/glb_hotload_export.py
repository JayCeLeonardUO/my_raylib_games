bl_info = {
    "name": "GLB Hotload Export",
    "author": "Claude",
    "version": (1, 0),
    "blender": (4, 0, 0),
    "location": "View3D > Sidebar > Hotload",
    "description": "One-click export to GLB for Raylib hotload viewer",
    "category": "Import-Export",
}

import bpy
import os
from bpy.props import StringProperty, BoolProperty, EnumProperty


class HOTLOAD_OT_export(bpy.types.Operator):
    """Export to GLB for Raylib hotload"""
    bl_idname = "hotload.export"
    bl_label = "Export to Raylib"
    bl_options = {'REGISTER'}

    def execute(self, context):
        props = context.scene.hotload_props
        export_path = bpy.path.abspath(props.export_path)

        # Ensure directory exists
        export_dir = os.path.dirname(export_path)
        if export_dir and not os.path.exists(export_dir):
            os.makedirs(export_dir)

        # Determine what to export
        if props.export_mode == 'SELECTED':
            if not context.selected_objects:
                self.report({'WARNING'}, "No objects selected")
                return {'CANCELLED'}
            use_selection = True
        else:
            use_selection = False

        # Export GLB
        try:
            bpy.ops.export_scene.gltf(
                filepath=export_path,
                use_selection=use_selection,
                export_format='GLB',
                export_apply=props.apply_modifiers,
            )
            self.report({'INFO'}, f"Exported to {export_path}")
        except Exception as e:
            self.report({'ERROR'}, f"Export failed: {e}")
            return {'CANCELLED'}

        return {'FINISHED'}


class HOTLOAD_OT_set_path(bpy.types.Operator):
    """Set export path via file browser"""
    bl_idname = "hotload.set_path"
    bl_label = "Browse"
    bl_options = {'REGISTER'}

    filepath: StringProperty(subtype='FILE_PATH')

    def execute(self, context):
        context.scene.hotload_props.export_path = self.filepath
        return {'FINISHED'}

    def invoke(self, context, event):
        context.window_manager.fileselect_add(self)
        return {'RUNNING_MODAL'}


class HOTLOAD_Props(bpy.types.PropertyGroup):
    export_path: StringProperty(
        name="Export Path",
        description="Path to export the GLB file",
        default="//hotload.glb",
        subtype='FILE_PATH'
    )

    export_mode: EnumProperty(
        name="Export",
        description="What to export",
        items=[
            ('SCENE', "Entire Scene", "Export all visible objects"),
            ('SELECTED', "Selected Only", "Export only selected objects"),
        ],
        default='SCENE'
    )

    apply_modifiers: BoolProperty(
        name="Apply Modifiers",
        description="Apply modifiers before exporting",
        default=True
    )


class HOTLOAD_PT_panel(bpy.types.Panel):
    """Hotload Export Panel"""
    bl_label = "GLB Hotload"
    bl_idname = "HOTLOAD_PT_panel"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = 'Hotload'

    def draw(self, context):
        layout = self.layout
        props = context.scene.hotload_props

        # Export path
        box = layout.box()
        box.label(text="Export Path:")
        row = box.row(align=True)
        row.prop(props, "export_path", text="")
        row.operator("hotload.set_path", text="", icon='FILEBROWSER')

        # Options
        box = layout.box()
        box.label(text="Options:")
        box.prop(props, "export_mode")
        box.prop(props, "apply_modifiers")

        # Export button
        layout.separator()
        row = layout.row()
        row.scale_y = 2.0
        row.operator("hotload.export", text="EXPORT TO RAYLIB", icon='EXPORT')

        # Info
        layout.separator()
        box = layout.box()
        box.label(text="Info:", icon='INFO')
        if props.export_mode == 'SELECTED':
            count = len(context.selected_objects)
            box.label(text=f"Selected: {count} object(s)")
        else:
            count = len([o for o in context.scene.objects if o.visible_get()])
            box.label(text=f"Visible: {count} object(s)")


classes = (
    HOTLOAD_Props,
    HOTLOAD_OT_export,
    HOTLOAD_OT_set_path,
    HOTLOAD_PT_panel,
)


def register():
    for cls in classes:
        bpy.utils.register_class(cls)
    bpy.types.Scene.hotload_props = bpy.props.PointerProperty(type=HOTLOAD_Props)


def unregister():
    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)
    del bpy.types.Scene.hotload_props


if __name__ == "__main__":
    register()
