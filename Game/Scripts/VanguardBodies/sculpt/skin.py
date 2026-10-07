"""Skin weights from the sculpt: each vertex takes the bones of the part it lies on (a garment, of the body part
beneath it), then the weights are smoothed across the surface so joints bend in a blend, not a crease."""
import numpy as np

from .surface import labels_at

INFLUENCES = 4


def weights(obj, full_root, body_root, sculpt, bone_names, smoothing=14):
    mesh = obj.data
    n = len(mesh.vertices)
    co = np.zeros(n * 3, dtype=np.float32)
    mesh.vertices.foreach_get("co", co)
    P = co.reshape(-1, 3)
    labels = labels_at(full_root, P)
    inherit = np.array([labels[i] < 0 or sculpt.parts[labels[i]].bones is None for i in range(n)])
    if inherit.any():
        labels[inherit] = labels_at(body_root, P[inherit])
    index = {name: i for i, name in enumerate(bone_names)}
    W = np.zeros((n, len(bone_names)), dtype=np.float32)
    for label in np.unique(labels):
        sel = labels == label
        part = sculpt.parts[label] if label >= 0 else None
        if part is None or part.bones is None:
            continue
        for bone, w in part.bones(P[sel]).items():
            W[sel, index[bone]] += w
    # Across the surface: each pass moves a vertex halfway to its neighbours' mean, so a seam between two bones'
    # parts widens into a blend about as wide as smoothing edges.
    edges = np.zeros(len(mesh.edges) * 2, dtype=np.int64)
    mesh.edges.foreach_get("vertices", edges)
    edges = edges.reshape(-1, 2)
    degree = np.bincount(edges.ravel(), minlength=n).astype(np.float32)
    for _ in range(smoothing):
        total = np.zeros_like(W)
        np.add.at(total, edges[:, 0], W[edges[:, 1]])
        np.add.at(total, edges[:, 1], W[edges[:, 0]])
        mean = total / np.maximum(degree, 1)[:, None]
        W = np.where(degree[:, None] > 0, 0.5 * W + 0.5 * mean, W)
    order = np.argsort(-W, axis=1)
    keep = np.zeros_like(W, dtype=bool)
    np.put_along_axis(keep, order[:, :INFLUENCES], True, axis=1)
    W = np.where(keep & (W > 0.01), W, 0.0)
    W = W / np.maximum(W.sum(axis=1, keepdims=True), 1e-9)
    return W


def assign(obj, W, bone_names):
    for name in bone_names:
        if name not in obj.vertex_groups:
            obj.vertex_groups.new(name=name)
    for b, name in enumerate(bone_names):
        group = obj.vertex_groups[name]
        column = W[:, b]
        for value in np.unique(np.round(column[column > 0], 3)):
            idx = np.nonzero(np.round(column, 3) == value)[0].tolist()
            group.add(idx, float(value), "REPLACE")
