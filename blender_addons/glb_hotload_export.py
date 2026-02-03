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


def clear_scene_objects():
    """Remove all mesh objects from scene"""
    bpy.ops.object.select_all(action='DESELECT')
    for obj in bpy.data.objects:
        if obj.type in ('MESH', 'CURVE', 'SURFACE', 'META', 'FONT', 'EMPTY'):
            obj.select_set(True)
    bpy.ops.object.delete()


class HOTLOAD_OT_import(bpy.types.Operator):
    """Import GLB as starting point"""
    bl_idname = "hotload.import"
    bl_label = "Import GLB"
    bl_options = {'REGISTER', 'UNDO'}

    filepath: StringProperty(subtype='FILE_PATH')

    def execute(self, context):
        props = context.scene.hotload_props

        # Use the provided filepath or the export path
        import_path = self.filepath if self.filepath else bpy.path.abspath(props.export_path)

        if not import_path or not os.path.exists(import_path):
            self.report({'ERROR'}, f"File not found: {import_path}")
            return {'CANCELLED'}

        try:
            # Clear scene if enabled
            if props.clear_scene:
                clear_scene_objects()

            bpy.ops.import_scene.gltf(filepath=import_path)
            self.report({'INFO'}, f"Imported: {import_path}")

        except Exception as e:
            self.report({'ERROR'}, f"Import failed: {e}")
            return {'CANCELLED'}

        return {'FINISHED'}

    def invoke(self, context, event):
        # Start in the GLB zoo directory
        props = context.scene.hotload_props
        if props.glb_zoo_path:
            self.filepath = props.glb_zoo_path
        context.window_manager.fileselect_add(self)
        return {'RUNNING_MODAL'}


class HOTLOAD_OT_import_from_path(bpy.types.Operator):
    """Import GLB from the current export path"""
    bl_idname = "hotload.import_from_path"
    bl_label = "Load from Export Path"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        props = context.scene.hotload_props
        import_path = bpy.path.abspath(props.export_path)

        if not import_path or not os.path.exists(import_path):
            self.report({'ERROR'}, f"File not found: {import_path}")
            return {'CANCELLED'}

        try:
            # Clear scene if enabled
            if props.clear_scene:
                clear_scene_objects()

            bpy.ops.import_scene.gltf(filepath=import_path)
            self.report({'INFO'}, f"Imported: {import_path}")
        except Exception as e:
            self.report({'ERROR'}, f"Import failed: {e}")
            return {'CANCELLED'}

        return {'FINISHED'}


class HOTLOAD_OT_export(bpy.types.Operator):
    """Export to GLB for Raylib hotload"""
    bl_idname = "hotload.export"
    bl_label = "Export to Raylib"
    bl_options = {'REGISTER'}

    def execute(self, context):
        import subprocess

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

        # Launch Raylib viewer if not running
        viewer_path = bpy.path.abspath(props.viewer_path)
        if viewer_path and os.path.exists(viewer_path):
            # Check if viewer is already running
            try:
                result = subprocess.run(['pgrep', '-f', 'glb_hotload'], capture_output=True)
                viewer_running = result.returncode == 0
            except:
                viewer_running = False

            if not viewer_running:
                try:
                    subprocess.Popen([viewer_path, export_path],
                                   start_new_session=True,
                                   stdout=subprocess.DEVNULL,
                                   stderr=subprocess.DEVNULL)
                    self.report({'INFO'}, f"Launched viewer: {viewer_path}")
                except Exception as e:
                    self.report({'WARNING'}, f"Could not launch viewer: {e}")

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
        default="/home/jpleona/CLionProjects/my_raylib_games/assets/hotload.glb",
        subtype='FILE_PATH'
    )

    glb_zoo_path: StringProperty(
        name="GLB Zoo Path",
        description="Path to GLB model zoo directory",
        default="/home/jpleona/CLionProjects/my_raylib_games/assets/glb_output",
        subtype='DIR_PATH'
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

    clear_scene: BoolProperty(
        name="Clear Scene",
        description="Clear scene before importing",
        default=True
    )

    viewer_path: StringProperty(
        name="Viewer Path",
        description="Path to Raylib GLB viewer executable",
        default="/home/jpleona/CLionProjects/my_raylib_games/build/glb_hotload",
        subtype='FILE_PATH'
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

        # Viewer path
        box = layout.box()
        box.label(text="Raylib Viewer:")
        box.prop(props, "viewer_path", text="")

        # Options
        box = layout.box()
        box.label(text="Options:")
        box.prop(props, "export_mode")
        box.prop(props, "apply_modifiers")

        # Import section
        box = layout.box()
        box.label(text="Import from GLB Zoo:")
        box.prop(props, "glb_zoo_path", text="")
        box.prop(props, "clear_scene")
        row = box.row(align=True)
        row.scale_y = 1.5
        row.operator("hotload.import", text="LOAD GLB", icon='IMPORT')
        row = box.row(align=True)
        row.operator("hotload.import_from_path", text="Reload Current", icon='FILE_REFRESH')

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
    HOTLOAD_OT_import,
    HOTLOAD_OT_import_from_path,
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
