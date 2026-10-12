"""Turns a skill showcase recording into clips (ADR-074 §5).

The recorder (UVeyraShowcaseRecorderSubsystem, through RecordShowcase.ps1) writes a folder holding manifest.json and
each ability's frames as <ability>/<index>.png. This script writes, for each ability, <ability>.gif (Pillow) and
<ability>.mp4 (H.264, through the ffmpeg imageio-ffmpeg bundles), each frame shown for as long as it lasted on the
server, so a clip plays at the speed its cast happened. The MP4 keeps every frame; the GIF keeps fewer (--gif-fps), as
its frames are large. Its dependencies are pinned in requirements.txt beside it.

    python Game/Scripts/Showcase/encode_showcase.py <recording folder> [--out <folder>] [--gif-width <pixels>] [--gif-fps <rate>]
"""
from __future__ import annotations

import argparse
import json
import statistics
import subprocess
import sys
import tempfile
from pathlib import Path

MANIFEST_SCHEMA_VERSION = 1


def frame_durations(times: list[float]) -> list[float]:
    """How long each frame shows, in seconds: until the next frame's time. The last shows as long as a typical frame.

    A frame whose time is not after the one before it (a server clock estimate that stood still) shows as long as a
    typical frame too, so no frame vanishes and the clip keeps its length.
    """
    if not times:
        return []
    steps = [later - earlier for earlier, later in zip(times, times[1:]) if later > earlier]
    typical = statistics.median(steps) if steps else 1.0 / 30.0
    durations = []
    for index, time in enumerate(times):
        following = times[index + 1] - time if index + 1 < len(times) else 0.0
        durations.append(following if following > 0.0 else typical)
    return durations


def resample(durations: list[float], fps: float) -> list[tuple[int, float]]:
    """Which frames a clip keeps at fps, and how long each shows: a frame is kept once the clip's time has reached the
    next tick, and shows until the next kept frame. The clip's length is unchanged. A non-positive fps keeps all."""
    if fps <= 0.0:
        return list(enumerate(durations))
    step = 1.0 / fps
    kept: list[tuple[int, float]] = []
    elapsed = 0.0
    due = 0.0
    for index, duration in enumerate(durations):
        if elapsed >= due - 1e-9 or not kept:
            kept.append((index, duration))
            due = elapsed + step
        else:
            kept[-1] = (kept[-1][0], kept[-1][1] + duration)
        elapsed += duration
    return kept


def concat_list(files: list[Path], durations: list[float]) -> str:
    """An ffmpeg concat-demuxer script showing each file for its duration. The last file is named twice, as the
    demuxer needs, so its duration is kept."""
    if len(files) != len(durations) or not files:
        raise ValueError("each frame needs a duration")
    lines = ["ffconcat version 1.0"]
    for file, duration in zip(files, durations):
        lines.append(f"file '{file.as_posix()}'")
        lines.append(f"duration {duration:.6f}")
    lines.append(f"file '{files[-1].as_posix()}'")
    return "\n".join(lines) + "\n"


def read_manifest(folder: Path) -> dict:
    """The recording's manifest, checked for what this script reads."""
    manifest = json.loads((folder / "manifest.json").read_text(encoding="utf-8"))
    if manifest.get("schemaVersion") != MANIFEST_SCHEMA_VERSION:
        raise ValueError(f"manifest.json: schemaVersion {manifest.get('schemaVersion')} is not {MANIFEST_SCHEMA_VERSION}")
    for clip in manifest.get("clips", []):
        if not clip.get("ability") or not isinstance(clip.get("frames"), list):
            raise ValueError("manifest.json: each clip needs an ability and its frames")
    return manifest


def write_gif(files: list[Path], durations: list[float], target: Path, width: int, fps: float) -> None:
    from PIL import Image

    kept = resample(durations, fps)
    files = [files[index] for index, _ in kept]
    durations = [duration for _, duration in kept]
    frames = []
    for file in files:
        with Image.open(file) as image:
            frame = image.convert("RGB")
            if width and frame.width > width:
                frame = frame.resize((width, round(frame.height * width / frame.width)), Image.Resampling.LANCZOS)
            frames.append(frame)
    # GIF frame times are in hundredths of a second; Pillow takes milliseconds.
    milliseconds = [max(20, round(duration * 1000.0)) for duration in durations]
    frames[0].save(target, save_all=True, append_images=frames[1:], duration=milliseconds, loop=0, optimize=True)


def write_mp4(files: list[Path], durations: list[float], target: Path) -> None:
    import imageio_ffmpeg

    with tempfile.TemporaryDirectory() as scratch:
        script = Path(scratch) / "frames.ffconcat"
        script.write_text(concat_list(files, durations), encoding="utf-8")
        command = [imageio_ffmpeg.get_ffmpeg_exe(), "-y", "-loglevel", "error", "-f", "concat", "-safe", "0", "-i", str(script),
                   "-fps_mode", "vfr", "-c:v", "libx264", "-pix_fmt", "yuv420p", "-vf", "pad=ceil(iw/2)*2:ceil(ih/2)*2", str(target)]
        subprocess.run(command, check=True)


def encode(folder: Path, out: Path, gif_width: int, gif_fps: float = 20.0, missing: list[str] | None = None) -> list[Path]:
    """Writes each clip the manifest lists; the abilities with nothing to encode, as the recorder left one out or its
    frames were never written, are added to missing."""
    manifest = read_manifest(folder)
    out.mkdir(parents=True, exist_ok=True)
    written = []
    for clip in manifest["clips"]:
        if clip.get("failure"):
            print(f"{clip['ability']}: the recorder left it out: {clip['failure']}")
            if missing is not None:
                missing.append(clip["ability"])
            continue
        files = [folder / frame["file"] for frame in clip["frames"]]
        files_present = [file for file in files if file.is_file()]
        if not files_present:
            print(f"{clip['ability']}: no frames were written.")
            if missing is not None:
                missing.append(clip["ability"])
            continue
        times = [frame["time"] for frame, file in zip(clip["frames"], files) if file.is_file()]
        durations = frame_durations(times)
        for suffix, writer in ((".gif", lambda target: write_gif(files_present, durations, target, gif_width, gif_fps)),
                               (".mp4", lambda target: write_mp4(files_present, durations, target))):
            target = out / f"{clip['ability']}{suffix}"
            writer(target)
            written.append(target)
        print(f"{clip['ability']}: {len(files_present)} frame(s) over {sum(durations):.2f} s.")
    return written


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("recording", type=Path, help="the folder the recorder wrote: manifest.json and the frames")
    parser.add_argument("--out", type=Path, help="where the clips go; the recording's parent folder unless given")
    parser.add_argument("--gif-width", type=int, default=640, help="the GIFs' width in pixels, to keep them small; 0 keeps the frames' own")
    parser.add_argument("--gif-fps", type=float, default=20.0, help="the GIFs' frame rate, to keep them small; 0 keeps every frame")
    arguments = parser.parse_args(argv)
    missing: list[str] = []
    written = encode(arguments.recording, arguments.out or arguments.recording.parent, arguments.gif_width, arguments.gif_fps, missing)
    print(f"Wrote {len(written)} file(s).")
    # A recording passes only whole: every clip the manifest lists encoded.
    if missing:
        print(f"Missing {len(missing)} clip(s): {', '.join(missing)}.")
    return 0 if written and not missing else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
