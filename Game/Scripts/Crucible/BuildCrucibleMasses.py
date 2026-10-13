"""Writes World.json's generated rock and forest masses from its authored open spaces (ADR-073 §2).

    python Game/Scripts/Crucible/BuildCrucibleMasses.py           # rewrite layout.masses and its inputSha256
    python Game/Scripts/Crucible/BuildCrucibleMasses.py --check   # exit 1 if layout.masses is stale or differs

Only this script writes layout.masses. Edit the open spaces (keep, clearings, routes, river, lanes, massRules), then run
it. Needs shapely (requirements.txt beside this file).
"""
from __future__ import annotations

import sys

from crucible_geometry import derive_masses, input_sha256, load_world, write_world


def main(argv: list[str]) -> int:
    world = load_world()
    expected = {"inputSha256": input_sha256(world), "items": derive_masses(world)}
    if "--check" in argv:
        actual = world["layout"].get("masses", {})
        if actual.get("inputSha256") != expected["inputSha256"]:
            print("layout.masses is stale: its inputs changed. Run Game/Scripts/Crucible/BuildCrucibleMasses.py.")
            return 1
        if actual.get("items") != expected["items"]:
            print("layout.masses differs from what its inputs derive: never edit it by hand.")
            return 1
        print(f"layout.masses is current: {len(expected['items'])} masses.")
        return 0
    world["layout"]["masses"] = expected
    write_world(world)
    kinds = {}
    for mass in expected["items"]:
        kinds[mass["kind"]] = kinds.get(mass["kind"], 0) + 1
    print(f"Wrote {len(expected['items'])} masses {kinds}, inputs {expected['inputSha256'][:16]}.")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
