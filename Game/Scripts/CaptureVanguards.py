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


def stage():
    """The Bottom lane's straight run along +Y: its centre line, level at the lane's height, clear of base and river."""
    lane = next(lane for lane in WORLD_LAYOUT["lanes"] if lane["lane"] == "Bottom")
    start, turn = lane["points"][0], lane["points"][1]
    assert start["x"] == turn["x"], "The Bottom lane's first run no longer runs along +Y"
    along = start["y"] + (turn["y"] - start["y"]) * 0.25
    return unreal.Vector(start["x"], along, WORLD_LAYOUT["terrain"]["laneZ"]), lane["width"]


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
            actor = ACTORS.spawn_actor_from_class(unreal.SkeletalMeshActor, unreal.Vector(x, y, centre.z), unreal.Rotator(0.0, 0.0, yaw))
            component = actor.skeletal_mesh_component
            component.set_skeletal_mesh_asset(mesh)
            sequence = unreal.load_asset(f"{root}/AS_{folder}_Armature_{clip}")
            assert sequence, f"{asset['id']} has no {clip}"
            # An editor world evaluates a body's animation only when asked to.
            component.set_update_animation_in_editor(True)
            seconds = clip_seconds(asset, clip, share)
            # It switches the body to play one sequence, and so (only on that switch) gives the sequence to the player.
            component.override_animation_data(sequence, False, False, seconds, 0.0)
            # Held at its moment.
            component.set_position(seconds, False)
            spawned.append(actor)
    return spawned, centre, gap * len(POSES)


def camera_at(centre, fov, distance):
    """A camera at the gameplay camera's pitch, looking along +X at centre from distance."""
    pitch = float(CAMERA["PitchDegrees"])
    forward = unreal.Vector(math.cos(math.radians(pitch)), 0.0, math.sin(math.radians(pitch)))
    camera = ACTORS.spawn_actor_from_class(unreal.CameraActor, centre - forward * distance, unreal.Rotator(0.0, pitch, 0.0))
    camera.camera_component.set_editor_property("field_of_view", fov)
    camera.camera_component.set_editor_property("constrain_aspect_ratio", False)
    return camera


def plan():
    """Every body of each Vanguard named: its own, then those it wears while it holds a status (a rider's ride)."""
    shots = []
    for vanguard in IDS:
        bodies = [entry for entry in MANIFEST["assets"] if entry["id"] == vanguard]
        assert bodies, f"{vanguard} has no generated body in the manifest"
        shots += sorted(bodies, key=lambda entry: entry.get("status", ""))
    return shots


STATE = {"queue": [], "task": None, "next": time.monotonic() + 15, "started": time.monotonic(), "captures": [], "spawned": []}
for asset in plan():
    STATE["queue"].append(("rows", asset))
    STATE["queue"].append(("Game", asset))
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
            STATE["spawned"], STATE["centre"], STATE["span"] = spawn_rows(asset)
            # Let the poses evaluate and the lighting settle on them.
            STATE["next"] = time.monotonic() + 5
        elif step == "clear":
            for actor in STATE["spawned"]:
                ACTORS.destroy_actor(actor)
            STATE["spawned"] = []
        else:
            fov = GAME_FOV if step == "Game" else CLOSE_FOV
            # The game view from the camera's own distance; the close one from as far as frames both rows.
            distance = float(CAMERA["Distance"]) if step == "Game" else STATE["span"] * 0.62 / math.tan(math.radians(fov / 2))
            camera = camera_at(STATE["centre"], fov, distance)
            STATE["spawned"].append(camera)
            LEVEL.pilot_level_actor(camera)
            filename = OUTPUT / f"{asset['name']}_{step}.png"
            if filename.exists():
                filename.unlink()
            unreal.SystemLibrary.execute_console_command(WORLD, f'HighResShot 1920x1080 filename="{filename.as_posix()}"')
            STATE["task"] = str(filename)
            STATE["captures"].append({"vanguard": asset["id"], "view": step, "file": str(filename), "fieldOfView": fov, "distance": round(distance)})
            STATE["next"] = time.monotonic() + 3
    except Exception:
        (OUTPUT / "failure.txt").write_text(traceback.format_exc(), encoding="utf-8")
        unreal.unregister_slate_post_tick_callback(HANDLE)
        unreal.SystemLibrary.quit_editor()


HANDLE = unreal.register_slate_post_tick_callback(tick)
