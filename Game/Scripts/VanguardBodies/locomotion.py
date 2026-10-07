"""IK-authored locomotion (ADR-069 §6): a leg's pose solved from where its foot must be, so a planted foot stays where
it was set down while the body moves over it, and a swinging foot arcs to its next step. The clips stay forward
kinematics everywhere else (arms swing, the chest turns); only the legs, which hold to the ground, are solved.

Rotations come out as the archetypes' poses take them: each bone's rotation about the armature's axes, relative to its
parent, as Euler angles (VanguardBodies.parts.local)."""
import math

from mathutils import Matrix, Quaternion, Vector


def two_bone(root, joint_rest, end_rest, target, pole, stretch=0.0):
    """Where the joint and end of a two-bone limb lie when its end reaches target from root, bending toward pole; each
    bone may stretch by stretch of its length before the end falls short."""
    upper = (joint_rest - root).length
    lower = (end_rest - joint_rest).length
    to_target = target - root
    distance = max(to_target.length, 1e-6)
    along = to_target / distance
    give = min(max(distance / (upper + lower), 1.0), 1.0 + stretch)
    a, b = upper * give, lower * give
    reach = min(distance, (a + b) * 0.9999)
    cosine = max(-1.0, min(1.0, (a * a + reach * reach - b * b) / (2.0 * a * reach)))
    bend = (pole - root) - along * (pole - root).dot(along)
    if bend.length < 1e-6:
        bend = Vector((1.0, 0.0, 0.0)) - along * along.x
    bend.normalize()
    joint = root + along * (cosine * a) + bend * (math.sqrt(max(0.0, 1.0 - cosine * cosine)) * a)
    return joint, root + along * reach


def _frame(direction, reference):
    """A rotation whose Y axis is direction and whose X axis leans toward reference (Blender bones run along Y)."""
    y = direction.normalized()
    x = reference - y * reference.dot(y)
    if x.length < 1e-6:
        x = Vector((1.0, 0.0, 0.0)) - y * y.x
    x.normalize()
    z = x.cross(y)
    return Matrix((x, y, z)).transposed().to_quaternion()


def leg_pose(rest, hip, foot_target, foot_pitch, pole, parent_world=Quaternion(), stretch=0.0):
    """The thigh's, calf's and foot's rotations (Euler, about the armature's axes, each relative to its parent) that
    put the ankle at foot_target with the knee toward pole and the foot pitched foot_pitch degrees (toes down positive).
    rest: the leg's rest positions {"hip", "knee", "ankle", "toe"} in the armature's space, before any pose; hip: where
    the hip is now (after the pelvis's lift); parent_world: the pelvis's own rotation now."""
    knee, ankle = two_bone(hip, hip + (rest["knee"] - rest["hip"]), hip + (rest["ankle"] - rest["hip"]), foot_target, pole, stretch)
    # Each bone's world rotation away from rest, keeping the knee's plane (the bones' reference is the pole's side).
    side = (pole - hip).normalized()
    thigh_world = _frame(knee - hip, side) @ _frame(rest["knee"] - rest["hip"], side).inverted()
    calf_world = _frame(ankle - knee, side) @ _frame(rest["ankle"] - rest["knee"], side).inverted()
    # The foot: flat on the ground, then pitched about the leg's sideways axis.
    across = (rest["toe"] - rest["ankle"]).cross(Vector((0.0, 0.0, 1.0)))
    foot_world = Quaternion(across.normalized() if across.length > 1e-6 else Vector((0.0, 1.0, 0.0)), math.radians(-foot_pitch))
    thigh = parent_world.inverted() @ thigh_world
    calf = thigh_world.inverted() @ calf_world
    foot = calf_world.inverted() @ foot_world
    return {"thigh": tuple(thigh.to_euler("XYZ")), "calf": tuple(calf.to_euler("XYZ")), "foot": tuple(foot.to_euler("XYZ"))}


def run_feet(t, stride, sole, phase_offset=0.5, contact=0.3, lift=0.22, leg=80.0):
    """Where each foot is (forward of its rest, up, pitch degrees toes down) in a Run cycle at t from 0 to 1, for a run
    that carries the body stride in one cycle (two steps). A foot is planted for contact of the cycle, sliding back
    exactly as far as the body moves in that time (stride x contact), so it stays put on the ground; then it lifts and
    swings ahead to its next step, its arc peaking at lift of the leg's length. The right foot is half a cycle behind."""
    reach = stride * contact
    feet = {}
    for side, offset in (("l", 0.0), ("r", phase_offset)):
        p = (t + offset) % 1.0
        if p < contact:
            # Planted: from heel strike (toes up a little) to push-off (toes down), flat on the ground between.
            share = p / contact
            forward = reach * (0.5 - share)
            up = 0.0
            pitch = -8.0 * max(0.0, 1.0 - share * 4.0) + 24.0 * max(0.0, share - 0.7) / 0.3
        else:
            # Swinging: eased forward from behind to ahead, lifted in an arc, the toes trailing down as it leaves.
            share = (p - contact) / (1.0 - contact)
            eased = 0.5 - 0.5 * math.cos(share * math.pi)
            forward = reach * (-0.5 + eased)
            up = lift * leg * math.sin(share * math.pi) ** 1.3
            pitch = 30.0 * (1.0 - share) ** 2 - 8.0 * share
        feet[side] = (forward, sole + up, pitch)
    return feet


def run_pelvis(t, leg, contact=0.3):
    """The pelvis's rise in a Run cycle: a runner's crouch, lowest as each foot takes the weight mid-contact and highest
    in the flight between steps (two bobs a cycle), so a planted leg reaches its ground."""
    p = (t % 0.5) / 0.5
    return leg * (-0.04 - 0.018 * math.cos((p - contact) * math.tau))