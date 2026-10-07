"""Capture the Vanguards' generated bodies in the lit Crucible, as players see them (ADR-064 §4).

Run through Game/Scripts/CaptureVanguards.ps1 in a rendering editor. For each Vanguard named, its body stands on the
Bottom lane in its animations' key poses: one row turned three-quarters toward the camera, one in profile. Each is
captured from the gameplay camera's pitch: once at the camera's own distance and field of view, so it reads at the size
players see, and once close, so its silhouette can be judged. Nothing is saved: the spawned bodies leave with the editor.
"""
import json
import math
import os
import time
import traceback
from pathlib import Path

import unreal

unreal.EditorPythonScripting.set_keep_python_script_alive(True)

GAME = Path(unreal.Paths.project_dir()).resolve()
OUTPUT = Path(os.environ["VEYRA_VANGUARD_REVIEW_OUTPUT"])
OUTPUT.mkdir(parents=True, exist_ok=True)
IDS = [vanguard for vanguard in os.environ["VEYRA_VANGUARD_REVIEW_IDS"].split(",") if vanguard]
MANIFEST = json.loads((GAME / "ArtSource" / "Vanguards" / "manifest.json").read_text())
KIT = json.loads((GAME / "ArtSource" / "Vanguards" / "VanguardKit.json").read_text())
WORLD_LAYOUT = json.loads((GAME / "Tuning" / "World.json").read_text())["layout"]
CAMERA = {key.strip(): value.strip() for key, value in
          (line.split("=", 1) for line in os.environ["VEYRA_VANGUARD_REVIEW_CAMERA"].split(";") if "=" in line)}
# The poses GenerateVanguardBodies.py previews, as (animation, time from 0 to 1).
POSES = [("Idle", 0.0), ("Run", 0.25), ("Run", 0.75), ("AttackWindup", 0.65), ("AttackWindup", 1.0),
         ("Cast", 0.5), ("Hit", 0.5), ("Recall", 0.5), ("Death", 1.0)]
# The rows' facings in degrees of yaw: three-quarters toward the camera, which looks along +X, and in profile.
ROWS = [145.0, 90.0]
# The game camera's field of view (its component's default), and the close view's, narrow so it keeps the same angle.
GAME_FOV = 90.0
CLOSE_FOV = 18.0
# How far apart the poses stand, in shares of a body's largest extent, and the rows, in shares of the lane's width.
POSE_GAP = 1.25
ROW_GAP = 0.55

LEVEL = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ACTORS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert LEVEL.load_level("/Game/Veyra/World/Maps/L_Battleground")
LEVEL.editor_set_game_view(True)
# The viewport keeps rendering, and its bodies keep evaluating, between frames the editor would otherwise skip.
LEVEL.editor_set_viewport_realtime(True)
WORLD = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
for name in ("ViewDistance", "AntiAliasing", "Shadow", "GlobalIllumination", "Reflection", "PostProcess", "Texture", "Effects", "Foliage", "Shading"):
    unreal.SystemLibrary.execute_console_command(WORLD, f"sg.{name}Quality 2")
unreal.SystemLibrary.execute_console_command(WORLD, "r.ScreenPercentage 100")


def greybox_setting(name):
    """A value of the presentation's settings (VeyraGreyboxSettings in DefaultGame.ini), from the one place it is set."""
    section = False
    for line in (GAME / "Config" / "DefaultGame.ini").read_text(encoding="utf-8").splitlines():
        if line.startswith("["):
            section = line.strip() == "[/Script/VeyraUI.VeyraGreyboxSettings]"
        elif section and line.startswith(name + "="):
            return line.split("=", 1)[1].strip()
    raise AssertionError("DefaultGame.ini has no VeyraGreyboxSettings " + name)


def toon_presentation():
    """What the game's presentation gives the toon bodies (ADR-068 §2-3), which an editor world does not run: the ink pass
    and its stencil, and the toon light set from the map's brightest sun as the game sets it."""
    ink = unreal.load_asset(greybox_setting("ToonInkMaterial"))
    assert ink, "The toon ink does not load; run BuildPresentationMaterials.ps1"
    stencil = round(unreal.MaterialEditingLibrary.get_material_default_scalar_parameter_value(ink, greybox_setting("ToonInkStencilParameter")))
    assert stencil > 0, "The toon ink has no stencil"
    assert unreal.VeyraToonLight.light_by_sun(WORLD), "The battleground has no sun to light the toon bodies by"
    return ink, stencil


INK, INK_STENCIL = toon_presentation()


def stage():
    """The Bottom lane's straight run along +Y: its centre line, clear of base and river."""
    lane = next(lane for lane in WORLD_LAYOUT["lanes"] if lane["lane"] == "Bottom")
    start, turn = lane["points"][0], lane["points"][1]
    assert start["x"] == turn["x"], "The Bottom lane's first run no longer runs along +Y"
    along = start["y"] + (turn["y"] - start["y"]) * 0.25
    return standing(start["x"], along), lane["width"]


def standing(x, y):
    """Where a unit's feet stand at (x, y) on the loaded battleground's playable ground, as the game places units."""
    result = unreal.VeyraWorldToolsLibrary.standing_point(WORLD, unreal.Vector2D(x, y))
    found, location = result if isinstance(result, tuple) else (result is not None, result)
    assert found, f"No playable ground under ({x}, {y})"
    return location


def clip_seconds(asset, clip, share):
    entry = next(animation for animation in asset["animations"] if animation["name"] == clip)
    return (entry["frames"] - 1) / KIT["fps"] * share


def spawn_rows(asset):
    folder = asset["name"][len("SK_"):]
    root = f"/Game/Veyra/Vanguards/{folder}"
    mesh = unreal.load_asset(f"{root}/{asset['name']}")
    assert mesh, f"No body imported for {asset['id']}"
    extent = mesh.get_bounds().box_extent
    gap = max(extent.x, extent.y, extent.z) * 2.0 * POSE_GAP
    centre, width = stage()
    spawned = []
    for row, yaw in enumerate(ROWS):
        x = centre.x + (row - (len(ROWS) - 1) / 2) * width * ROW_GAP
        for index, (clip, share) in enumerate(POSES):
            y = centre.y + (index - (len(POSES) - 1) / 2) * gap
            # Each on the ground under it: the lane is level only where the terrain lets it be.
            actor = ACTORS.spawn_actor_from_class(unreal.SkeletalMeshActor, standing(x, y), unreal.Rotator(0.0, 0.0, yaw))
            component = actor.skeletal_mesh_component
            component.set_skeletal_mesh_asset(mesh)
            # Inked as a character is in the game.
            component.set_render_custom_depth(True)
            component.set_custom_depth_stencil_value(INK_STENCIL)
            sequence =unreal.load_asset(f"{root}/AS_{folder}_Armature_{clip}")
            assert sequence, f"{asset['id']} has no {clip}"
            # An editor world evaluates a body's animation only when asked to.
            component.set_update_animation_in_editor(True)
            seconds = clip_seconds(asset, clip, share)
            # It switches the body to play one sequence, and so (only on that switch) gives the sequence to the player.
            component.override_animation_data(sequence, False, False, seconds, 0.0)
            # Held at its moment.
            component.set_position(seconds, False)
            # What it is made of where no mesh shows it (smoke) pours off its bones, as the game pours it.
            if asset.get("effect"):
                system = unreal.load_asset(asset["effect"]["system"])
                assert system, f"{asset['name']}'s effect does not load"
                for bone in asset["effect"]["bones"]:
                    effect = unreal.NiagaraFunctionLibrary.spawn_system_attached(system, component, bone, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0),
                                                                                unreal.AttachLocation.SNAP_TO_TARGET, False)
                    assert effect, f"{asset['name']}'s effect did not spawn at {bone}"
                    effect.set_variable_linear_color("Color", unreal.LinearColor(*asset["effect"]["color"]))
                    effect.set_variable_float("Scale", asset["effect"]["scale"])
                    EFFECTS.append(effect)
            spawned.append(actor)
    return spawned, centre, gap * len(POSES)


def camera_at(centre, fov, distance):
    """A camera at the gameplay camera's pitch, looking along +X at centre from distance."""
    pitch = float(CAMERA["PitchDegrees"])
    forward = unreal.Vector(math.cos(math.radians(pitch)), 0.0, math.sin(math.radians(pitch)))
    camera = ACTORS.spawn_actor_from_class(unreal.CameraActor, centre - forward * distance, unreal.Rotator(0.0, pitch, 0.0))
    camera.camera_component.set_editor_property("field_of_view", fov)
    camera.camera_component.set_editor_property("constrain_aspect_ratio", False)
    # The ink pass, as the game's camera carries it.
    settings = camera.camera_component.get_editor_property("post_process_settings")
    blendables = settings.get_editor_property("weighted_blendables")
    blendables.set_editor_property("array", [unreal.WeightedBlendable(1.0, INK)])
    settings.set_editor_property("weighted_blendables", blendables)
    camera.camera_component.set_editor_property("post_process_settings", settings)
    return camera


def plan():
    """Every body of each Vanguard named: its own, then those it wears while it holds a status (a rider's ride)."""
    shots = []
    for vanguard in IDS:
        bodies = [entry for entry in MANIFEST["assets"] if entry["id"] == vanguard]
        assert bodies, f"{vanguard} has no generated body in the manifest"
        shots += sorted(bodies, key=lambda entry: entry.get("status", ""))
    return shots


# The effects the current rows pour (a body of smoke), started again once their system has compiled, and how many
# frames each is simulated ahead before the shot (two seconds).
EFFECTS = []
WARM_TICKS = 60
# How many seconds an effect may take to start (its system compiling) before the capture gives up on it.
COMPILE_TRIES = 120
STATE = {"queue": [],"task": None, "next": time.monotonic() + 15, "started": time.monotonic(), "captures": [], "spawned": []}
for asset in plan():
    STATE["queue"].append(("rows", asset))
    # Poured again before each shot: particles live about a second, and nothing else moves them on between shots.
    STATE["queue"].append(("pour", asset))
    if len(STATE["queue"]) == 2:
        # The session's first shot draws no particles: one is taken and thrown away before any that is kept.
        STATE["queue"].append(("Warmup", asset))
        STATE["queue"].append(("pour", asset))
    STATE["queue"].append(("Game", asset))
    STATE["queue"].append(("pour", asset))
    STATE["queue"].append(("Close", asset))
    STATE["queue"].append(("clear", asset))


def tick(delta):
    try:
        if time.monotonic() - STATE["started"] > 900:
            raise RuntimeError("Vanguard capture timed out")
        if time.monotonic() < STATE["next"]:
            return
        if STATE["task"] and not Path(STATE["task"]).is_file():
            return
        STATE["task"] = None
        if not STATE["queue"]:
            (OUTPUT / "manifest.json").write_text(json.dumps({"status": "captured", "captures": STATE["captures"]}, indent=2), encoding="utf-8")
            unreal.unregister_slate_post_tick_callback(HANDLE)
            unreal.SystemLibrary.quit_editor()
            return
        step, asset = STATE["queue"].pop(0)
        if step == "rows":
            EFFECTS.clear()
            STATE["spawned"], STATE["centre"], STATE["span"] = spawn_rows(asset)
            # Let the poses evaluate and the lighting settle on them.
            STATE["next"] = time.monotonic() + 5
        elif step == "pour":
            # An effect spawned while its system still compiled (on the editor's first load of it) did not start: start
            # it now, and let its particles fill out before the shot. A cooked game ships its systems compiled.
            # An editor world does not tick them either, so each is simulated ahead by hand to where a moving body's
            # would be.
            for effect in EFFECTS:
                if not effect.is_active():
                    effect.activate(True)
            # One that will not start yet is still compiling: try again shortly, rather than shoot an empty body.
            if any(not effect.is_active() for effect in EFFECTS):
                STATE["compiling"] = STATE.get("compiling", 0) + 1
                assert STATE["compiling"] <= COMPILE_TRIES, f"{asset['name']}'s effect never started"
                STATE["queue"].insert(0, (step, asset))
                STATE["next"] = time.monotonic() + 1
                return
            STATE["compiling"] = 0
            for effect in EFFECTS:
                effect.advance_simulation(WARM_TICKS, 1.0 / KIT["fps"])
            STATE["next"] = time.monotonic() + (2 if EFFECTS else 0)
        elif step == "clear":
            for actor in STATE["spawned"]:
                ACTORS.destroy_actor(actor)
            STATE["spawned"] = []
        else:
            game = step in ("Game", "Warmup")
            fov = GAME_FOV if game else CLOSE_FOV
            # The game view from the camera's own distance; the close one from as far as frames both rows.
            distance = float(CAMERA["Distance"]) if game else STATE["span"] * 0.62 / math.tan(math.radians(fov / 2))
            camera = camera_at(STATE["centre"], fov, distance)
            STATE["spawned"].append(camera)
            LEVEL.pilot_level_actor(camera)
            filename = OUTPUT / ("warmup.png" if step == "Warmup" else f"{asset['name']}_{step}.png")
            if filename.exists():
                filename.unlink()
            unreal.SystemLibrary.execute_console_command(WORLD, f'HighResShot 1920x1080 filename="{filename.as_posix()}"')
            STATE["task"] = str(filename)
            if step != "Warmup":
                STATE["captures"].append({"vanguard": asset["id"], "view": step, "file": str(filename), "fieldOfView": fov, "distance": round(distance)})
            STATE["next"] = time.monotonic() + 3
    except Exception:
        (OUTPUT / "failure.txt").write_text(traceback.format_exc(), encoding="utf-8")
        unreal.unregister_slate_post_tick_callback(HANDLE)
        unreal.SystemLibrary.quit_editor()


HANDLE = unreal.register_slate_post_tick_callback(tick)
