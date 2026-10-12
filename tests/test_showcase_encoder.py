"""Tests for the skill showcase encoder (ADR-074 §5): clips play at the speed their casts happened."""
from __future__ import annotations

import importlib.util
import json
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "Game" / "Scripts" / "Showcase" / "encode_showcase.py"
SPEC = importlib.util.spec_from_file_location("encode_showcase", SCRIPT)
encoder = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(encoder)


class FrameDurations(unittest.TestCase):
    def test_each_frame_lasts_until_the_next(self):
        self.assertEqual(encoder.frame_durations([1.0, 1.5, 2.25]), [0.5, 0.75, 0.625])

    def test_a_frame_whose_clock_stood_still_shows_as_long_as_a_typical_one(self):
        # A server clock estimate that did not move between two frames must not drop the frame from the clip.
        durations = encoder.frame_durations([1.0, 1.1, 1.1, 1.2])
        self.assertEqual(len(durations), 4)
        self.assertTrue(all(duration > 0.0 for duration in durations))

    def test_one_frame_and_none(self):
        self.assertEqual(len(encoder.frame_durations([3.0])), 1)
        self.assertEqual(encoder.frame_durations([]), [])


class Resample(unittest.TestCase):
    def test_a_gif_keeps_fewer_frames_and_the_clips_length(self):
        durations = [1.0 / 60.0] * 60
        kept = encoder.resample(durations, 20.0)
        self.assertEqual(len(kept), 20)
        self.assertAlmostEqual(sum(duration for _, duration in kept), sum(durations))
        self.assertEqual(kept[0][0], 0)

    def test_no_rate_keeps_every_frame(self):
        self.assertEqual(encoder.resample([0.1, 0.2], 0.0), [(0, 0.1), (1, 0.2)])


class ConcatList(unittest.TestCase):
    def test_every_frame_has_its_duration_and_the_last_is_named_again(self):
        script = encoder.concat_list([Path("a/0000.png"), Path("a/0001.png")], [0.5, 0.25])
        lines = script.splitlines()
        self.assertEqual(lines[0], "ffconcat version 1.0")
        self.assertIn("duration 0.500000", lines)
        self.assertIn("duration 0.250000", lines)
        self.assertEqual(lines[-1], "file 'a/0001.png'")

    def test_frames_and_durations_must_match(self):
        with self.assertRaises(ValueError):
            encoder.concat_list([Path("a.png")], [])


class Encode(unittest.TestCase):
    def write_recording(self, folder: Path, schema_version: int = 1) -> None:
        from PIL import Image

        frames = []
        for index, colour in enumerate(((255, 0, 0), (0, 255, 0), (0, 0, 255))):
            name = f"test_ability/{index:04d}.png"
            (folder / "test_ability").mkdir(parents=True, exist_ok=True)
            Image.new("RGBA", (64, 36), colour + (0,)).save(folder / name)
            frames.append({"file": name, "time": 10.0 + index * 0.033})
        manifest = {"schemaVersion": schema_version, "vanguard": "test_vanguard", "frameStride": 2, "width": 64, "height": 36,
                    "clips": [{"ability": "test_ability", "slot": "Q", "orderedAt": 10.0, "committedAt": 10.03, "frames": frames}]}
        (folder / "manifest.json").write_text(json.dumps(manifest), encoding="utf-8")

    def test_a_recording_becomes_a_gif_and_an_mp4_per_ability(self):
        with tempfile.TemporaryDirectory() as scratch:
            folder = Path(scratch) / "frames"
            self.write_recording(folder)
            written = encoder.encode(folder, Path(scratch), gif_width=32, gif_fps=0.0)
            self.assertEqual(sorted(path.name for path in written), ["test_ability.gif", "test_ability.mp4"])
            self.assertTrue(all(path.stat().st_size > 0 for path in written))
            from PIL import Image

            with Image.open(Path(scratch) / "test_ability.gif") as gif:
                self.assertEqual(gif.n_frames, 3)
                self.assertEqual(gif.width, 32)

    def test_a_manifest_of_another_version_is_refused(self):
        with tempfile.TemporaryDirectory() as scratch:
            folder = Path(scratch)
            self.write_recording(folder, schema_version=99)
            with self.assertRaises(ValueError):
                encoder.read_manifest(folder)


if __name__ == "__main__":
    unittest.main()
