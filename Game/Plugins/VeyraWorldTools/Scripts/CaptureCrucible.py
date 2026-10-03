"""Capture stable generated review cameras using the real editor renderer.

Run through Game/Scripts/CaptureBattleground.ps1. No map assets are edited.
"""
import json
import os
import time
import traceback
from pathlib import Path
import unreal

unreal.EditorPythonScripting.set_keep_python_script_alive(True)

GAME = Path(unreal.Paths.project_dir()).resolve()
OUTPUT = GAME / "Saved" / "WorldReview" / os.environ.get("VEYRA_REVIEW_PROFILE", "high")
OUTPUT.mkdir(parents=True, exist_ok=True)
LEVEL = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert LEVEL.load_level("/Game/Veyra/World/Maps/L_Battleground")
LEVEL.editor_set_game_view(True)
WORLD = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
PROFILE = os.environ.get("VEYRA_REVIEW_PROFILE", "high")
QUALITY = 0 if PROFILE == "low" else 2
for name in ("ViewDistance", "AntiAliasing", "Shadow", "GlobalIllumination", "Reflection", "PostProcess", "Texture", "Effects", "Foliage", "Shading"):
    unreal.SystemLibrary.execute_console_command(WORLD, f"sg.{name}Quality {QUALITY}")
unreal.SystemLibrary.execute_console_command(WORLD, "r.ScreenPercentage 100")
unreal.SystemLibrary.execute_console_command(WORLD, "r.VSync 0")
ACTORS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
CAMERAS = sorted([a for a in ACTORS if isinstance(a, unreal.CameraActor) and "Veyra.ReviewCamera" in [str(t) for t in a.tags]], key=lambda a: a.get_actor_label())
FILTER = os.environ.get("VEYRA_REVIEW_VIEWS", "")
if FILTER:
    CAMERAS = [a for a in CAMERAS if a.get_actor_label().removeprefix("Review_") in FILTER.split(",")]
assert CAMERAS, "No requested review cameras found"
STATE = {"index": 0, "task": None, "next": time.monotonic() + 15, "started": time.monotonic(), "captures": []}


def tick(delta):
    try:
        if time.monotonic() - STATE["started"] > 900:
            raise RuntimeError("World capture timed out")
        if time.monotonic() < STATE["next"]:
            return
        task = STATE["task"]
        if task and not Path(task).is_file():
            return
        if task:
            previous = STATE["captures"][-1]
            assert Path(previous["file"]).is_file(), "Screenshot task finished without an image"
            STATE["task"] = None
        if STATE["index"] >= len(CAMERAS):
            (OUTPUT / "manifest.json").write_text(json.dumps({"status": "captured", "profile": PROFILE, "resolution": [1920, 1080], "renderScalePercent": 100, "captures": STATE["captures"]}, indent=2), encoding="utf-8")
            unreal.unregister_slate_post_tick_callback(HANDLE)
            unreal.SystemLibrary.quit_editor()
            return
        camera = CAMERAS[STATE["index"]]
        filename = OUTPUT / (camera.get_actor_label() + ".png")
        if filename.exists():
            filename.unlink()
        LEVEL.pilot_level_actor(camera)
        unreal.log(f"Capturing {camera.get_actor_label()}")
        unreal.SystemLibrary.execute_console_command(WORLD, f'HighResShot 1920x1080 filename="{filename.as_posix()}"')
        task = str(filename)
        STATE["task"] = task
        STATE["index"] += 1
        STATE["next"] = time.monotonic() + 5
        STATE["captures"].append({"camera": camera.get_actor_label(), "file": str(filename), "location": str(camera.get_actor_location()), "rotation": str(camera.get_actor_rotation())})
    except Exception:
        (OUTPUT / "failure.txt").write_text(traceback.format_exc(), encoding="utf-8")
        unreal.unregister_slate_post_tick_callback(HANDLE)
        unreal.SystemLibrary.quit_editor()


HANDLE = unreal.register_slate_post_tick_callback(tick)
