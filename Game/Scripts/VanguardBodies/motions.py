"""Skill motions (ADR-072 §2): the shared primitives a Vanguard's own skill clips are made from.

Each motion poses the upper body every archetype shares with the humanoid (the arm chains, the spine, the head) through
four beats: anticipation up to its release, the release itself, follow-through, and a settle back toward rest by the
clip's end. The release is the moment the cast commits; the runtime holds a clip there while its cast is held (a windup
waiting or a channel) and plays the rest once it commits (ADR-072 §3), so whatever a skill shows during a channel is its
release pose.

Beside the pose, each motion says what the rest of the body does in terms any archetype can read, so a colossus and a
construct can share one primitive:
- crouch: 0 to 1, how far the legs bend under the body (a colossus sinks; a floating construct dips its core);
- spread: 0 to 1, how wide the feet are set (a brace);
- rise: -1 to 1, how far the body lifts with the motion (a construct's core and halo rise; a colossus rocks up on it);
- gather: -1 to 1, a construct's orbiting parts drawn in behind it (positive) or flung out ahead (negative);
- spin: degrees, a construct's orbiting parts turned about it together.
The archetype turns these into its own bones (colossus.skill_pose, construct.skill_pose).

Poses are rotations about the armature's axes (X forward, Y left, Z up), as VanguardBodies.parts builds them. Angles are
art values, chosen at the gameplay camera's distance, where a motion must read from its silhouette alone.
"""
import math

from .parts import combine, ease, forward_swing, lean, roll_side, twist

# How long a release takes to strike, as a share of the clip: fast, so the commit reads as a snap.
STRIKE_SHARE = 0.12
# Where the settle back to rest begins, as a share of the clip, unless a motion holds longer.
SETTLE_START = 0.7
# A hurl's settle is the haul on its chain; the arm falls back to rest only over the clip's last share.
HURL_REST_START = 0.88


def _beats(t, release, strike=STRIKE_SHARE, settle_start=SETTLE_START):
    """The four beats at t: the rise to the release (0 to 1), the strike through it (0 to 1, fast), and the settle back
    to rest (0 to 1). A clip's release may come late (an ultimate's long rise), so the settle never begins before the
    strike has ended."""
    rise = ease(min(1.0, t / release)) if release > 0.0 else 1.0
    strike_end = min(1.0, release + strike)
    hit = ease(min(1.0, max(0.0, (t - release) / max(strike_end - release, 1e-6))))
    begin = max(settle_start, strike_end)
    settle = ease(max(0.0, (t - begin) / max(1.0 - begin, 1e-6)))
    return rise, hit, settle


def _side(options):
    """The arm a one-armed motion uses (options "side": "l" or "r", right by default) and its sign for roll_side."""
    side = options.get("side", "r")
    return side, (1 if side == "l" else -1)


def _arm(pose, side, upper, lower=None, hand=None):
    pose["upperarm_" + side] = upper
    if lower is not None:
        pose["lowerarm_" + side] = lower
    if hand is not None:
        pose["hand_" + side] = hand


def _blend(a, b, share):
    """Rotation a eased toward b by share, component by component."""
    return tuple(x + (y - x) * share for x, y in zip(a, b))


ZERO = (0.0, 0.0, 0.0)


def hurl(t, release, options):
    """One arm wound back across the body, then flung out straight at full reach and held there, then hauled back in:
    a hook thrown on its chain, and the pull that follows."""
    side, sign = _side(options)
    rise, hit, settle = _beats(t, release, settle_start=options.get("holdUntil", 0.62))
    pose = {}
    # Wound: the arm drawn back and out, the forearm folded, the chest turned toward the throwing side.
    wound = combine(forward_swing(-45), roll_side(35, sign))
    flung = forward_swing(95)
    hauled = forward_swing(35)
    # The haul's last beat lets the arm fall back to rest by the clip's end.
    rest = ease(max(0.0, (t - HURL_REST_START) / (1.0 - HURL_REST_START)))
    upper = _blend(_blend(_blend(ZERO, wound, rise), flung, hit), hauled, settle)
    lower = forward_swing(70 * rise * (1 - hit) + 55 * settle)
    _arm(pose, side, _blend(upper, ZERO, rest), _blend(lower, ZERO, rest))
    other = "l" if side == "r" else "r"
    _arm(pose, other, forward_swing(25 * rise * (1 - settle)), forward_swing(40 * rise * (1 - rest)))
    turn = sign * (30 * rise - 55 * hit + 25 * settle) * (1 - rest)
    pose["spine_02"] = combine(twist(turn), lean((-6 * rise + 18 * hit - 22 * settle) * (1 - rest)))
    pose["head"] = lean(6 * hit * (1 - settle))
    return pose, {"crouch": (0.25 * rise + 0.25 * hit * (1 - settle)) * (1 - rest), "gather": rise * (1 - hit) - hit * (1 - settle)}


def slam(t, release, options):
    """Both arms raised high on a rising chest, then driven into the ground before the body, which follows them down."""
    rise, hit, settle = _beats(t, release)
    pose = {}
    for side, sign in (("l", 1), ("r", -1)):
        up = 165 * rise
        down = -125 * hit
        _arm(pose, side, combine(forward_swing((up + down) * (1 - settle)), roll_side(10 * rise * (1 - hit), sign)),
             forward_swing((25 * rise - 15 * hit) * (1 - settle)))
    pose["spine_02"] = lean((-18 * rise + 48 * hit) * (1 - settle))
    pose["spine_03"] = lean((-8 * rise + 14 * hit) * (1 - settle))
    pose["head"] = lean((-10 * rise - 18 * hit) * (1 - settle))
    return pose, {"crouch": (0.15 * rise + 0.7 * hit) * (1 - settle), "rise": (0.6 * rise - 0.9 * hit) * (1 - settle)}


def brace(t, release, options):
    """Feet planted wide and the body sinking, forearms crossed before the chest; at the release the chest swells and
    the arms set, then it eases out of the brace (a status body takes over while the brace lasts)."""
    rise, hit, settle = _beats(t, release, settle_start=0.75)
    pose = {}
    k = max(rise, hit) * (1 - settle)
    for side, sign in (("l", 1), ("r", -1)):
        _arm(pose, side, combine(forward_swing(75 * k), roll_side(-22 * k, sign), twist(sign * 25 * k)),
             combine(forward_swing(105 * k), twist(sign * -20 * k)))
    pose["spine_02"] = lean((14 * rise - 22 * hit) * (1 - settle))
    pose["head"] = lean((-6 * rise - 6 * hit) * (1 - settle))
    return pose, {"crouch": 0.9 * k, "spread": k, "rise": -0.3 * k}


def anchor(t, release, options):
    """Arms raised, then driven down into the ground on bent knees and held there as the cast gathers; at the release
    they tear back up out of it, and the body heaves upright with them."""
    drive = options.get("driveShare", 0.55)
    rise = ease(min(1.0, t / (release * drive)))
    plunge = ease(min(1.0, max(0.0, (t - release * drive) / (release * (1 - drive) * 0.5))))
    _, hit, settle = _beats(t, release, strike=0.16)
    pose = {}
    for side, sign in (("l", 1), ("r", -1)):
        upper = 160 * rise - 125 * plunge + 125 * hit
        _arm(pose, side, combine(forward_swing(upper * (1 - settle)), roll_side((14 * rise * (1 - plunge) + 20 * hit) * (1 - settle), sign)),
             forward_swing((20 * rise - 10 * plunge + 30 * hit) * (1 - settle)))
    pose["spine_02"] = lean((-14 * rise + 52 * plunge - 70 * hit) * (1 - settle))
    pose["spine_03"] = lean((14 * plunge - 18 * hit) * (1 - settle))
    pose["head"] = lean((-20 * plunge + 10 * hit) * (1 - settle))
    crouch = (0.25 * rise + 0.75 * plunge) * (1 - hit) + 0.1 * hit
    return pose, {"crouch": crouch * (1 - settle), "spread": plunge * (1 - settle), "rise": (0.5 * rise - plunge + 1.2 * hit) * (1 - settle)}


def lob(t, release, options):
    """One arm swung low and back, then heaved up and over in an arc, the body rocking forward into it."""
    side, sign = _side(options)
    rise, hit, settle = _beats(t, release)
    pose = {}
    upper = -55 * rise + 200 * hit
    _arm(pose, side, combine(forward_swing(upper * (1 - settle)), roll_side(12 * rise * (1 - hit), sign)),
         forward_swing((35 * rise - 25 * hit) * (1 - settle)))
    other = "l" if side == "r" else "r"
    _arm(pose, other, forward_swing((30 * rise - 10 * hit) * (1 - settle)))
    pose["spine_02"] = combine(lean((16 * rise - 24 * hit) * (1 - settle)), twist(sign * (18 * rise - 26 * hit) * (1 - settle)))
    return pose, {"crouch": 0.35 * rise * (1 - hit), "rise": 0.4 * hit * (1 - settle)}


def harden(t, release, options):
    """Hunched in on itself, arms drawn in tight, then flexed out hard as the shell sets."""
    rise, hit, settle = _beats(t, release, settle_start=0.68)
    pose = {}
    for side, sign in (("l", 1), ("r", -1)):
        drawn = combine(forward_swing(45), roll_side(-18, sign))
        flexed = combine(roll_side(70, sign), forward_swing(15))
        _arm(pose, side, _blend(_blend(_blend(ZERO, drawn, rise), flexed, hit), ZERO, settle),
             forward_swing((125 * rise - 25 * hit) * (1 - settle)))
    pose["spine_02"] = lean((28 * rise - 40 * hit) * (1 - settle))
    pose["head"] = lean((14 * rise - 22 * hit) * (1 - settle))
    return pose, {"crouch": (0.45 * rise - 0.3 * hit) * (1 - settle), "spread": 0.6 * hit * (1 - settle), "rise": 0.4 * hit * (1 - settle)}


def sweep(t, release, options):
    """One arm swept low across the body, from the far side to the near, pouring along a line on the ground."""
    side, sign = _side(options)
    rise, hit, settle = _beats(t, release, strike=0.22)
    pose = {}
    # Raised out to its side and turned about the vertical: forward across the body, then out past the near side.
    across = math.radians(sign * -70)
    out = math.radians(sign * 45)
    turn = (across * rise + (out - across) * hit) * (1 - settle)
    _arm(pose, side, (roll_side(55 * max(rise, hit) * (1 - settle), sign)[0], 0.0, turn), forward_swing(25 * rise * (1 - settle)))
    other = "l" if side == "r" else "r"
    _arm(pose, other, forward_swing(20 * rise * (1 - settle)))
    pose["spine_02"] = combine(lean(24 * max(rise, hit) * (1 - settle)), twist(sign * (-28 * rise + 50 * hit) * (1 - settle)))
    return pose, {"crouch": 0.45 * max(rise, hit) * (1 - settle)}


def heave(t, release, options):
    """Both arms scooped low behind, the body deep in a crouch, then hurled forward and up as it rises onto it."""
    rise, hit, settle = _beats(t, release, strike=0.16)
    pose = {}
    for side, sign in (("l", 1), ("r", -1)):
        _arm(pose, side, combine(forward_swing((-50 * rise + 175 * hit) * (1 - settle)), roll_side(15 * rise * (1 - hit), sign)),
             forward_swing((30 * rise - 20 * hit) * (1 - settle)))
    pose["spine_02"] = lean((34 * rise - 46 * hit) * (1 - settle))
    pose["head"] = lean((-18 * rise + 10 * hit) * (1 - settle))
    return pose, {"crouch": (0.75 * rise * (1 - hit) + 0.1 * hit) * (1 - settle), "rise": (-0.4 * rise + 0.9 * hit) * (1 - settle)}


def clench(t, release, options):
    """A fist raised high and crushed shut, then the arm snapped down: the signal for what it set to break."""
    side, sign = _side(options)
    rise, hit, settle = _beats(t, release)
    pose = {}
    _arm(pose, side, combine(forward_swing((150 * rise - 105 * hit) * (1 - settle)), roll_side(15 * rise * (1 - hit), sign)),
         forward_swing((70 * rise - 50 * hit) * (1 - settle)), forward_swing(-30 * rise * (1 - settle)))
    pose["spine_02"] = combine(lean((-10 * rise + 22 * hit) * (1 - settle)), twist(sign * 12 * rise * (1 - settle)))
    return pose, {"crouch": 0.3 * hit * (1 - settle), "rise": 0.4 * rise * (1 - hit)}


def thrust(t, release, options):
    """One hand drawn back to the shoulder, then driven forward and open, the parts about it flung after it."""
    side, sign = _side(options)
    rise, hit, settle = _beats(t, release)
    pose = {}
    drawn = combine(forward_swing(-15), roll_side(35, sign))
    driven = forward_swing(92)
    _arm(pose, side, _blend(_blend(ZERO, drawn, rise), driven, hit) if settle == 0 else _blend(driven, ZERO, settle),
         forward_swing((125 * rise - 125 * hit) * (1 - settle)), forward_swing(-70 * hit * (1 - settle)))
    other = "l" if side == "r" else "r"
    _arm(pose, other, combine(forward_swing((30 * rise - 20 * hit) * (1 - settle)), roll_side(20 * hit * (1 - settle), -sign)))
    pose["spine_03"] = combine(twist(sign * (25 * rise - 40 * hit) * (1 - settle)), lean(10 * hit * (1 - settle)))
    return pose, {"gather": rise * (1 - hit) - hit * (1 - settle), "crouch": 0.2 * hit * (1 - settle)}


def summon(t, release, options):
    """One arm raised high, palm up, the body lifting with it, calling down what falls."""
    side, sign = _side(options)
    rise, hit, settle = _beats(t, release, settle_start=0.65)
    pose = {}
    raised = 165 * rise + 12 * hit
    _arm(pose, side, combine(forward_swing(raised * (1 - settle)), roll_side(12 * rise * (1 - settle), sign)),
         forward_swing(15 * rise * (1 - settle)), forward_swing(-40 * rise * (1 - settle)))
    other = "l" if side == "r" else "r"
    _arm(pose, other, roll_side(30 * rise * (1 - settle), -sign))
    pose["spine_03"] = lean(-14 * rise * (1 - settle))
    pose["head"] = lean(-22 * rise * (1 - settle))
    return pose, {"rise": (0.7 * rise + 0.3 * hit) * (1 - settle), "spin": 90 * t, "gather": -0.6 * hit * (1 - settle)}


def veil(t, release, options):
    """Arms folded in close, the body turning inside its parts as they wheel about it; then the arms open out."""
    rise, hit, settle = _beats(t, release, strike=0.2, settle_start=0.6)
    pose = {}
    for side, sign in (("l", 1), ("r", -1)):
        folded = combine(forward_swing(60), roll_side(-25, sign), twist(sign * 30))
        opened = combine(roll_side(45, sign), forward_swing(20))
        _arm(pose, side, _blend(_blend(_blend(ZERO, folded, rise), opened, hit), ZERO, settle), forward_swing(110 * rise * (1 - hit)))
    pose["spine_03"] = combine(lean(12 * rise * (1 - hit)), twist(40 * rise * (1 - settle)))
    return pose, {"spin": 300 * ease(t), "gather": 0.8 * rise * (1 - hit), "rise": 0.25 * hit * (1 - settle)}


def beam(t, release, options):
    """Both hands raised and joined overhead as the parts gather into a prism there, then thrust forward at the
    release and held for the channel, then a recoil as it ends."""
    raised_share = options.get("raiseShare", 0.6)
    lift = ease(min(1.0, t / (release * raised_share)))
    aim = ease(min(1.0, max(0.0, (t - release * raised_share) / (release * (1 - raised_share)))))
    _, hit, settle = _beats(t, release, strike=0.1, settle_start=0.75)
    recoil = math.sin(hit * math.pi) * (1 - settle)
    pose = {}
    for side, sign in (("l", 1), ("r", -1)):
        upper = (170 * lift - 80 * aim - 10 * recoil) * (1 - settle)
        _arm(pose, side, combine(forward_swing(upper), roll_side(-14 * lift * (1 - settle), sign)),
             forward_swing(15 * lift * (1 - aim) * (1 - settle)), forward_swing(-80 * aim * (1 - settle)))
    pose["spine_03"] = lean((-16 * lift + 22 * aim - 12 * recoil) * (1 - settle))
    pose["head"] = lean((-20 * lift + 20 * aim) * (1 - settle))
    return pose, {"rise": (0.9 * lift - 0.3 * aim) * (1 - settle), "gather": -0.5 * aim * (1 - settle), "spin": 120 * lift, "crouch": 0.15 * aim * (1 - settle)}


MOTIONS = {"Hurl": hurl, "Slam": slam, "Brace": brace, "Anchor": anchor, "Lob": lob, "Harden": harden, "Sweep": sweep,
           "Heave": heave, "Clench": clench, "Thrust": thrust, "Summon": summon, "Veil": veil, "Beam": beam}


def skill_clip(name):
    """The take a skill's clip is keyed as (ADR-072 §1), for its ability ID."""
    return "Skill_" + name


def motion(skill, t):
    """The upper body's pose and the body hints of skill (a kit entry: motion, releaseShare, options) at t from 0 to 1."""
    return MOTIONS[skill["motion"]](t, skill["releaseShare"], skill.get("options", {}))
