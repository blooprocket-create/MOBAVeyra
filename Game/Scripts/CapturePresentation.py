"""Capture the combat readability presentation in the lit Crucible, as players see it (ADR-063).

Run through Game/Scripts/CapturePresentation.ps1 in a rendering editor. On the Bottom lane of L_Battleground, under
the map's own sun and manual exposure, it stands the presentation's moments side by side:
- the near row: a body unflashed, at the hit flash's peak, at its Reduce Flashing peak, an enemy hovered (its outline),
  and the first imported Vanguard body at the flash's peak, when there is one (the overlay on a skinned mesh);
- the middle row: the impact, the cast flash and the death burst, each frozen early, midway and late in its life;
- the far row: a projectile's trail after a flight, the click marker's ring for a move and for an attack, and a melee
  swing's arc, drawn through line batches as the game draws them;
- then, alone on the middle row, the skills' own effects (ADR-072 §6) midway through their particles' life.
It captures them from the gameplay camera's pitch: once at the camera's own distance and field of view, the size players
see, and once through a narrow lens per row. Every look comes from where the game reads it: DefaultGame.ini's grey-box
and camera settings, the presentation specs and the tuning. Nothing is saved: what it spawns leaves with the editor.
"""
import json
import math
import os
import re
import time
import traceback
from pathlib import Path

import unreal

unreal.EditorPythonScripting.set_keep_python_script_alive(True)

GAME = Path(unreal.Paths.project_dir()).resolve()
OUTPUT = Path(os.environ["VEYRA_PRESENTATION_REVIEW_OUTPUT"])
OUTPUT.mkdir(parents=True, exist_ok=True)
WORLD_LAYOUT = json.loads((GAME / "Tuning" / "World.json").read_text())["layout"]
VANGUARDS = json.loads((GAME / "Tuning" / "Vanguards.json").read_text())["vanguards"]
EFFECTS_SPEC = json.loads((GAME / "ArtSource" / "Presentation" / "Effects.json").read_text())
MATERIALS_SPEC = json.loads((GAME / "ArtSource" / "Presentation" / "PresentationMaterials.json").read_text())
BODY_MANIFEST = GAME / "ArtSource" / "Vanguards" / "manifest.json"


def ini_section(name):
    """The key=value pairs of DefaultGame.ini's section name, where the game reads its settings."""
    values, inside = {}, False
    for line in (GAME / "Config" / "DefaultGame.ini").read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if line.startswith("[") and line.endswith("]"):
            inside = line[1:-1] == name
        elif inside and "=" in line and not line.startswith(";"):
            key, value = line.split("=", 1)
            values[key.strip().lstrip("+-")] = value.strip()
    return values


GREYBOX = ini_section("/Script/VeyraUI.VeyraGreyboxSettings")
CAMERA = ini_section("/Script/VeyraMatch.VeyraCameraSettings")
# The game camera's field of view (its component's default), and the close views', narrow so they keep the same angle.
GAME_FOV = 90.0
CLOSE_FOV = 18.0
# Where each row stands across the lane, in shares of its width (near the camera first), and how far apart the
# moments in a row stand, in shares of a body's diameter.
ROWS = {"Near": -0.36, "Middle": 0.0, "Far": 0.36}
GAP = 3.5
# When each burst is frozen, in shares of its longest particle life: just born, midway, nearly gone.
MOMENTS = (0.08, 0.4, 0.75)
# How long the trail's projectile has flown when it is frozen, in shares of the trail's longest particle life past
# that life, so the trail is whole.
FLIGHT_SHARE = 1.25
# The simulation step effects are advanced by, in seconds (a 60 Hz frame).
STEP = 1.0 / 60.0


def colour(key):
    channels = dict(re.findall(r"([RGBA])=(-?[0-9.]+)", GREYBOX[key]))
    return unreal.LinearColor(float(channels["R"]), float(channels["G"]), float(channels["B"]), float(channels.get("A", 1.0)))


def number(key):
    return float(GREYBOX[key])


def asset(key):
    loaded = unreal.load_asset(GREYBOX[key])
    assert loaded, f"{key}: {GREYBOX[key]} does not load; build it first"
    return loaded


def life_of(system):
    """The longest particle life a system's spec gives, in seconds."""
    spec = next(entry for entry in EFFECTS_SPEC["systems"] if Path(GREYBOX[system].split(".")[0]).name == entry["name"])
    lives = [entry["value"][0] for entry in spec["inputs"] if entry["input"] in ("Lifetime", "Lifetime Max") and "value" in entry]
    assert lives, f"{spec['name']} gives no particle life"
    return max(lives)


# The Vanguard whose capsule the review bodies take, and the projectile whose flight the trail follows: the first ranged
# basic attack in the tuning.
RANGED = next(vanguard for vanguard in VANGUARDS.values() if vanguard["basicAttack"].get("projectile"))
CAPSULE_RADIUS = RANGED["body"]["capsuleRadius"]
CAPSULE_HALF_HEIGHT = RANGED["body"]["capsuleHalfHeight"]
FLIGHT_SPEED = RANGED["basicAttack"]["projectile"][0]["speed"]
OUTLINE = next(entry for entry in MATERIALS_SPEC["materials"] if entry["kind"] == "postProcessOutline")

LEVEL = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ACTORS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert LEVEL.load_level("/Game/Veyra/World/Maps/L_Battleground")
LEVEL.editor_set_game_view(True)
LEVEL.editor_set_viewport_realtime(True)
WORLD = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
for name in ("ViewDistance", "AntiAliasing", "Shadow", "GlobalIllumination", "Reflection", "PostProcess", "Texture", "Effects", "Foliage", "Shading"):
    unreal.SystemLibrary.execute_console_command(WORLD, f"sg.{name}Quality 2")
unreal.SystemLibrary.execute_console_command(WORLD, "r.ScreenPercentage 100")


def standing(x, y):
    """Where a unit's feet stand at (x, y) on the loaded battleground's playable ground, as the game places units."""
    result = unreal.VeyraWorldToolsLibrary.standing_point(WORLD, unreal.Vector2D(x, y))
    found, location = result if isinstance(result, tuple) else (result is not None, result)
    assert found, f"No playable ground under ({x}, {y})"
    return location


def lane():
    """The Bottom lane's straight run along +Y: a point on its centre line clear of base and river, and its width."""
    bottom = next(entry for entry in WORLD_LAYOUT["lanes"] if entry["lane"] == "Bottom")
    start, turn = bottom["points"][0], bottom["points"][1]
    assert start["x"] == turn["x"], "The Bottom lane's first run no longer runs along +Y"
    return start["x"], start["y"] + (turn["y"] - start["y"]) * 0.25, bottom["width"]


LANE_X, LANE_Y, LANE_WIDTH = lane()
SPACING = CAPSULE_RADIUS * 2.0 * GAP


def place(row, index, count):
    """The ground under the index-th of count moments in row, centred on the lane."""
    return standing(LANE_X + ROWS[row] * LANE_WIDTH, LANE_Y + (index - (count - 1) / 2) * SPACING)


def lifted(ground, height):
    return unreal.Vector(ground.x, ground.y, ground.z + height)


SPAWNED = []
EFFECTS = []


def body(ground, side_colour, flash=0.0, stencil=0):
    """A grey-box body on its capsule, in a side's colour; flashed at strength flash, outlined in stencil if above 0."""
    mesh = asset("BodyMesh")
    bounds = mesh.get_bounds()
    scale = unreal.Vector(CAPSULE_RADIUS / bounds.box_extent.x, CAPSULE_RADIUS / bounds.box_extent.y, CAPSULE_HALF_HEIGHT / bounds.box_extent.z)
    # Its centre on the capsule's, whatever the mesh's own pivot.
    centre = lifted(ground, CAPSULE_HALF_HEIGHT)
    pivot = unreal.Vector(centre.x - bounds.origin.x * scale.x, centre.y - bounds.origin.y * scale.y, centre.z - bounds.origin.z * scale.z)
    actor = ACTORS.spawn_actor_from_class(unreal.StaticMeshActor, pivot, unreal.Rotator(0.0, 0.0, 0.0))
    component = actor.static_mesh_component
    component.set_mobility(unreal.ComponentMobility.MOVABLE)
    component.set_static_mesh(mesh)
    actor.set_actor_scale3d(scale)
    tinted = component.create_dynamic_material_instance(0, asset("ShapeMaterial"))
    tinted.set_vector_parameter_value(GREYBOX["ColorParameter"], side_colour)
    dress(component, flash, stencil)
    SPAWNED.append(actor)


def dress(component, flash, stencil):
    if flash > 0.0:
        overlay = unreal.MaterialLibrary.create_dynamic_material_instance(WORLD, asset("HitFlashMaterial"))
        overlay.set_vector_parameter_value(GREYBOX["HitFlashColorParameter"], colour("HitFlashColor"))
        overlay.set_scalar_parameter_value(GREYBOX["HitFlashStrengthParameter"], flash)
        component.set_overlay_material(overlay)
    if stencil > 0:
        component.set_render_custom_depth(True)
        component.set_custom_depth_stencil_value(stencil)


def vanguard_body(ground):
    """The first imported Vanguard body, at rest, flashed at its peak: the overlay on a skinned mesh."""
    if not BODY_MANIFEST.is_file():
        return False
    for entry in json.loads(BODY_MANIFEST.read_text())["assets"]:
        folder = entry["name"][len("SK_"):]
        mesh = unreal.load_asset(f"/Game/Veyra/Vanguards/{folder}/{entry['name']}")
        if mesh:
            actor = ACTORS.spawn_actor_from_class(unreal.SkeletalMeshActor, ground, unreal.Rotator(0.0, 0.0, 145.0))
            actor.skeletal_mesh_component.set_skeletal_mesh_asset(mesh)
            dress(actor.skeletal_mesh_component, 1.0, 0)
            SPAWNED.append(actor)
            return True
    return False


def effect(system_key, location, rotation, side_colour, seconds, path=None):
    """A system at location in a side's colour, to be advanced seconds into its life (along path, if it moves) and frozen."""
    system = asset(system_key)
    component = unreal.NiagaraFunctionLibrary.spawn_system_at_location(WORLD, system, location, rotation, unreal.Vector(1.0, 1.0, 1.0), False, False,
                                                                       unreal.NCPoolMethod.NONE, False)
    assert component, f"{system_key} did not spawn"
    component.set_variable_linear_color(GREYBOX["EffectColorParameter"], side_colour)
    EFFECTS.append({"component": component, "seconds": seconds, "path": path, "start": location})


def stage():
    ally, enemy = colour("AllyColor"), colour("EnemyColor")
    near = ["unflashed", "flash", "reducedFlash", "outlined", "vanguard"]
    body(place("Near", 0, len(near)), ally)
    body(place("Near", 1, len(near)), ally, flash=1.0)
    body(place("Near", 2, len(near)), ally, flash=number("ReducedFlashStrength"))
    body(place("Near", 3, len(near)), enemy, stencil=int(OUTLINE["stencils"]["enemy"]))
    has_vanguard = vanguard_body(place("Near", 4, len(near)))
    # The middle row: each burst early, midway and late, where the game plays it (a unit's centre); the cast toward +Y.
    middle = [("ImpactEffect", enemy, unreal.Rotator(0.0, 0.0, 0.0)), ("CastEffect", ally, unreal.Rotator(0.0, 0.0, 90.0)),
              ("DeathEffect", enemy, unreal.Rotator(0.0, 0.0, 0.0))]
    count = len(middle) * len(MOMENTS)
    index = 0
    for system_key, side_colour, rotation in middle:
        for share in MOMENTS:
            effect(system_key, lifted(place("Middle", index, count), CAPSULE_HALF_HEIGHT), rotation, side_colour, life_of(system_key) * share)
            index += 1
    # The far row: a trail flown along +Y, the click marker for a move and an attack, and a melee swing's arc.
    flight = life_of("TrailEffect") * FLIGHT_SHARE
    trail_end = lifted(place("Far", 1, 5), CAPSULE_HALF_HEIGHT)
    trail_start = trail_end - unreal.Vector(0.0, FLIGHT_SPEED * flight, 0.0)
    effect("TrailEffect", trail_start, unreal.Rotator(0.0, 0.0, 90.0), ally, flight, path=unreal.Vector(0.0, FLIGHT_SPEED, 0.0))
    lift = number("TelegraphLift")
    thickness = number("TelegraphThickness")
    segments = int(number("CircleSegments"))
    for slot, key in ((2, "OrderMoveColor"), (3, "OrderAttackColor")):
        unreal.SystemLibrary.draw_debug_circle(WORLD, lifted(place("Far", slot, 5), lift), number("OrderMarkStartRadius"), segments, colour(key), 3600.0,
                                               thickness, unreal.Vector(1.0, 0.0, 0.0), unreal.Vector(0.0, 1.0, 0.0), False)
    swing_at = place("Far", 4, 5)
    body(swing_at, ally)
    reach = CAPSULE_RADIUS * 2.0 + number("SwingArcReach") / 2.0
    half = math.radians(number("SwingArcDegrees") / 2.0)
    centre = lifted(swing_at, lift)
    points = [centre + unreal.Vector(math.cos(-half + 2.0 * half * step / segments), math.sin(-half + 2.0 * half * step / segments), 0.0) * reach
              for step in range(segments + 1)]
    for start, end in zip([centre] + points, points + [centre]):
        unreal.SystemLibrary.draw_debug_line(WORLD, start, end, ally, 3600.0, thickness)
    return near if has_vanguard else near[:-1]


# Skills' own effects (ADR-072 §6): the systems the spec gives that purpose, each shown at a share of its particles'
# life, in the ally colour, a channel's at a length it might run. Captured after the rest, alone on the middle row.
SKILL_EFFECT_PURPOSE = "ADR-072"
SKILL_EFFECT_MOMENT = 0.5
SKILL_CHANNEL_LENGTH = 600.0


def skill_stage():
    """Clears the rows' effects and sets out every skill effect along the middle row, a channel's toward +Y."""
    for entry in EFFECTS:
        entry["component"].destroy_component(entry["component"])
    EFFECTS.clear()
    systems = [entry for entry in EFFECTS_SPEC["systems"] if entry["purpose"].startswith(SKILL_EFFECT_PURPOSE)]
    ally = colour("AllyColor")
    for index, spec in enumerate(systems):
        path = f"{EFFECTS_SPEC['destination']}/{spec['name']}.{spec['name']}"
        system = unreal.load_asset(path)
        assert system, f"{path} does not load; build it first"
        lives = [entry["value"][0] for entry in spec["inputs"] if entry["input"] in ("Lifetime", "Lifetime Max") and "value" in entry]
        where = lifted(place("Middle", index, len(systems)), CAPSULE_HALF_HEIGHT)
        component = unreal.NiagaraFunctionLibrary.spawn_system_at_location(WORLD, system, where, unreal.Rotator(0.0, 0.0, 90.0), unreal.Vector(1.0, 1.0, 1.0),
                                                                           False, False, unreal.NCPoolMethod.NONE, False)
        assert component, f"{spec['name']} did not spawn"
        component.set_variable_linear_color(GREYBOX["EffectColorParameter"], ally)
        component.set_variable_float(GREYBOX["EffectScaleParameter"], 1.0)
        for own in spec.get("userFloats", []):
            component.set_variable_float(own["name"], SKILL_CHANNEL_LENGTH)
        EFFECTS.append({"component": component, "seconds": max(lives) * SKILL_EFFECT_MOMENT if lives else STEP, "path": None, "start": where})
    return [entry["name"] for entry in systems]


def freeze():
    """Starts every effect (again, if its system was still compiling when spawned), advances it to its moment and
    pauses it there. False while any is still compiling."""
    for entry in EFFECTS:
        if not entry["component"].is_active():
            entry["component"].activate(True)
    if any(not entry["component"].is_active() for entry in EFFECTS):
        return False
    for entry in EFFECTS:
        component = entry["component"]
        component.set_paused(False)
        component.reset_system()
        component.set_world_location(entry["start"], False, False)
        ticks = max(1, round(entry["seconds"] / STEP))
        if entry["path"] is None:
            component.advance_simulation(ticks, STEP)
        else:
            # Moved a step at a time, so the trail pours along its flight as a moving projectile's does.
            for tick in range(ticks):
                component.set_world_location(entry["start"] + entry["path"] * (STEP * (tick + 1)), False, False)
                component.advance_simulation(1, STEP)
        component.set_paused(True)
    return True


def camera_at(centre, fov, distance):
    """A camera at the gameplay camera's pitch, looking along +X at centre from distance, drawing the hover outline."""
    pitch = float(CAMERA["PitchDegrees"])
    forward = unreal.Vector(math.cos(math.radians(pitch)), 0.0, math.sin(math.radians(pitch)))
    camera = ACTORS.spawn_actor_from_class(unreal.CameraActor, centre - forward * distance, unreal.Rotator(0.0, pitch, 0.0))
    component = camera.camera_component
    component.set_editor_property("field_of_view", fov)
    component.set_editor_property("constrain_aspect_ratio", False)
    outline = unreal.MaterialLibrary.create_dynamic_material_instance(WORLD, asset("HoverOutlineMaterial"))
    for side in ("Enemy", "Ally", "Neutral"):
        outline.set_vector_parameter_value(GREYBOX[f"Hover{side}ColorParameter"], colour(f"{side}Color"))
    settings = component.get_editor_property("post_process_settings")
    settings.set_editor_property("weighted_blendables", unreal.WeightedBlendables(array=[unreal.WeightedBlendable(weight=1.0, object=outline)]))
    component.set_editor_property("post_process_settings", settings)
    component.set_editor_property("post_process_blend_weight", 1.0)
    SPAWNED.append(camera)
    return camera


# How many seconds an effect may take to start (its system compiling) before the capture gives up on it.
COMPILE_TRIES = 120
STATE = {"queue": ["stage", "freeze", "Warmup", "freeze", "Game", "Near", "Middle", "Far", "skills", "freeze", "Skills"], "task": None, "next": time.monotonic() + 15,
         "started": time.monotonic(), "captures": [], "compiling": 0}


def shoot(step):
    game = step in ("Game", "Warmup")
    fov = GAME_FOV if game else CLOSE_FOV
    row = "Middle" if step == "Skills" else step
    centre = lifted(standing(LANE_X + (ROWS[row] * LANE_WIDTH if row in ROWS else 0.0), LANE_Y), CAPSULE_HALF_HEIGHT)
    # The game view from the camera's own distance; a row's close view from as far as frames the row.
    span = SPACING * (len(MOMENTS) * 3)
    distance = float(CAMERA["Distance"]) if game else span * 0.62 / math.tan(math.radians(fov / 2))
    LEVEL.pilot_level_actor(camera_at(centre, fov, distance))
    filename = OUTPUT / ("warmup.png" if step == "Warmup" else f"Presentation_{step}.png")
    if filename.exists():
        filename.unlink()
    unreal.SystemLibrary.execute_console_command(WORLD, f'HighResShot 1920x1080 filename="{filename.as_posix()}"')
    STATE["task"] = str(filename)
    if step != "Warmup":
        STATE["captures"].append({"view": step, "file": str(filename), "fieldOfView": fov, "distance": round(distance)})


def tick(delta):
    try:
        if time.monotonic() - STATE["started"] > 900:
            raise RuntimeError("Presentation capture timed out")
        if time.monotonic() < STATE["next"]:
            return
        if STATE["task"] and not Path(STATE["task"]).is_file():
            return
        STATE["task"] = None
        if not STATE["queue"]:
            (OUTPUT / "manifest.json").write_text(json.dumps({"status": "captured", "nearRow": STATE["near"], "skillEffects": STATE.get("skills", []),
                                                                   "captures": STATE["captures"]}, indent=2),
                                                  encoding="utf-8")
            unreal.unregister_slate_post_tick_callback(HANDLE)
            unreal.SystemLibrary.quit_editor()
            return
        step = STATE["queue"].pop(0)
        if step == "skills":
            STATE["skills"] = skill_stage()
            STATE["next"] = time.monotonic() + 2
        elif step == "stage":
            STATE["near"] = stage()
            # Let the lighting settle on what stands.
            STATE["next"] = time.monotonic() + 5
        elif step == "freeze":
            if not freeze():
                STATE["compiling"] += 1
                assert STATE["compiling"] <= COMPILE_TRIES, "An effect never started"
                STATE["queue"].insert(0, step)
                STATE["next"] = time.monotonic() + 1
                return
            STATE["compiling"] = 0
            STATE["next"] = time.monotonic() + 1
        else:
            shoot(step)
            STATE["next"] = time.monotonic() + 3
    except Exception:
        (OUTPUT / "failure.txt").write_text(traceback.format_exc(), encoding="utf-8")
        unreal.unregister_slate_post_tick_callback(HANDLE)
        unreal.SystemLibrary.quit_editor()


HANDLE = unreal.register_slate_post_tick_callback(tick)
