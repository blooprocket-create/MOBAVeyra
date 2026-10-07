"""Turns a sculpt's distance tree into a closed surface: the tree is evaluated brick by brick over the space it fills,
only in bricks the surface may cross, into an OpenVDB level set that OpenVDB meshes."""
import math
import time

import numpy as np
import openvdb

from . import tree, workers

# A brick's edge in voxels.
BRICK = 32
# The level set's half width in voxels (the band the surface is resolved within).
BAND_VOXELS = 3.0
# A brick is skipped when its centre lies further from the surface than its half diagonal times this, plus the band:
# blends and displacements bend distances a little from true ones.
SAFETY = 1.5


def level_set(root, voxel, pad=2.0, log=print):
    """The level set of root at voxel centimetres: (grid, index origin)."""
    band = BAND_VOXELS * voxel
    tree.REACH[0] = band
    lo = root.bounds.lo - pad
    hi = root.bounds.hi + pad
    origin = np.floor(lo / voxel).astype(np.int64)
    extent = np.ceil((hi - lo) / voxel).astype(np.int64) + 1
    bricks = (extent + BRICK - 1) // BRICK
    grid = openvdb.FloatGrid(band)
    grid.transform = openvdb.createLinearTransform(voxelSize=voxel)
    grid.gridClass = openvdb.GridClass.LEVEL_SET
    half = BRICK * voxel * 0.5
    reach = half * math.sqrt(3.0) * SAFETY + band
    started, total = time.time(), int(np.prod(bricks))
    starts = [origin + np.array([bi, bj, bk]) * BRICK for bi in range(bricks[0]) for bj in range(bricks[1]) for bk in range(bricks[2])]
    # Evaluated in the workers while a pool is open; written into the grid in one order, so the grid is the same
    # however many share the work.
    kept = 0
    values_of = tree.spread(workers.brick, root, [(start, voxel, BRICK, band, reach) for start in starts])
    for start, values in zip(starts, values_of):
        if values is not None:
            grid.copyFromArray(values, ijk=tuple(int(v) for v in start), tolerance=0.0)
            kept += 1
    grid.signedFloodFill()
    log("level set: %d of %d bricks, %.1f s, %d active voxels" % (kept, total, time.time() - started, grid.activeVoxelCount()))
    return grid


def polygons(grid, adaptivity=0.0):
    """The level set's zero surface: (points (n, 3) cm, triangles (t, 3), quads (q, 4))."""
    points, triangles, quads = grid.convertToPolygons(isovalue=0.0, adaptivity=adaptivity)
    return np.asarray(points, dtype=np.float64), np.asarray(triangles, dtype=np.int64), np.asarray(quads, dtype=np.int64)
