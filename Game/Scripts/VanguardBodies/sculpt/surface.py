"""A level set's polygons as a Blender mesh object, and the sculpt part nearest each of its points."""
import bpy
import numpy as np

from . import tree


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
    """The label of the part whose surface is nearest each point (every part, detail included), evaluated in cells so
    each evaluates only the parts near it."""
    tree.MODE[0] = "detail"
    tree.REACH[0] = 2.0
    P = np.asarray(points, dtype=np.float32)
    keys = np.floor(P / cell).astype(np.int64)
    order = np.lexsort((keys[:, 2], keys[:, 1], keys[:, 0]))
    sk = keys[order]
    change = np.any(np.diff(sk, axis=0) != 0, axis=1)
    bounds = np.concatenate([[0], np.nonzero(change)[0] + 1, [len(P)]])
    labels = np.empty(len(P), dtype=np.int16)
    for a, b in zip(bounds[:-1], bounds[1:]):
        idx = order[a:b]
        Q = P[idx]
        labels[idx] = tree.evaluate(root, Q, tree.Box.around(Q, 0.5)).m
    return labels
