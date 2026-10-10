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
# Lit: the scene as players see it. Collision: what blocks a pawn (generated dressing must not appear). Dressing:
# the terrain hidden, so only generated instances and the river remain.
MODE = os.environ.get("VEYRA_REVIEW_MODE", "Lit")
MODE_COMMANDS = {"Lit": [], "Collision": [], "Dressing": ["ShowFlag.Landscape 0"]}
# View modes reach the level viewport through the automation library; a "viewmode" console command does not.
MODE_VIEWS = {"Collision": unreal.ViewModeIndex.VMI_COLLISION_PAWN}
assert MODE in MODE_COMMANDS, f"Unknown capture mode {MODE}"
OUTPUT = Path(os.environ["VEYRA_REVIEW_OUTPUT"]) if os.environ.get("VEYRA_REVIEW_OUTPUT") else GAME / "Saved" / "WorldReview" / os.environ.get("VEYRA_REVIEW_PROFILE", "high")
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
# The mode's console commands, then any extra ones for a diagnostic capture, separated by semicolons.
for command in MODE_COMMANDS[MODE] + list(filter(None, os.environ.get("VEYRA_REVIEW_COMMANDS", "").split(";"))):
    unreal.SystemLibrary.execute_console_command(WORLD, command.strip())
if MODE in MODE_VIEWS:
    unreal.AutomationLibrary.set_editor_viewport_view_mode(MODE_VIEWS[MODE])
ACTORS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
CAMERAS = sorted([a for a in ACTORS if isinstance(a, unreal.CameraActor) and "Veyra.ReviewCamera" in [str(t) for t in a.tags]], key=lambda a: a.get_actor_label())
FILTER = os.environ.get("VEYRA_REVIEW_VIEWS", "")
if FILTER:
    CAMERAS = [a for a in CAMERAS if a.get_actor_label().removeprefix("Review_") in FILTER.split(",")]
assert CAMERAS, "No requested review cameras found"
# Budget for both preparation passes and each screenshot across the full camera suite.
CAPTURE_TIMEOUT_SECONDS = max(900, 120 + len(CAMERAS) * 30)
STATE = {"index": 0, "task": None, "readiness_pass": 0, "compiling": False, "next": time.monotonic() + 15, "started": time.monotonic(), "captures": []}


def tick(delta):
    # Compilation can pump Slate ticks while waiting on worker processes.
    if STATE["compiling"]:
        return
    try:
        if time.monotonic() - STATE["started"] > CAPTURE_TIMEOUT_SECONDS:
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
            (OUTPUT / "manifest.json").write_text(json.dumps({"status": "captured", "profile": PROFILE, "mode": MODE, "resolution": [1920, 1080], "renderScalePercent": 100, "captures": STATE["captures"]}, indent=2), encoding="utf-8")
            unreal.unregister_slate_post_tick_callback(HANDLE)
            unreal.SystemLibrary.quit_editor()
            return
        camera = CAMERAS[STATE["index"]]
        if STATE["readiness_pass"] < 2:
            if STATE["readiness_pass"] == 0:
                LEVEL.pilot_level_actor(camera)
            # Let the new view render between barriers so deferred material jobs
            # triggered by its first frames are also complete before the screenshot.
            unreal.log(f"Preparing {camera.get_actor_label()}: compilation pass {STATE['readiness_pass'] + 1}")
            STATE["compiling"] = True
            try:
                unreal.SystemLibrary.execute_console_command(WORLD, "Editor.AsyncAssetCompilationFinishAll")
            finally:
                STATE["compiling"] = False
            STATE["readiness_pass"] += 1
            STATE["next"] = time.monotonic() + 5
            return
        filename = OUTPUT / (camera.get_actor_label() + ".png")
        if filename.exists():
            filename.unlink()
        unreal.log(f"Capturing {camera.get_actor_label()}")
        unreal.SystemLibrary.execute_console_command(WORLD, f'HighResShot 1920x1080 filename="{filename.as_posix()}"')
        task = str(filename)
        STATE["task"] = task
        STATE["index"] += 1
        STATE["readiness_pass"] = 0
        STATE["next"] = time.monotonic() + 5
        STATE["captures"].append({"camera": camera.get_actor_label(), "file": str(filename), "location": str(camera.get_actor_location()), "rotation": str(camera.get_actor_rotation())})
    except Exception:
        (OUTPUT / "failure.txt").write_text(traceback.format_exc(), encoding="utf-8")
        unreal.unregister_slate_post_tick_callback(HANDLE)
        unreal.SystemLibrary.quit_editor()


HANDLE = unreal.register_slate_post_tick_callback(tick)
