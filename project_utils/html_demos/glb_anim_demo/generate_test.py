"""
Generate a test .glb with two cubes driven by ONE armature, ONE skin.
The cubes are JOINED into a single mesh object so Blender exports exactly one skin.
Two independent bones, each controlling one cube via vertex groups.

Usage: blender --background --python generate_test.py
"""

import bpy
import math
import os
from mathutils import Quaternion

bpy.ops.wm.read_factory_settings(use_empty=True)
for obj in bpy.data.objects:
    bpy.data.objects.remove(obj, do_unlink=True)

bpy.context.scene.frame_start = 0
bpy.context.scene.frame_end = 120
bpy.context.scene.render.fps = 60

# ---- Single armature with two bones ----
bpy.ops.object.armature_add(enter_editmode=True, location=(0, 0, 0))
armature = bpy.context.active_object
armature.name = "Armature"

bone1 = armature.data.edit_bones[0]
bone1.name = "SlowBone"
bone1.head = (-2, 0, -0.5)
bone1.tail = (-2, 0, 0.5)
bone1.parent = None

bone2 = armature.data.edit_bones.new("FastBone")
bone2.head = (2, 0, -0.5)
bone2.tail = (2, 0, 0.5)
bone2.parent = None

bpy.ops.object.mode_set(mode='OBJECT')

# ---- Create two cubes as separate objects first ----
bpy.ops.mesh.primitive_cube_add(size=1.5, location=(-2, 0, 0))
cube1 = bpy.context.active_object
cube1.name = "SlowCube"

bpy.ops.mesh.primitive_cube_add(size=1.5, location=(2, 0, 0))
cube2 = bpy.context.active_object
cube2.name = "FastCube"

# ---- Apply transforms so vertex positions are in world space ----
bpy.ops.object.select_all(action='DESELECT')
cube1.select_set(True)
bpy.context.view_layer.objects.active = cube1
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)

bpy.ops.object.select_all(action='DESELECT')
cube2.select_set(True)
bpy.context.view_layer.objects.active = cube2
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)

# ---- Join cubes into one mesh object ----
bpy.ops.object.select_all(action='DESELECT')
cube1.select_set(True)
cube2.select_set(True)
bpy.context.view_layer.objects.active = cube1
bpy.ops.object.join()
joined = bpy.context.active_object
joined.name = "CubesMesh"

# ---- Manually assign vertex groups (not auto weights — we need precise control) ----
# Clear any existing groups
for vg in joined.vertex_groups:
    joined.vertex_groups.remove(vg)

# Create groups matching bone names
vg_slow = joined.vertex_groups.new(name="SlowBone")
vg_fast = joined.vertex_groups.new(name="FastBone")

# Assign vertices based on X position:
# After join, cube1 verts are near x=-2, cube2 verts are near x=+2
# Use midpoint (x=0) as the split
slow_count = 0
fast_count = 0
for v in joined.data.vertices:
    if v.co.x < 0.0:
        vg_slow.add([v.index], 1.0, 'REPLACE')
        vg_fast.add([v.index], 0.0, 'REPLACE')
        slow_count += 1
    else:
        vg_fast.add([v.index], 1.0, 'REPLACE')
        vg_slow.add([v.index], 0.0, 'REPLACE')
        fast_count += 1

print(f"Total verts: {len(joined.data.vertices)}")
print(f"Vertex groups: SlowBone={slow_count}, FastBone={fast_count}")
# Debug: print all vertex positions
for v in joined.data.vertices:
    if v.index < 8 or v.index >= len(joined.data.vertices) - 4:
        print(f"  v[{v.index}] = ({v.co.x:.2f}, {v.co.y:.2f}, {v.co.z:.2f})")

# ---- Parent joined mesh to armature with vertex groups (NOT auto weights) ----
bpy.ops.object.select_all(action='DESELECT')
joined.select_set(True)
armature.select_set(True)
bpy.context.view_layer.objects.active = armature
bpy.ops.object.parent_set(type='ARMATURE_NAME')  # use existing vertex groups
bpy.ops.object.select_all(action='DESELECT')

# ---- Animate POSE BONES ----
bpy.context.view_layer.objects.active = armature
armature.select_set(True)
bpy.ops.object.mode_set(mode='POSE')

slow_bone = armature.pose.bones["SlowBone"]
fast_bone = armature.pose.bones["FastBone"]
slow_bone.rotation_mode = 'QUATERNION'
fast_bone.rotation_mode = 'QUATERNION'

total_frames = 120

for frame in range(0, total_frames + 1, 2):
    bpy.context.scene.frame_set(frame)
    t = frame / total_frames

    # SlowBone: slow spin + gentle scale
    angle1 = t * 1.0 * 2.0 * math.pi
    slow_bone.rotation_quaternion = Quaternion((0, 0, 1), angle1)
    slow_bone.keyframe_insert(data_path="rotation_quaternion", frame=frame)
    s1 = 1.0 + 0.2 * math.sin(t * 2.0 * 2.0 * math.pi)
    slow_bone.scale = (s1, s1, s1)
    slow_bone.keyframe_insert(data_path="scale", frame=frame)

    # FastBone: fast spin + dramatic scale
    angle2 = t * 3.0 * 2.0 * math.pi
    fast_bone.rotation_quaternion = Quaternion((0, 0, 1), angle2)
    fast_bone.keyframe_insert(data_path="rotation_quaternion", frame=frame)
    s2 = 1.0 + 0.4 * math.sin(t * 4.0 * 2.0 * math.pi)
    fast_bone.scale = (s2, s2, s2)
    fast_bone.keyframe_insert(data_path="scale", frame=frame)

bpy.ops.object.mode_set(mode='OBJECT')

# ---- Export ----
bpy.context.scene.frame_set(0)
script_dir = os.path.dirname(os.path.abspath(__file__))
output_path = os.path.join(script_dir, "test_cubes.glb")

bpy.ops.export_scene.gltf(
    filepath=output_path,
    export_format='GLB',
    export_animations=True,
    export_skins=True,
    export_all_influences=True,
    export_apply=False,
)
print(f"Exported: {output_path}")
