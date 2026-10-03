"""Synthesise the presentation's placeholder cue sounds (ADR-063 section 5) from ArtSource/Presentation/CueSounds.json.

Plain Python, no Unreal: writes one 16-bit mono WAV per sound to ArtSource/Presentation/Audio and a manifest of their
hashes. The same spec, seed and generator version always write the same bytes. BuildCueSounds.ps1 runs it, then imports.
"""
import hashlib
import json
import math
import random
import struct
import sys
import wave
from pathlib import Path

GENERATOR_VERSION = 1
GAME = Path(__file__).resolve().parents[1]
SOURCE = GAME / "ArtSource" / "Presentation"
SPEC_FILE = SOURCE / "CueSounds.json"
OUTPUT = SOURCE / "Audio"


def envelope(t, seconds, attack=0.004):
    """A quick fade in, so no sound starts with a click of its own, and a fade to silence at its end."""
    rise = min(1.0, t / attack)
    fall = max(0.0, 1.0 - t / seconds)
    return rise * fall


def impact(spec, rate, rng):
    noise_decay, thump_decay = spec["noiseDecaySeconds"], spec["thumpDecaySeconds"]
    for i in range(int(spec["seconds"] * rate)):
        t = i / rate
        noise = rng.uniform(-1.0, 1.0) * math.exp(-t / noise_decay)
        thump = math.sin(2.0 * math.pi * spec["thumpHz"] * t) * math.exp(-t / thump_decay)
        yield (0.55 * noise + 0.75 * thump) * envelope(t, spec["seconds"])


def swing(spec, rate, rng):
    # Noise smoothed into a whoosh that swells and passes.
    smoothed = 0.0
    for i in range(int(spec["seconds"] * rate)):
        t = i / rate
        smoothed += spec["smoothing"] * (rng.uniform(-1.0, 1.0) - smoothed)
        swell = math.sin(math.pi * t / spec["seconds"])
        yield 2.5 * smoothed * swell


def cast(spec, rate, rng):
    phase = 0.0
    for i in range(int(spec["seconds"] * rate)):
        t = i / rate
        progress = t / spec["seconds"]
        frequency = spec["fromHz"] + (spec["toHz"] - spec["fromHz"]) * progress
        frequency *= 1.0 + spec["vibratoDepth"] * math.sin(2.0 * math.pi * spec["vibratoHz"] * t)
        phase += 2.0 * math.pi * frequency / rate
        tone = math.sin(phase) + 0.35 * math.sin(2.0 * phase)
        yield 0.7 * tone * envelope(t, spec["seconds"], attack=0.02)


def death(spec, rate, rng):
    phase = 0.0
    for i in range(int(spec["seconds"] * rate)):
        t = i / rate
        progress = t / spec["seconds"]
        # Falling on a curve, as a thing that gives way.
        frequency = spec["toHz"] + (spec["fromHz"] - spec["toHz"]) * (1.0 - progress) ** 2
        phase += 2.0 * math.pi * frequency / rate
        tone = (1.0 - spec["noiseShare"]) * math.sin(phase) + spec["noiseShare"] * rng.uniform(-1.0, 1.0)
        yield tone * envelope(t, spec["seconds"], attack=0.01)


def click(spec, rate, rng):
    for i in range(int(spec["seconds"] * rate)):
        t = i / rate
        yield math.sin(2.0 * math.pi * spec["hz"] * t) * math.exp(-t / (spec["seconds"] / 4.0))


KINDS = {"impact": impact, "swing": swing, "cast": cast, "death": death, "click": click}


def main():
    spec_bytes = SPEC_FILE.read_bytes()
    spec = json.loads(spec_bytes)
    assert spec["schemaVersion"] == 1, "Unknown spec schema"
    assert spec["generatorVersion"] == GENERATOR_VERSION, "The spec was written for another generator version"
    rate = spec["sampleRate"]
    assert rate in (22050, 44100, 48000)
    names = [sound["name"] for sound in spec["sounds"]]
    assert len(set(names)) == len(names), "Each sound needs its own name"
    OUTPUT.mkdir(parents=True, exist_ok=True)
    manifest = {"generatorVersion": GENERATOR_VERSION, "specSha256": hashlib.sha256(spec_bytes).hexdigest(), "sounds": []}
    for index, sound in enumerate(spec["sounds"]):
        assert sound["kind"] in KINDS, "Unknown kind: " + sound["kind"]
        assert 0.0 < sound["gain"] <= 1.0 and 0.0 < sound["seconds"] <= 2.0
        # Each sound its own stream from the seed, so adding one never changes another.
        rng = random.Random(spec["seed"] * 1000 + index)
        samples = [max(-1.0, min(1.0, value * sound["gain"])) for value in KINDS[sound["kind"]](sound, rate, rng)]
        frames = b"".join(struct.pack("<h", int(round(value * 32767))) for value in samples)
        target = OUTPUT / (sound["name"] + ".wav")
        with wave.open(str(target), "wb") as out:
            out.setnchannels(1)
            out.setsampwidth(2)
            out.setframerate(rate)
            out.writeframes(frames)
        manifest["sounds"].append({"name": sound["name"], "file": "Audio/" + target.name,
                                   "sha256": hashlib.sha256(target.read_bytes()).hexdigest(), "samples": len(samples)})
        print("VEYRA_CUE_SOUND: " + target.name)
    (SOURCE / "CueSounds.manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
