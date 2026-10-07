"""A level set's polygons as a Blender mesh object, and the sculpt part nearest each of its points."""
import bpy
import numpy as np

from . import tree, workers


def to_object(name, points, triangles, quads):
    """A mesh object of the polygons OpenVDB made, faces turned to face out (OpenVDB winds them the other way)."""
    quads, triangles = quads[:, ::-1].copy(), triangles[:, ::-1].copy()
    mesh = bpy.data.meshes.new(name)
    mesh.vertices.add(len(points))
    mesh.vertices.foreach_set("co", points.astype(np.float32).ravel())
    loops = np.concatenate([quads.reshape(-1), triangles.reshape(-1)]).astype(np.int32)
    sizes = np.concatenate([np.full(len(quads), 4, dtype=np.int32), np.full(len(triangles), 3, dtype=np.int32)])
    starts = np.concatenate([[0], np.cumsum(sizes)[:-1]]).astype(np.int32)
    mesh.loops.add(len(loops))
    mesh.loops.foreach_set("vertex_index", loops)
    mesh.polygons.add(len(sizes))
    mesh.polygons.foreach_set("loop_start", starts)
    mesh.polygons.foreach_set("loop_total", sizes)
    mesh.update(calc_edges=True)
    mesh.validate()
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.scene.collection.objects.link(obj)
    return obj


def labels_at(root, points, cell=8.0):
    """The label of the part whose surface is nearest each point, evaluated in cells so each evaluates only the parts
    near it."""
    tree.REACH[0] = 2.0
    P = np.asarray(points, dtype=np.float32)
    (labels,) = tree.in_cells(root, P, workers.labels, cell)
    return labels.astype(np.int16)
