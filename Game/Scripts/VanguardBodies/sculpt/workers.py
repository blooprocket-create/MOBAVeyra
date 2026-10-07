"""A sculpt evaluated in worker processes (ADR-069 §2). A field is numpy driven by Python, and Blender runs one
interpreter, so meshing, labelling and baking spread across processes, each holding the sculpt rebuilt from its
model's script. A model's build is deterministic (every random draw is seeded), so each worker's sculpt is the
generator's; and a point's result depends only on its own cell or brick, so it is the same however many share the
work. The task functions here import no Blender module, so a worker runs in Blender's own Python without Blender."""
import importlib
import multiprocessing
import os
import sys

import numpy as np

from . import tree

# The worker processes a build evaluates in: a machine's setting (VEYRA_SCULPT_PROCESSES), half its logical cores by
# default; 1 evaluates in the generator's own process.
PROCESSES = int(os.environ.get("VEYRA_SCULPT_PROCESSES", max(1, (os.cpu_count() or 2) // 2)))
# The tetrahedron a gradient is sampled on, and its size (cm).
TETRA = np.array([[1, -1, -1], [-1, -1, 1], [-1, 1, -1], [1, 1, 1]], dtype=np.float32)
GRADIENT_STEP = 0.02
# A worker's sculpt by name: "root", the whole, and "body", the skin beneath the garments.
_ROOTS = {}


# ---------------------------------------------------------------------------------------------- tasks
def field(root, Q, box):
    """The distance and label at Q."""
    f = tree.evaluate(root, Q.astype(np.float32), box)
    return (f.d, f.m)


def labels(root, Q, box):
    """The label of the part whose surface is nearest each of Q."""
    return (tree.evaluate(root, Q.astype(np.float32), box).m,)


def distance_and_gradient(root, Q, box):
    """The distance at Q, its label and its gradient (tetrahedral differences)."""
    h = GRADIENT_STEP
    d = tree.evaluate(root, Q.astype(np.float32), box.grown(h * 2))
    grad = np.zeros_like(Q, dtype=np.float32)
    for corner in TETRA:
        dv = tree.evaluate(root, (Q + corner * h).astype(np.float32), box.grown(h * 2)).d
        grad += corner[None, :] * dv[:, None]
    norm = np.linalg.norm(grad, axis=1, keepdims=True)
    return (d.d, d.m, grad / np.maximum(norm, 1e-9))


def brick(root, start, voxel, size, band, reach):
    """A level set's brick of size voxels a side from start (in voxels): its distances clipped to band, or None where
    the surface cannot cross it (its centre further than reach from the surface)."""
    corner = start.astype(np.float32) * voxel
    box = tree.Box(corner, corner + size * voxel)
    if root.bounds.gap(box) > band:
        return None
    if abs(float(tree.evaluate(root, (corner + size * voxel * 0.5)[None, :], box).d[0])) > reach:
        return None
    axis = np.arange(size, dtype=np.float32)
    local = np.stack(np.meshgrid(axis, axis, axis, indexing="ij"), axis=-1).reshape(-1, 3)
    return np.clip(tree.evaluate(root, corner + local * voxel, box).d, -band, band).astype(np.float32).reshape(size, size, size)


# ---------------------------------------------------------------------------------------------- the pool
def _plain(value):
    """value with Blender's vectors and numpy's arrays as tuples, so it crosses to a worker without Blender."""
    if isinstance(value, dict):
        return {key: _plain(item) for key, item in value.items()}
    if isinstance(value, (list, tuple)):
        return type(value)(_plain(item) for item in value)
    if hasattr(value, "to_tuple") or isinstance(value, np.ndarray):
        return tuple(float(x) for x in value)
    return value


def _start(recipe):
    script = importlib.import_module("VanguardBodies.models." + recipe["script"])
    root, info = script.build(tree.Sculpt(), recipe["layout"], recipe["dims"], recipe["spec"])
    _ROOTS.update({"root": root, "body": info["body"]})


def _run(task):
    fn, name, mode, reach, args = task
    tree.MODE[0], tree.REACH[0] = mode, reach
    return fn(_ROOTS[name], *args)


class Pool:
    """Worker processes each holding the sculpt its recipe builds ({script, layout, dims, spec}, as model.build takes
    them). While open (a with block), tree.spread runs here the tasks on a node the pool has adopted: the generator's
    own copy of a sculpt the workers hold by name."""

    def __init__(self, recipe):
        self.recipe, self.pool, self.names = _plain(recipe), None, {}

    def adopt(self, name, node):
        """node (the generator's) is the one the workers hold as name ("root" or "body")."""
        self.names[id(node)] = (name, node)

    def name_of(self, node):
        held = self.names.get(id(node))
        return held[0] if held is not None and held[1] is node else None

    def __enter__(self):
        if PROCESSES > 1:
            # Blender names its main script by a path a worker cannot run; a spawned worker would try to, so the pool
            # starts with the main module unnamed (workers import only what their tasks need).
            main = sys.modules["__main__"]
            hidden = main.__dict__.pop("__file__", None)
            try:
                self.pool = multiprocessing.get_context("spawn").Pool(PROCESSES, _start, (self.recipe,))
            finally:
                if hidden is not None:
                    main.__file__ = hidden
            tree.POOL[0] = self
        return self

    def __exit__(self, *_exc):
        tree.POOL[0] = None
        if self.pool is not None:
            self.pool.terminate()
            self.pool.join()
        return False

    def map(self, fn, name, args_list):
        mode, reach = tree.MODE[0], tree.REACH[0]
        chunk = max(1, len(args_list) // (PROCESSES * 8))
        return self.pool.map(_run, [(fn, name, mode, reach, args) for args in args_list], chunk)
