"""The rider archetype (ADR-064 §1): a humanoid seated on its mount, the two one silhouette. Raska rides Hound.

The rider is the humanoid's figure, seated: its upper body leans over the mount, its hands on the bars and its feet on
the pegs. The mount is a heavy machine on two great knobbled wheels that spin as it rides. Rider and mount are separate
chains under the root, so a crash can throw the rider clear of a machine falling on its side. Wherever the mount moves
the whole silhouette (a wheelie, a lunge, a jolt), the rider is posed to follow it. Nothing here decides gameplay: the
capsule stays the only collision and movement.
"""
import math

from mathutils import Euler, Vector

from . import humanoid
from .parts import ease, forward_swing, lean, mix, roll_side, twist, two_bone

# The mount's chain beside the rider's, both under the root. (name, parent)
BONES = [("root", None), ("mount", "root"), ("wheel_back", "mount"), ("wheel_front", "mount")] + [
    bone for bone in humanoid.BONES if bone[0] != "root"]
UPPER_BODY_BONE = "spine_01"
LIFT_BONE = "mount"
# Every bone a pose may move as well as turn: the mount, and the rider it carries or throws.
MOVED_BONES = ("mount", "pelvis")
GROUNDED = True
CAST_RELEASE_SHARE = humanoid.CAST_RELEASE_SHARE
# The share of a melee windup spent drawing back, as the humanoid's.
MELEE_COCK_SHARE = humanoid.MELEE_COCK_SHARE
# Knobs around each tyre, so its spin shows.
TYRE_KNOBS = 14
# How far a crashed mount rolls onto its side, in degrees.
CRASH_ROLL = 82.0
RUBBER = [0.05, 0.05, 0.05]
DARK = [0.08, 0.08, 0.09]


def layout(spec, capsule):
    """Every bone's head and tail in centimetres, facing +X with its left at +Y: the standing humanoid, seated."""
    mount = spec["mount"]
    # The mount fills the unit's footprint, so the rider keeps a figure's own shoulders rather than the capsule's.
    standing, d = humanoid.layout(spec, dict(capsule, capsuleRadius=0.0))
    full = d["full"]
    radius = full * mount["wheelRadiusShare"]
    width = full * mount["wheelWidthShare"]
    base = full * mount["wheelbaseShare"]
    seat_z = full * mount["seatHeightShare"]
    seat = Vector((-base * mount["seatShare"], 0.0, seat_z + d["torso"] * 0.08))
    turn = Euler((0.0, math.radians(mount["leanDegrees"]), 0.0)).to_matrix()
    hip = standing["pelvis"][0]

    def seated(point):
        return seat + turn @ (point - hip)

    L = {"root": (Vector((0, 0, 0)), Vector((0, 0, radius)))}
    back, front = Vector((-base / 2, 0, radius)), Vector((base / 2, 0, radius))
    # The mount turns about its rear axle, so a wheelie lifts the front.
    L["mount"] = (back, front)
    L["wheel_back"] = (back, back + Vector((0, 0, radius * 0.5)))
    L["wheel_front"] = (front, front + Vector((0, 0, radius * 0.5)))
    for name in ("pelvis", "spine_01", "spine_02", "spine_03", "neck_01", "head", "clavicle_l", "clavicle_r", "tail_01", "tail_02", "tail_03",
                 "cape_01", "cape_02", "cape_03"):
        L[name] = tuple(seated(point) for point in standing[name])
    arm = d["arm"]
    upper, lower, hand = arm * 0.48, arm * 0.40, arm * 0.12
    grips = {}
    for side, sign in (("l", 1.0), ("r", -1.0)):
        # Arms reach forward and down to the bars, elbows out.
        shoulder = seated(standing["upperarm_" + side][0])
        reach = Vector((1.0, sign * 0.12, -0.5)).normalized()
        wrist = shoulder + reach * (upper + lower) * 0.9
        grip = wrist + reach * hand
        elbow = two_bone(shoulder, wrist, upper, lower, Vector((0.0, sign * 0.6, -0.8)))
        L["upperarm_" + side] = (shoulder, elbow)
        L["lowerarm_" + side] = (elbow, wrist)
        L["hand_" + side] = (wrist, grip)
        L["prop_" + side] = (grip, grip + reach * arm * 0.15)
        grips[side] = grip
        # Thighs along the tank, knees out; shins down to the pegs; feet forward on them.
        thigh = (standing["thigh_" + side][1] - standing["thigh_" + side][0]).length
        calf = (standing["calf_" + side][1] - standing["calf_" + side][0]).length
        hip_point = seated(standing["thigh_" + side][0])
        knee = hip_point + Vector((0.88, sign * 0.18, -0.25)).normalized() * thigh
        hock = knee + Vector((-0.25, sign * 0.02, -0.97)).normalized() * calf
        L["thigh_" + side] = (hip_point, knee)
        L["calf_" + side] = (knee, hock)
        L["foot_" + side] = (hock, hock + Vector((d["leg"] * 0.22, 0.0, -d["leg"] * 0.03)))
    d = dict(d, length=base + radius * 2, wheel=radius, tyre=width, wheelbase=base, seatZ=seat_z, seat=seat,
             grips=grips, rest={name: points[0].copy() for name, points in L.items()},
             punch="l" if any(prop["kind"] == "bracer" and prop["hand"] == "left" for prop in spec["props"]) else "r")
    d["crashDrop"] = _crash_drop(d)
    return L, d


def _crash_drop(d):
    """How far the mount must drop (negative) or rise once rolled onto its side, so its lowest part meets the ground."""
    pivot = d["rest"]["mount"]
    roll = Euler((math.radians(CRASH_ROLL), 0.0, 0.0)).to_matrix()
    r, w, base = d["wheel"], d["tyre"], d["wheelbase"]
    corners = [Vector((x, y, z)) for x in (-base / 2, base / 2) for y in (-w / 2, w / 2) for z in (0.0, 2 * r)]
    corners += [d["grips"]["l"], d["grips"]["r"], Vector((-base * 0.55, -w * 1.2, r * 1.6)), Vector((-base * 0.55, w * 1.2, r * 1.6))]
    lowest = min((pivot + roll @ (corner - pivot)).z for corner in corners)
    return -lowest


def body(spec, L, d):
    body = humanoid.body(spec, L, d, BONES)
    mount = spec["mount"]
    paint, frame, metal, glow = mount["primary"], mount["secondary"], mount["metal"], spec["accent"]
    r, w, base = d["wheel"], d["tyre"], d["wheelbase"]
    seat_z = d["seatZ"]
    # The wheels: fat tyres ringed with knobs, hubs burning in the side's light, dark spokes across them.
    for bone in ("wheel_back", "wheel_front"):
        axle = L[bone][0]
        body.limb(bone, axle - Vector((0, w / 2, 0)), axle + Vector((0, w / 2, 0)), r * 0.93, r * 0.93, RUBBER, segments=16)
        for index in range(TYRE_KNOBS):
            angle = index / TYRE_KNOBS * math.tau
            out = Vector((math.cos(angle), 0.0, math.sin(angle)))
            body.box(bone, axle + out * r * 0.93, (r * 0.22, w * 1.04, r * 0.14), RUBBER, rotation=Euler((0.0, math.pi / 2 - angle, 0.0)))
        body.limb(bone, axle - Vector((0, w * 0.56, 0)), axle + Vector((0, w * 0.56, 0)), r * 0.5, r * 0.5, glow, glow=True, segments=12)
        for sign in (1, -1):
            face = axle + Vector((0, sign * w * 0.58, 0))
            for spoke in range(3):
                body.box(bone, face, (r * 0.95, 1.0, r * 0.12), DARK, rotation=Euler((0.0, spoke * math.pi / 3, 0.0)))
            body.ball(bone, face, r * 0.16, metal)
    # The engine between the wheels, its channels lit; a V of cylinders above it; a plate under it to take the hits;
    # crash bars standing out from its flanks, the machine's widest point.
    engine = Vector((base * 0.02, 0, r * 1.3))
    body.box("mount", engine, (base * 0.5, w * 1.5, r * 1.25), frame)
    for sign in (1, -1):
        body.box("mount", engine + Vector((0, sign * w * 0.76, -r * 0.18)), (base * 0.36, 1.0, r * 0.09), glow, glow=True)
        body.box("mount", engine + Vector((0, sign * w * 0.76, r * 0.18)), (base * 0.26, 1.0, r * 0.07), glow, glow=True)
        top, low, back = (Vector((base * 0.3, sign * w * 1.25, r * 1.95)), Vector((base * 0.3, sign * w * 1.25, r * 0.85)),
                          Vector((base * 0.02, sign * w * 1.1, r * 0.75)))
        body.limb("mount", top, low, r * 0.09, r * 0.09, metal)
        body.limb("mount", low, back, r * 0.09, r * 0.09, metal)
        body.limb("mount", top, Vector((base * 0.18, sign * w * 0.6, r * 2.3)), r * 0.09, r * 0.09, metal)
    for lean_x in (-1, 1):
        start = engine + Vector((lean_x * base * 0.07, 0, r * 0.45))
        body.limb("mount", start, start + Vector((lean_x * base * 0.12, 0, r * 0.75)), r * 0.3, r * 0.26, metal)
        body.limb("mount", start + Vector((lean_x * base * 0.1, 0, r * 0.62)), start + Vector((lean_x * base * 0.13, 0, r * 0.8)), r * 0.34, r * 0.34, frame)
    body.box("mount", Vector((base * 0.05, 0, r * 0.62)), (base * 0.46, w * 1.4, 3.0), metal)
    # The frame: a swingarm back to the rear axle and a spine up to the headstock.
    grip_l, grip_r = d["grips"]["l"], d["grips"]["r"]
    bars = Vector((grip_l.x, 0.0, grip_l.z))
    head_stock = bars + Vector((-r * 0.15, 0, -r * 0.5))
    for sign in (1, -1):
        body.limb("mount", Vector((-base / 2, sign * w * 0.62, r)), Vector((-base * 0.12, sign * w * 0.62, r * 1.1)), r * 0.1, r * 0.12, frame)
    body.limb("mount", Vector((-base * 0.3, 0, seat_z - r * 0.15)), head_stock, r * 0.16, r * 0.16, frame)
    # The tank, heavy and painted, plated on its flanks with a lit seam; the seat behind it.
    tank = Vector((base * 0.16, 0, seat_z + r * 0.4))
    body.ball("mount", tank, r * 0.85, paint, scale=(1.6, 1.15, 0.7))
    for sign in (1, -1):
        body.box("mount", tank + Vector((0, sign * r * 0.92, -r * 0.1)), (r * 1.45, 2.5, r * 0.7), mix(paint, frame, 0.45))
        body.box("mount", tank + Vector((0, sign * r * 0.97, -r * 0.1)), (r * 1.2, 1.0, r * 0.08), glow, glow=True)
    body.box("mount", Vector((d["seat"].x - base * 0.06, 0, seat_z - 3.0)), (base * 0.36, w * 1.35, 7.0), DARK)
    # Fenders: a long tail plate over the rear wheel, a short one over the front, a light at the tail.
    tail_end = Vector((-base / 2 - r * 1.05, 0, r * 1.7))
    body.slab("mount", Vector((-base * 0.26, 0, seat_z - r * 0.05)), tail_end, w * 1.5, 6.0, paint)
    body.box("mount", tail_end + Vector((-1.5, 0, 0)), (3.0, w * 0.9, r * 0.18), glow, glow=True)
    body.slab("mount", Vector((base / 2 + r * 0.8, 0, r * 1.75)), Vector((base / 2 - r * 0.55, 0, r * 2.12)), w * 1.3, 4.0, paint)
    # The fork, thick and raked; the headlight's grille, lit through its slats; the bars and their risers.
    for sign in (1, -1):
        top = head_stock + Vector((0, sign * w * 0.55, 0))
        axle = Vector((base / 2, sign * w * 0.55, r))
        body.limb("mount", top, top.lerp(axle, 0.55), r * 0.12, r * 0.12, metal)
        body.limb("mount", top.lerp(axle, 0.5), axle, r * 0.17, r * 0.15, frame)
    # The armoured front: a broad plate around the grille, wider than the tyre, built to hit things.
    grille = head_stock + Vector((r * 0.75, 0, -r * 0.2))
    body.box("mount", grille, (r * 0.55, w * 1.9, r * 1.0), frame)
    for sign in (1, -1):
        body.box("mount", grille + Vector((-r * 0.1, sign * w * 0.95, 0)), (r * 0.7, 3.0, r * 1.15), paint,
                 rotation=Euler((0.0, 0.0, -sign * 0.35)))
    for index in (-1, 0, 1):
        body.box("mount", grille + Vector((r * 0.28, 0, index * r * 0.26)), (1.5, w * 1.5, r * 0.12), glow, glow=True)
    body.limb("mount", head_stock, bars, r * 0.09, r * 0.09, metal)
    body.limb("mount", grip_r, grip_l, r * 0.075, r * 0.075, metal)
    for grip in (grip_l, grip_r):
        body.ball("mount", grip, r * 0.11, DARK)
    # Exhausts swept back along both flanks, their mouths glowing.
    for sign in (1, -1):
        start = Vector((base * 0.12, sign * w * 0.9, r * 1.05))
        end = Vector((-base * 0.55, sign * w * 1.2, r * 1.6))
        body.limb("mount", start, end, r * 0.15, r * 0.2, mix(metal, DARK, 0.5))
        body.ball("mount", end, r * 0.15, glow, glow=True)
    return body


def run_stride(d):
    """How far one Run cycle carries the mount: one turn of its wheels."""
    return math.tau * d["wheel"]


def _ride(pose, moved, d, turn=(0.0, 0.0, 0.0), shift=(0.0, 0.0, 0.0)):
    """Turns the mount about its rear axle and moves it by shift, carrying the seated rider with it."""
    pose["mount"] = turn
    pose["pelvis"] = turn
    rotation = Euler(turn, "XYZ").to_matrix()
    pivot, hip = d["rest"]["mount"], d["rest"]["pelvis"]
    moved["mount"] = Vector(shift)
    moved["pelvis"] = Vector(shift) + rotation @ (hip - pivot) - (hip - pivot)


def pose(name, t, melee, d):
    """Each bone's rotation about the armature's axes (radians), and the bones moved, at t from 0 to 1."""
    pose, moved = {}, {}
    tau = math.tau
    side = d["punch"]
    sign = 1 if side == "l" else -1
    if name == "Idle":
        # The engine idles under the rider: a fast, faint shudder; she breathes and looks about.
        _ride(pose, moved, d, shift=(0.0, 0.0, d["height"] * 0.003 * math.sin(t * tau * 8)))
        pose["spine_03"] = lean(1.5 * math.sin(t * tau))
        pose["head"] = twist(10 * math.sin(t * tau + 1.0))
    elif name == "Run":
        # The wheels turn once a cycle; the machine bucks over the ground; the rider tucks in.
        spin = (0.0, t * tau, 0.0)
        pose["wheel_back"] = pose["wheel_front"] = spin
        _ride(pose, moved, d, lean(0.8 * math.sin(t * tau * 2)), (0.0, 0.0, d["height"] * 0.006 * abs(math.sin(t * tau * 2))))
        pose["spine_01"] = lean(6)
        pose["head"] = lean(-6)
    elif name in ("AttackWindup", "AttackStrike"):
        # Melee: she revs and draws her bracer arm back, then the machine lunges and the fist drives through, landing as
        # the windup ends; the strike settles back. Ranged: the bracer arm raises to aim and the recoil rocks her back.
        if name == "AttackWindup":
            cock = ease(min(1.0, t / MELEE_COCK_SHARE)) if melee else ease(t)
            sweep = ease(max(0.0, (t - MELEE_COCK_SHARE) / (1.0 - MELEE_COCK_SHARE))) if melee else 0.0
            settle, recoil = 0.0, 0.0
        else:
            cock, sweep = 1.0, 1.0 if melee else 0.0
            settle = ease(t) if melee else ease(max(0.0, (t - 0.35) / 0.65))
            recoil = 0.0 if melee else math.sin(min(1.0, t / 0.4) * math.pi)
        hold = 1.0 - settle
        if melee:
            _ride(pose, moved, d, lean((-4 * cock + 9 * sweep) * hold), (d["wheel"] * 0.35 * sweep * hold, 0.0, 0.0))
            pose["spine_02"] = twist(sign * (30 * cock - 60 * sweep) * hold)
            pose["upperarm_" + side] = tuple(a + b for a, b in zip(forward_swing((-50 * cock + 95 * sweep) * hold), roll_side((35 * cock - 25 * sweep) * hold, sign)))
            pose["lowerarm_" + side] = forward_swing((70 * cock - 70 * sweep) * hold)
        else:
            _ride(pose, moved, d)
            pose["upperarm_" + side] = forward_swing(55 * cock * hold - 10 * recoil)
            pose["hand_" + side] = forward_swing(-45 * cock * hold)
            pose["spine_02"] = lean(-6 * recoil)
    elif name == "Cast":
        # A wheelie: the front rears up and the bracer arm lifts its light high.
        rise = ease(min(1.0, t / CAST_RELEASE_SHARE))
        fall = ease(max(0.0, (t - 0.6) / 0.4))
        k = rise * (1 - fall)
        _ride(pose, moved, d, lean(-18 * k))
        pose["upperarm_" + side] = tuple(a + b for a, b in zip(forward_swing(85 * k), roll_side(20 * k, sign)))
        pose["lowerarm_" + side] = forward_swing(25 * k)
        pose["spine_03"] = lean(-6 * k)
    elif name == "Hit":
        # The machine jolts back on its tyres and the rider recoils.
        k = math.sin(min(1.0, t) * math.pi)
        _ride(pose, moved, d, lean(-3 * k), (0.0, 0.0, -d["height"] * 0.01 * k))
        pose["spine_02"] = lean(-15 * k)
        pose["head"] = lean(-12 * k)
    elif name == "Death":
        # The machine goes down on its right side; the rider is thrown clear to its left and lands on her back, legs
        # out, arms flung wide.
        k = ease(min(1.0, t / 0.8))
        pose["mount"] = (math.radians(CRASH_ROLL) * k, 0.0, 0.0)
        moved["mount"] = Vector((0.0, 0.0, d["crashDrop"] * k))
        hip = d["rest"]["pelvis"]
        # Back past level from her forward lean, her chest's breadth above the ground; hips and knees straightened from
        # the seat (the knee's turn is the seated shin's angle to the thigh).
        pose["pelvis"] = lean(-105 * k)
        moved["pelvis"] = Vector((-d["leg"] * 0.4, d["wheel"] * 2.2, d["shoulder"] * 0.75 - hip.z)) * k
        pose["spine_02"] = lean(10 * k)
        pose["head"] = lean(-15 * k)
        for limb_side, limb_sign in (("l", 1), ("r", -1)):
            pose["thigh_" + limb_side] = forward_swing(-90 * k)
            pose["calf_" + limb_side] = forward_swing(88 * k)
            pose["upperarm_" + limb_side] = tuple(a + b for a, b in zip(roll_side(55 * k, limb_sign), forward_swing(-35 * k)))
    elif name == "Recall":
        # She sits up off the bars and holds the bracer's light to her chest while the engine idles.
        glow = math.sin(t * tau)
        _ride(pose, moved, d, shift=(0.0, 0.0, d["height"] * 0.003 * math.sin(t * tau * 6)))
        pose["spine_01"] = lean(-14)
        pose["upperarm_" + side] = forward_swing(10 + 2 * glow)
        pose["lowerarm_" + side] = forward_swing(70)
        pose["head"] = lean(8 + 2 * glow)
    else:
        raise AssertionError("Unknown animation: " + name)
    return pose, moved
