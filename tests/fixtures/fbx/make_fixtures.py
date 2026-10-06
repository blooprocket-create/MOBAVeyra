"""Writes one of tests/test_fbx_content.py's fixtures: a small cube exported as binary FBX, as the art generators export.

Run each in its own background Blender, so each export carries its own time stamp and object numbers:
    blender --background --factory-startup --python make_fixtures.py -- cube_a.fbx
    blender --background --factory-startup --python make_fixtures.py -- cube_b.fbx
    blender --background --factory-startup --python make_fixtures.py -- cube_moved.fbx --moved
    blender --background --factory-startup --python make_fixtures.py -- cube_glowing.fbx --glowing
    blender --background --factory-startup --python make_fixtures.py -- cube_renamed.fbx --renamed
cube_a and cube_b hold the same cube; cube_moved has one vertex moved by a millimetre; cube_glowing's material glows
(values no importer reads); cube_renamed's material slot has another name.
"""
import sys
from pathlib import Path

import bpy

ARGS = sys.argv[sys.argv.index("--") + 1:]
TARGET = Path(__file__).resolve().parent / ARGS[0]
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.mesh.primitive_cube_add(size=1.0)
cube = bpy.context.active_object
cube.name = "SM_FixtureCube"
material = bpy.data.materials.new("Iron" if "--renamed" in ARGS else "Stone")
material.use_nodes = True
if "--glowing" in ARGS:
    shader = material.node_tree.nodes.get("Principled BSDF")
    shader.inputs["Emission Color"].default_value = (0.1, 0.3, 0.9, 1.0)
    shader.inputs["Emission Strength"].default_value = 2.5
cube.data.materials.append(material)
if "--moved" in ARGS:
    cube.data.vertices[0].co.x += 0.001
bpy.ops.export_scene.fbx(filepath=str(TARGET), use_selection=True, object_types={"MESH"}, apply_unit_scale=True,
                         apply_scale_options="FBX_SCALE_UNITS", axis_forward="-Y", axis_up="Z", bake_anim=False,
                         mesh_smooth_type="EDGE", colors_type="SRGB", use_mesh_modifiers=False)
print("VEYRA_FIXTURE: " + str(TARGET))
