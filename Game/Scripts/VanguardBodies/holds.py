"""A weapon held in both hands (ADR-069 §6). Its body rests empty-handed in the A pose, the weapon on its own bone in the
right hand, and each clip reaches both arms to where the weapon is held, by inverse kinematics: carried low across the
body as it stands, runs, casts, recalls or is struck; raised to the sight only to attack; let fall as it dies.

A hold is where the weapon is: its right grip and how it is turned from its aim. The carry rides the chest, so the
weapon moves with the body that carries it; the aim holds the sight where the layout put it, true whatever the chest
does. Rotations are as the archetypes' poses take them: about the armature's axes, each relative to its parent's."""
import math

from mathutils import Euler, Matrix, Quaternion, Vector

from . import locomotion
from .parts import ease, forward_swing, roll_side

ARMS = {side: ("clavicle_" + side, "upperarm_" + side, "lowerarm_" + side, "hand_" + side) for side in ("l", "r")}
# Where a straight resting arm's elbow points: back, the way it folds.
ELBOW_BACK = Vector((-1.0, 0.0, 0.0))
# How far a pole's point stands from the shoulder along its direction (cm): only its side of the arm counts.
POLE_REACH = 100.0
# The share of a death before the hands let the weapon go, and how far through it the weapon has fallen flat.
RELEASE_SHARE, FALLEN_SHARE = 0.1, 0.55
# How a fallen weapon lies, from its aim: turned across the body (yaw) and onto its side (roll), degrees.
FALLEN_YAW, FALLEN_ROLL = 70.0, 85.0
# How a fired weapon kicks: up (degrees) and back (cm) at the recoil's height.
RECOIL_PITCH, RECOIL_BACK = 6.0, 2.5


def _frame(y, x_hint):
    """A rotation whose Y axis is y and whose X axis leans toward x_hint (Blender bones run along Y)."""
    y = y.normalized()
    x = x_hint - y * x_hint.dot(y)
    x.normalize()
    return Matrix((x, y, x.cross(y))).transposed()


def fk(bones, L, pose, lift_bone, lift):
    """Every bone's rotation away from its rest and its head, under pose with the lift bone raised by lift:
    ({bone: Quaternion}, {bone: Vector}), about the armature's axes."""
    turned, head = {}, {}
    for name, parent in bones:
        q = Euler(pose.get(name, (0.0, 0.0, 0.0)), "XYZ").to_quaternion()
        rest = Vector(L[name][0])
        if parent is None:
            turned[name], head[name] = q, rest
        else:
            turned[name] = turned[parent] @ q
            head[name] = head[parent] + turned[parent] @ (rest - Vector(L[parent][0]))
        if name == lift_bone:
            head[name] = head[name] + (turned[parent] if parent else Quaternion()) @ Vector((0.0, 0.0, lift))
    return turned, head


def arm_pose(L, turned, head, side, wrist, hand_turn, pole):
    """The upper arm's, forearm's and hand's rotations putting side's wrist at wrist, its elbow toward pole (a
    direction), its hand turned hand_turn from its rest; with turned and head (fk) the body's pose above the arm."""
    clavicle, upper, lower, hand = ARMS[side]
    shoulder = head[upper]
    rest_s, rest_e, rest_w = (Vector(L[bone][0]) for bone in (upper, lower, hand))
    elbow, reached = locomotion.two_bone(shoulder, shoulder + (rest_e - rest_s), shoulder + (rest_w - rest_s), wrist, shoulder + pole * POLE_REACH)
    # The elbow's side of the shoulder-to-wrist line: the hinge the forearm folds about, as it was at rest.
    reach = reached - shoulder
    out = (elbow - shoulder) - reach * ((elbow - shoulder).dot(reach) / max(reach.length_squared, 1e-9))
    out = out if out.length > 1e-6 else pole.copy()
    upper_turn = (_frame(elbow - shoulder, out) @ _frame(rest_e - rest_s, ELBOW_BACK).transposed()).to_quaternion()
    lower_turn = (_frame(reached - elbow, out) @ _frame(rest_w - rest_e, ELBOW_BACK).transposed()).to_quaternion()
    return {upper: tuple((turned[clavicle].inverted() @ upper_turn).to_euler("XYZ")),
            lower: tuple((upper_turn.inverted() @ lower_turn).to_euler("XYZ")),
            hand: tuple((lower_turn.inverted() @ hand_turn).to_euler("XYZ"))}


def prepare(L, aimed, grips, poles, aim, height):
    """What a body's holds need, from its rest layout L and the arms' aim (aimed: each arm bone's head and tail at the
    aim; grips and poles each hand's grip and elbow direction there; aim: the kit's aimBore and its carry, in shares
    of height): a plain dict, so it crosses to a sculpt's workers.

    A model builds each hand, and the weapon in the right, as they are at the aim, then moves them to its rest wrist
    unturned (each hand's "offset"): at rest it holds the weapon level at its side, and a hold turns each hand just as
    it turns the weapon. So the weapon never points down through the ground the body stands on."""
    out = {"grip": {}, "wrist": {}, "pole": {}, "offset": {}}
    for side in ("l", "r"):
        bone = "hand_" + side
        out["grip"][side] = tuple(grips[side])
        out["wrist"][side] = tuple(aimed[bone][0])
        out["pole"][side] = tuple(Vector(poles[side]).normalized())
        out["offset"][side] = tuple(Vector(L[bone][0]) - Vector(aimed[bone][0]))
    carry = aim["carry"]
    grip = Vector((height * carry["forwardShare"], -height * carry["rightShare"], height * carry["heightShare"]))
    out["carry"] = {"grip": tuple(grip), "turn": tuple((Euler((0.0, 0.0, math.radians(carry["yaw"])), "XYZ").to_quaternion()
                                                                   @ Euler((0.0, math.radians(carry["pitch"]), 0.0), "XYZ").to_quaternion()))}
    out["head"] = (aim.get("headPitch", 0.0), aim.get("headRoll", 0.0))
    return out


def _place(grip, turn, holds, point):
    """point (at the aim) where a hold whose right grip is grip, turned turn from the aim, carries it."""
    return grip + turn @ (point - Vector(holds["grip"]["r"]))


def schedule(name, t, melee):
    """How far raised to the aim (0 carried, 1 at the sight), the recoil, and whether the hands still hold at t."""
    if name == "AttackWindup" and not melee:
        return ease(t), 0.0, True
    if name == "AttackStrike" and not melee:
        recoil = math.sin(min(1.0, t / 0.4) * math.pi)
        return 1.0 - ease(max(0.0, (t - 0.35) / 0.65)), recoil, True
    if name == "Death":
        return 0.0, 0.0, t < RELEASE_SHARE
    return 0.0, 0.0, True


def pose_arms(pose, name, t, melee, d, bones, lift_bone, lift):
    """Both arms into pose for clip name at t: reaching the weapon where its hold is, or letting it go as the body dies.
    The weapon's own bone (prop_r) turns it to lie flat as it falls. The head bows to the sight as the weapon rises."""
    holds = d["holds"]
    raised, recoil, holding = schedule(name, t, melee)
    for bone in [bone for bone in pose if bone.startswith(("upperarm_", "lowerarm_", "hand_", "prop_"))]:
        del pose[bone]
    pitch, roll = holds["head"]
    if raised > 0.0:
        pose["head"] = tuple(a + b for a, b in zip(pose.get("head", (0.0, 0.0, 0.0)), (math.radians(-roll * raised), math.radians(pitch * raised), 0.0)))
    if not holding:
        # Let go: the arms fall open with the body, and the weapon tips out of the hand to lie flat.
        k = ease(min(1.0, t / 0.6))
        for side, sign in (("l", 1), ("r", -1)):
            pose["upperarm_" + side] = tuple(a + b for a, b in zip(roll_side(55 * k, sign), forward_swing(-20 * k)))
            pose["lowerarm_" + side] = forward_swing(25 * k)
        turned, _head = fk(bones, d["layout"], pose, lift_bone, lift)
        # The weapon rests turned as at the aim (its hand moved there unturned): fallen, it lies across and on its side.
        fallen = Euler((0.0, 0.0, math.radians(FALLEN_YAW)), "XYZ").to_quaternion() @ Euler((math.radians(FALLEN_ROLL), 0.0, 0.0), "XYZ").to_quaternion()
        lie = turned["hand_r"].inverted() @ fallen
        share = ease(min(1.0, max(0.0, (t - RELEASE_SHARE) / (FALLEN_SHARE - RELEASE_SHARE))))
        pose["prop_r"] = tuple(Quaternion().slerp(lie, share).to_euler("XYZ"))
        return
    turned, head = fk(bones, d["layout"], pose, lift_bone, lift)
    chest, chest_rest = turned["spine_03"], Vector(d["layout"]["spine_03"][0])
    carry_grip, carry_turn = Vector(holds["carry"]["grip"]), Quaternion(holds["carry"]["turn"])
    kick = Euler((0.0, math.radians(-RECOIL_PITCH * recoil), 0.0), "XYZ").to_quaternion()
    aim_grip = Vector(holds["grip"]["r"]) - Vector((RECOIL_BACK * recoil, 0.0, 0.0))
    for side in ("r", "l"):
        wrist_aim = Vector(holds["wrist"][side])
        # Carried: on the chest, wherever it is now. At the sight: where the layout aimed it.
        carried = head["spine_03"] + chest @ (_place(carry_grip, carry_turn, holds, wrist_aim) - chest_rest)
        aimed = _place(aim_grip, kick, holds, wrist_aim)
        wrist = carried.lerp(aimed, raised)
        hand_turn = (chest @ carry_turn).slerp(kick, raised)
        pole = (chest @ carry_turn @ Vector(holds["pole"][side])).lerp(Vector(holds["pole"][side]), raised).normalized()
        pose.update(arm_pose(d["layout"], turned, head, side, wrist, hand_turn, pole))
