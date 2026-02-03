#!/usr/bin/env python3
"""
Blender Python script to convert images to GLB plane files.

Usage:
    blender --background --python images_to_glb.py -- <input_dir> <output_dir>

Example:
    blender --background --python images_to_glb.py -- ./assets/textures ./assets/glb_output

Each image becomes a textured plane exported as a GLB file.
The plane is sized to match the image aspect ratio (normalized to max 1.0 unit).
"""

import bpy
import sys
import os
from pathlib import Path


def clear_scene():
    """Remove all objects from the scene."""
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete()

    # Clear orphan data
    for block in bpy.data.meshes:
        if block.users == 0:
            bpy.data.meshes.remove(block)
    for block in bpy.data.materials:
        if block.users == 0:
            bpy.data.materials.remove(block)
    for block in bpy.data.textures:
        if block.users == 0:
            bpy.data.textures.remove(block)
    for block in bpy.data.images:
        if block.users == 0:
            bpy.data.images.remove(block)


def create_textured_plane(image_path: str, name: str) -> bpy.types.Object:
    """Create a plane with the image as a texture."""
    # Load the image
    img = bpy.data.images.load(image_path)
    width, height = img.size

    # Calculate aspect ratio (normalize to max 1.0)
    if width > height:
        plane_width = 1.0
        plane_height = height / width
    else:
        plane_height = 1.0
        plane_width = width / height

    # Create plane mesh
    bpy.ops.mesh.primitive_plane_add(size=1, location=(0, 0, 0))
    plane = bpy.context.active_object
    plane.name = name

    # Scale to match aspect ratio
    plane.scale = (plane_width, plane_height, 1.0)
    bpy.ops.object.transform_apply(scale=True)

    # Rotate so plane faces up (+Y in Blender, will be +Y in GLB/Raylib)
    # Default plane is on XY plane facing +Z, we want it on XZ plane facing +Y
    plane.rotation_euler = (0, 0, 0)  # Keep flat, facing +Z for now

    # Create material with the texture
    mat = bpy.data.materials.new(name=f"{name}_material")
    mat.use_nodes = True

    nodes = mat.node_tree.nodes
    links = mat.node_tree.links

    # Clear default nodes
    nodes.clear()

    # Create nodes
    output_node = nodes.new('ShaderNodeOutputMaterial')
    output_node.location = (300, 0)

    bsdf_node = nodes.new('ShaderNodeBsdfPrincipled')
    bsdf_node.location = (0, 0)

    tex_node = nodes.new('ShaderNodeTexImage')
    tex_node.location = (-300, 0)
    tex_node.image = img

    # Link nodes
    links.new(tex_node.outputs['Color'], bsdf_node.inputs['Base Color'])
    links.new(tex_node.outputs['Alpha'], bsdf_node.inputs['Alpha'])
    links.new(bsdf_node.outputs['BSDF'], output_node.inputs['Surface'])

    # Enable alpha blending
    mat.blend_method = 'BLEND'

    # Assign material to plane
    plane.data.materials.append(mat)

    # UV unwrap (plane already has default UVs)

    return plane


def export_to_glb(output_path: str):
    """Export the scene to GLB format."""
    bpy.ops.export_scene.gltf(
        filepath=output_path,
        export_format='GLB',
        use_selection=True,
        export_apply=True,
        export_texcoords=True,
        export_normals=True,
        export_materials='EXPORT',
        export_image_format='AUTO',
    )


def convert_image_to_glb(image_path: str, output_dir: str) -> str:
    """Convert a single image to a GLB file."""
    clear_scene()

    image_name = Path(image_path).stem
    output_path = os.path.join(output_dir, f"{image_name}.glb")

    plane = create_textured_plane(image_path, image_name)
    plane.select_set(True)

    export_to_glb(output_path)

    print(f"Converted: {image_path} -> {output_path}")
    return output_path


def process_directory(input_dir: str, output_dir: str, recursive: bool = True):
    """Process all images in a directory."""
    os.makedirs(output_dir, exist_ok=True)

    image_extensions = {'.png', '.jpg', '.jpeg', '.bmp', '.gif', '.tiff', '.tga'}

    input_path = Path(input_dir)

    if recursive:
        files = input_path.rglob('*')
    else:
        files = input_path.glob('*')

    count = 0
    for file_path in files:
        if file_path.is_file() and file_path.suffix.lower() in image_extensions:
            # Preserve directory structure in output
            rel_path = file_path.relative_to(input_path)
            out_subdir = output_dir
            if rel_path.parent != Path('.'):
                out_subdir = os.path.join(output_dir, str(rel_path.parent))
                os.makedirs(out_subdir, exist_ok=True)

            try:
                convert_image_to_glb(str(file_path), out_subdir)
                count += 1
            except Exception as e:
                print(f"Error converting {file_path}: {e}")

    print(f"\nConverted {count} images to GLB files in {output_dir}")


def main():
    # Get arguments after '--'
    argv = sys.argv
    if '--' in argv:
        argv = argv[argv.index('--') + 1:]
    else:
        print(__doc__)
        print("Error: No arguments provided. Use -- to separate Blender args from script args.")
        sys.exit(1)

    if len(argv) < 2:
        print(__doc__)
        print("Error: Need input_dir and output_dir arguments.")
        sys.exit(1)

    input_dir = argv[0]
    output_dir = argv[1]

    if not os.path.isdir(input_dir):
        print(f"Error: Input directory does not exist: {input_dir}")
        sys.exit(1)

    print(f"Converting images from: {input_dir}")
    print(f"Output directory: {output_dir}")

    process_directory(input_dir, output_dir)


if __name__ == "__main__":
    main()
