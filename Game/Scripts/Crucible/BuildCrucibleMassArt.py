"""Writes the forest masses' canopy (ADR-073 §5) for VeyraWorldTools from World.json's masses and CrucibleStyle.json's massArt.

    python Game/Scripts/Crucible/BuildCrucibleMassArt.py           # rewrite Config/CrucibleMassArt.json
    python Game/Scripts/Crucible/BuildCrucibleMassArt.py --check   # exit 1 if it is stale or differs

Run it after BuildCrucibleMasses.py, or after editing massArt. Needs shapely (requirements.txt beside this file).
"""
from __future__ import annotations

import json
import sys

from crucible_art import ART_JSON, derive_art, load_style, profile_sha256
from crucible_geometry import format_tuning, load_world


def main(argv: list[str]) -> int:
    world = load_world()
    style = load_style()
    if "--check" in argv:
        actual = json.loads(ART_JSON.read_text(encoding="utf-8")) if ART_JSON.exists() else {}
        if actual.get("massesInputSha256") != world["layout"]["masses"]["inputSha256"] or actual.get("profileSha256") != profile_sha256(style):
            print("CrucibleMassArt.json is stale: the masses or massArt changed. Run Game/Scripts/Crucible/BuildCrucibleMassArt.py.")
            return 1
        if actual != derive_art(world, style):
            print("CrucibleMassArt.json differs from what its inputs derive: never edit it by hand.")
            return 1
        print(f"CrucibleMassArt.json is current: {len(actual['masses'])} masses.")
        return 0
    art = derive_art(world, style)
    ART_JSON.write_text(format_tuning(art) + "\n", encoding="utf-8", newline="\n")
    canopy = sum(len(m["canopy"]) for m in art["masses"])
    print(f"Wrote the canopy of {len(art['masses'])} masses: {canopy} pieces.")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
