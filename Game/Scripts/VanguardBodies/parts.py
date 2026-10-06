"""The pieces every Vanguard body generator shares (ADR-064): a body of simple parts, colour and the pose maths."""
import math

import bmesh
from mathutils import Euler, Matrix, Vector

# Sides of a round part.
SEGMENTS = 10


class Body:
    """One bmesh of parts, each rigidly weighted to a bone and coloured through vertex colours."""

    def __init__(self, bone_names):
        self.bm = bmesh.new()
        self.deform = self.bm.verts.layers.deform.verify()
        self.color = self.bm.loops.layers.color.new("Col")
        self.uv = self.bm.loops.layers.uv.new("UVMap")
        self.groups = {name: index for index, name in enumerate(bone_names)}

    def _finish(self, verts, bone, color, glow):
        group = self.groups[bone]
        faces = set()
        for vert in verts:
            vert[self.deform][group] = 1.0
            faces.update(vert.link_faces)
        rgba = (color[0], color[1], color[2], 1.0 if glow else 0.0)
        for face in faces:
            # UVs project each face onto the plane it faces most, a metre to a tile, for later textures.
            normal = [abs(component) for component in face.normal]
            across = [axis for axis in range(3) if axis != normal.index(max(normal))]
            for loop in face.loops:
                loop[self.color] = rgba
                loop[self.uv].uv = (loop.vert.co[across[0]] / 100.0, loop.vert.co[across[1]] / 100.0)

    @staticmethod
    def _frame(start, end):
        """A matrix placing a unit shape's +Z along start→end, centred between them."""
        axis = end - start
        length = max(axis.length, 0.01)
        rotation = Vector((0, 0, 1)).rotation_difference(axis.normalized()).to_matrix().to_4x4()
        return Matrix.Translation((start + end) / 2) @ rotation, length

    def limb(self, bone, start, end, radius_start, radius_end, color, glow=False, segments=SEGMENTS):
        frame, length = self._frame(start, end)
        verts = bmesh.ops.create_cone(self.bm, cap_ends=True, cap_tris=False, segments=segments, radius1=radius_start,
                                      radius2=radius_end, depth=length, matrix=frame)["verts"]
        self._finish(verts, bone, color, glow)

    def ball(self, bone, center, radius, color, glow=False, scale=(1, 1, 1), segments=SEGMENTS):
        matrix = Matrix.Translation(center) @ Matrix.Diagonal((*scale, 1.0))
        verts = bmesh.ops.create_uvsphere(self.bm, u_segments=segments, v_segments=max(3, segments * 7 // 10), radius=radius, matrix=matrix)["verts"]
        self._finish(verts, bone, color, glow)

    def blob(self, bone, center, axes, color, glow=False, segments=6):
        """An ellipsoid at center whose three half-axes are the vectors axes, turned any way: a flake, a clump."""
        basis = Matrix((axes[0], axes[1], axes[2])).transposed().to_4x4()
        verts = bmesh.ops.create_uvsphere(self.bm, u_segments=segments, v_segments=max(3, segments * 2 // 3), radius=1.0,
                                          matrix=Matrix.Translation(center) @ basis)["verts"]
        self._finish(verts, bone, color, glow)

    def shingles(self, bone, start, end, radius, colors, rng, count=8, lift=0.35):
        """Overlapping rounded flakes laid around start→end, facing out, a share of them lifting away: flaking sheets
        of drying sediment, or scales (seeded)."""
        frame, length = self._frame(start, end)
        along = frame.to_quaternion()
        axis = (end - start).normalized()
        for index in range(count):
            angle = index / count * math.tau + rng.uniform(-0.35, 0.35)
            share = (index % 3 + 0.5) / 3 + rng.uniform(-0.12, 0.12)
            out = along @ Vector((math.cos(angle), math.sin(angle), 0.0))
            peel = rng.uniform(0.0, lift)
            # Its face turns out from the core, tipping further out the more it peels.
            normal = (out + axis * -peel).normalized()
            tangent = axis.cross(normal).normalized()
            up = normal.cross(tangent).normalized()
            size = radius * rng.uniform(0.45, 0.65)
            self.blob(bone, start.lerp(end, share) + out * radius * (0.85 + peel * 0.4),
                      (normal * radius * 0.1, tangent * size, up * max(size, length * rng.uniform(0.3, 0.45))),
                      colors[rng.randrange(len(colors))])

    def box(self, bone, center, size, color, glow=False, rotation=None):
        matrix = Matrix.Translation(center) @ (rotation.to_matrix().to_4x4() if rotation else Matrix.Identity(4)) @ Matrix.Diagonal((*size, 1.0))
        verts = bmesh.ops.create_cube(self.bm, size=1.0, matrix=matrix)["verts"]
        self._finish(verts, bone, color, glow)

    def slab(self, bone, start, end, width, depth, color, glow=False, roll=0.0):
        """A box from start to end: width across, depth front to back, turned roll degrees about its length."""
        frame, length = self._frame(start, end)
        matrix = frame @ Matrix.Rotation(math.radians(roll), 4, "Z") @ Matrix.Diagonal((depth, width, length, 1.0))
        verts = bmesh.ops.create_cube(self.bm, size=1.0, matrix=matrix)["verts"]
        self._finish(verts, bone, color, glow)

    def rocks(self, bone, start, end, radius_start, radius_end, colors, rng, chunks=3):
        """Irregular stone along start→end: rough boxes, each turned, sized and coloured a little differently (seeded)."""
        frame, length = self._frame(start, end)
        along = frame.to_quaternion()
        for index in range(chunks):
            share = (index + 0.5) / chunks
            radius = radius_start + (radius_end - radius_start) * share
            turn = Euler((rng.uniform(-0.45, 0.45), rng.uniform(-0.45, 0.45), rng.uniform(-math.pi, math.pi))).to_quaternion()
            size = (radius * rng.uniform(1.5, 2.0), radius * rng.uniform(1.5, 2.0), length / chunks * rng.uniform(1.1, 1.5))
            self.box(bone, start.lerp(end, share), size, colors[rng.randrange(len(colors))], rotation=along @ turn)

    def plates(self, bone, start, end, radius, color, rng, count=5, glow=False):
        """Overlapping flat plates laid around start→end, facing out, like flaking sheets or armour (seeded)."""
        frame, length = self._frame(start, end)
        along = frame.to_quaternion()
        for index in range(count):
            angle = index / count * math.tau + rng.uniform(-0.3, 0.3)
            share = rng.uniform(0.25, 0.75)
            out = along @ Vector((math.cos(angle), math.sin(angle), 0.0))
            facing = along @ Euler((0.0, 0.0, angle)).to_quaternion() @ Euler((0.0, rng.uniform(-0.25, 0.25), 0.0)).to_quaternion()
            size = (radius * 0.28, radius * rng.uniform(1.1, 1.5), length * rng.uniform(0.6, 0.95))
            self.box(bone, start.lerp(end, share) + out * radius * 0.9, size, color, glow, facing)


def two_bone(start, target, first, second, hint):
    """The joint of a two-bone limb (first then second long) from start reaching target, bent toward hint."""
    direction = target - start
    distance = min(direction.length, (first + second) * 0.999)
    direction.normalize()
    along = (first * first - second * second + distance * distance) / (2 * distance)
    out = math.sqrt(max(0.0, first * first - along * along))
    bend = (hint - direction * hint.dot(direction)).normalized()
    return start + direction * along + bend * out


def mix(a, b, share):
    return [a[i] * (1 - share) + b[i] * share for i in range(3)]


def local(rest, world_euler):
    """A rotation given about the armature's axes (X forward, Y left, Z up), as the bone's own rotation."""
    q_world = Euler(world_euler, "XYZ").to_quaternion()
    return rest.inverted() @ q_world @ rest


def forward_swing(angle):
    """A swing that carries a hanging limb forward (+X) by angle degrees. On a bone pointing ahead, it pitches it up."""
    return (0.0, -math.radians(angle), 0.0)


def lean(angle):
    """Tilts an upright bone (a spine, a head) forward by angle degrees: the same turn as forward_swing(-angle)."""
    return (0.0, math.radians(angle), 0.0)


def combine(*rotations):
    """Rotations about the armature's axes added together, as the poses take them."""
    return tuple(sum(axis) for axis in zip(*rotations))


def twist(angle):
    return (0.0, 0.0, math.radians(angle))


def roll_side(angle, sign):
    """Raises a hanging limb out to its side by angle degrees; sign is +1 for the left (+Y), -1 for the right."""
    return (math.radians(angle) * sign, 0.0, 0.0)


def ease(x):
    return x * x * (3 - 2 * x)
