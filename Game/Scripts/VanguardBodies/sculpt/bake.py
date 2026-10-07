"""Bakes a game mesh's textures from the sculpt it was made from: every texel's point on the low mesh is moved onto
the full-detail surface along the distance field, which gives its true normal (a tangent-space normal map), its
material (the base colour and what glows), and how open it is to the sky (ambient occlusion). No ray casting: the
field answers each directly."""
import math
import time

import bpy
import numpy as np

from . import sdf, tree

# Distances along the normal the occlusion samples (cm), and how much each counts.
AO_STEPS = np.array([0.4, 0.9, 1.8, 3.5, 7.0], dtype=np.float32)
AO_WEIGHTS = np.array([0.5, 0.3, 0.18, 0.1, 0.06], dtype=np.float32)
# The tetrahedron the gradient is sampled on, and its size (cm).
TETRA = np.array([[1, -1, -1], [-1, -1, 1], [-1, 1, -1], [1, 1, 1]], dtype=np.float32)
GRADIENT_STEP = 0.02
# Texels the islands are grown by into the empty texture around them (for filtering and mip levels).
DILATE = 12


def rasterise(obj, size):
    """Every covered texel's position, normal and tangent frame on obj (positions in cm): arrays over texels, and
    their pixel (x, y)."""
    mesh = obj.data
    mesh.calc_loop_triangles()
    mesh.calc_tangents()
    nloops = len(mesh.loops)
    uv = np.zeros(nloops * 2, dtype=np.float32)
    mesh.uv_layers.active.data.foreach_get("uv", uv)
    uv = uv.reshape(-1, 2)
    normals = np.zeros(nloops * 3, dtype=np.float32)
    mesh.corner_normals.foreach_get("vector", normals)
    normals = normals.reshape(-1, 3)
    tangents = np.zeros(nloops * 3, dtype=np.float32)
    mesh.loops.foreach_get("tangent", tangents)
    tangents = tangents.reshape(-1, 3)
    signs = np.zeros(nloops, dtype=np.float32)
    mesh.loops.foreach_get("bitangent_sign", signs)
    loop_vert = np.zeros(nloops, dtype=np.int64)
    mesh.loops.foreach_get("vertex_index", loop_vert)
    co = np.zeros(len(mesh.vertices) * 3, dtype=np.float32)
    mesh.vertices.foreach_get("co", co)
    co = co.reshape(-1, 3)
    tris = np.zeros(len(mesh.loop_triangles) * 3, dtype=np.int64)
    mesh.loop_triangles.foreach_get("loops", tris)
    tris = tris.reshape(-1, 3)
    out_xy, out_p, out_n, out_t, out_s = [], [], [], [], []
    for tri in tris:
        t_uv = uv[tri] * size - 0.5
        lo = np.floor(t_uv.min(axis=0)).astype(int)
        hi = np.ceil(t_uv.max(axis=0)).astype(int)
        lo = np.clip(lo, 0, size - 1)
        hi = np.clip(hi, 0, size - 1)
        xs, ys = np.meshgrid(np.arange(lo[0], hi[0] + 1), np.arange(lo[1], hi[1] + 1))
        px = np.stack([xs.ravel(), ys.ravel()], axis=1).astype(np.float32)
        a, b, c = t_uv
        v0, v1 = b - a, c - a
        den = v0[0] * v1[1] - v1[0] * v0[1]
        if abs(den) < 1e-12:
            continue
        d = px - a
        w1 = (d[:, 0] * v1[1] - v1[0] * d[:, 1]) / den
        w2 = (v0[0] * d[:, 1] - d[:, 0] * v0[1]) / den
        w0 = 1 - w1 - w2
        # A little past the edges, so a texel a seam halves is covered from both sides.
        tol = -0.6 / max(np.linalg.norm(v0), np.linalg.norm(v1), 1.0)
        inside = (w0 >= tol) & (w1 >= tol) & (w2 >= tol)
        if not inside.any():
            continue
        w = np.stack([w0[inside], w1[inside], w2[inside]], axis=1)
        out_xy.append(px[inside].astype(np.int32))
        out_p.append(w @ co[loop_vert[tri]])
        out_n.append(w @ normals[tri])
        out_t.append(w @ tangents[tri])
        out_s.append(np.full(inside.sum(), signs[tri[0]], dtype=np.float32))
    xy = np.concatenate(out_xy)
    # A texel covered twice (an edge) keeps the first triangle's.
    keys = xy[:, 1].astype(np.int64) * size + xy[:, 0]
    _, first = np.unique(keys, return_index=True)
    return (xy[first], np.concatenate(out_p)[first], np.concatenate(out_n)[first], np.concatenate(out_t)[first],
            np.concatenate(out_s)[first])


def _bricked(root, P, fn, cell=6.0):
    """fn(root, Q, box) over P grouped into cells, so each group evaluates only the parts near it."""
    keys = np.floor(P / cell).astype(np.int64)
    order = np.lexsort((keys[:, 2], keys[:, 1], keys[:, 0]))
    sk = keys[order]
    change = np.any(np.diff(sk, axis=0) != 0, axis=1)
    bounds = np.concatenate([[0], np.nonzero(change)[0] + 1, [len(P)]])
    results = None
    for a, b in zip(bounds[:-1], bounds[1:]):
        idx = order[a:b]
        Q = P[idx]
        out = fn(root, Q, tree.Box.around(Q, 0.5))
        if results is None:
            results = [np.empty((len(P),) + np.shape(o)[1:], dtype=np.asarray(o).dtype) for o in out]
        for r, o in zip(results, out):
            r[idx] = o
    return results


def _field(root, Q, box):
    f = tree.evaluate(root, Q.astype(np.float32), box)
    return (f.d, f.m)


def _distance_and_gradient(root, Q, box):
    """The distance at Q and its gradient (tetrahedral differences)."""
    h = GRADIENT_STEP
    d = tree.evaluate(root, Q.astype(np.float32), box.grown(h * 2))
    grad = np.zeros_like(Q, dtype=np.float32)
    for corner in TETRA:
        dv = tree.evaluate(root, (Q + corner * h).astype(np.float32), box.grown(h * 2)).d
        grad += corner[None, :] * dv[:, None]
    norm = np.linalg.norm(grad, axis=1, keepdims=True)
    return (d.d, d.m, grad / np.maximum(norm, 1e-9))


def bake(obj, root, sculpt, size, log=print):
    """The baked textures of obj from root: (base colour rgb linear, mask rgb [ao, glow, opacity], normal rgb encoded),
    each (size, size, 3) float32, rows from the bottom as Blender stores images."""
    started = time.time()
    tree.MODE[0] = "detail"
    tree.REACH[0] = 1.0
    xy, P, N, T, S = rasterise(obj, size)
    log("bake: %d texels (%.1f s)" % (len(xy), time.time() - started))
    N = N / np.maximum(np.linalg.norm(N, axis=1, keepdims=True), 1e-9)
    # Onto the full-detail surface: two steps along the field's gradient.
    Q = P.astype(np.float32).copy()
    for _ in range(2):
        d, _m, g = _bricked(root, Q, _distance_and_gradient)
        Q = Q - g * d[:, None]
    d, label, G = _bricked(root, Q, _distance_and_gradient)
    log("bake: projected (%.1f s)" % (time.time() - started))
    # Where the low surface lies far from any detail (a gap the reduction bridged), its own normal stands.
    far = np.abs(d) > 0.5
    G[far] = N[far]
    # Occlusion: how much nearer the surface is than open space would be, out along the normal.
    ao = np.zeros(len(Q), dtype=np.float32)
    for step, weight in zip(AO_STEPS, AO_WEIGHTS):
        (dist, _l) = _bricked(root, Q + G * step, _field)
        ao += weight * np.clip(step - dist, 0.0, step) / step
    ao = np.clip(1.0 - ao, 0.0, 1.0)
    # How the surface turns: out along the normal a convex edge opens faster than a flat face.
    (near, _l) = _bricked(root, Q + G * 0.35, _field)
    convex = np.clip((near - 0.35) / 0.15, -1.0, 1.0)
    log("bake: occlusion (%.1f s)" % (time.time() - started))
    ctx = {"P": Q, "N": G, "ao": ao, "convex": convex, "label": label}
    colour = np.ones((len(Q), 3), dtype=np.float32)
    glow = np.zeros(len(Q), dtype=np.float32)
    opacity = np.ones(len(Q), dtype=np.float32)
    materials = {}
    for part in sculpt.parts:
        materials.setdefault(part.material.name, (part.material, []))[1].append(part.label)
    for name, (material, labels) in materials.items():
        sel = np.isin(label, labels)
        if not sel.any():
            continue
        sub = {k: (v[sel] if isinstance(v, np.ndarray) else v) for k, v in ctx.items()}
        colour[sel] = material.colour(sub) if callable(material.colour) else np.asarray(material.preview, dtype=np.float32)
        if material.glow is not None:
            glow[sel] = material.glow(sub)
        if material.opacity is not None:
            opacity[sel] = material.opacity(sub)
    # The normal in the low surface's tangent frame (MikkTSpace, as the engine computes it on import). The frame is
    # rounded far below a texel's precision first: Blender's tangents can differ in their last bits from one run to the
    # next, and a rebuild must write the same texture.
    T, N, G = (np.round(v.astype(np.float64), 5) for v in (T, N, G))
    T = T - N * np.sum(T * N, axis=1, keepdims=True)
    T = T / np.maximum(np.linalg.norm(T, axis=1, keepdims=True), 1e-9)
    B = np.cross(N, T) * S[:, None]
    tangent_normal = np.stack([np.sum(G * T, axis=1), np.sum(G * B, axis=1), np.sum(G * N, axis=1)], axis=1)
    images = {"colour": colour, "mask": np.stack([ao, glow, opacity], axis=1), "normal": tangent_normal * 0.5 + 0.5}
    out = {}
    for key, values in images.items():
        img = np.zeros((size, size, 3), dtype=np.float32)
        filled = np.zeros((size, size), dtype=bool)
        img[xy[:, 1], xy[:, 0]] = values
        filled[xy[:, 1], xy[:, 0]] = True
        out[key] = _dilate(img, filled, DILATE)
    log("bake: done (%.1f s)" % (time.time() - started))
    return out


def _dilate(img, filled, steps):
    """Grows the filled texels outward into the empty ones by averaging, steps texels."""
    img = img.copy()
    filled = filled.copy()
    for _ in range(steps):
        total = np.zeros_like(img)
        count = np.zeros(filled.shape, dtype=np.float32)
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            shifted = np.roll(np.roll(img * filled[:, :, None], dx, axis=1), dy, axis=0)
            weight = np.roll(np.roll(filled, dx, axis=1), dy, axis=0).astype(np.float32)
            total += shifted
            count += weight
        grow = (~filled) & (count > 0)
        img[grow] = total[grow] / count[grow][:, None]
        filled |= grow
    return img


def save(values, path, srgb):
    """Writes (size, size, 3) values to a PNG, encoded to sRGB when srgb."""
    size = values.shape[0]
    data = np.clip(values, 0.0, 1.0)
    if srgb:
        data = np.where(data <= 0.0031308, data * 12.92, 1.055 * np.power(data, 1.0 / 2.4) - 0.055)
    rgba = np.concatenate([data, np.ones((size, size, 1), dtype=np.float32)], axis=2)
    img = bpy.data.images.new(path.stem, size, size, alpha=False, float_buffer=False)
    img.colorspace_settings.name = "Non-Color"
    img.pixels[:] = rgba.ravel()
    img.filepath_raw = str(path)
    img.file_format = "PNG"
    img.save()
    bpy.data.images.remove(img)
