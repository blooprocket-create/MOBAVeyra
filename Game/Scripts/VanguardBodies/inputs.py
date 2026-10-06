"""What a generated body is made from (ADR-064 §4): the inputs whose change makes it stale.

Pure Python, shared by the generator (Blender) and the importer (Unreal), so both derive the same answer. A body is
stale when its own kit entry, its archetype's settings, the kit's frame rate, the generator version or its Vanguard's
capsule and attack range class change, whichever Vanguard was rebuilt last.
"""
import hashlib
import json

GENERATOR_VERSION = 1


def bodies_of(spec):
    """A Vanguard's bodies, as (spec, status, name suffix): its own, then each it wears while it holds a status, built
    from its own entry with the status body's entries over it (a rider's ride, ADR-064 §1)."""
    yield spec, None, ""
    for status_body in spec.get("statusBodies", []):
        yield dict(spec, **status_body["body"]), status_body["status"], "_" + status_body["name"]


def units(vanguards, companions):
    """Every unit a body is fitted to, by ID, as Vanguards.json shapes a Vanguard (its body's capsule and its basic
    attack): the Vanguards, and the companions of Abilities.json, whose capsule sits on the definition itself."""
    table = dict(vanguards)
    for name, companion in companions.items():
        table[name] = {"body": {"capsuleRadius": companion["capsuleRadius"], "capsuleHalfHeight": companion["capsuleHalfHeight"]},
                       "basicAttack": companion["basicAttack"]}
    return table


def entries(kit):
    """Every entry the kit generates bodies for: its Vanguards, then its companions."""
    return list(kit["vanguards"]) + list(kit.get("companions", []))


def body_name(unit_id, suffix):
    """A body's asset name: SK_, its unit's ID in title case, then its status body's suffix (SK_Raska_Hound)."""
    return "SK_" + unit_id.title().replace("_", "") + suffix


def input_hash(kit, vanguards, body_spec):
    """The hash of everything body_spec's body is generated from: its entry, its archetype's settings, the kit's frame
    rate, the generator version, and its Vanguard's capsule and whether it fights in melee (Vanguards.json)."""
    vanguard = vanguards[body_spec["id"]]
    # Its other bodies' entries are not among its inputs: changing a ride's body leaves the rider's own as it was.
    own = {key: value for key, value in body_spec.items() if key != "statusBodies"}
    inputs = {"spec": own, "archetype": kit["archetypes"][body_spec["archetype"]], "fps": kit["fps"],
              "generatorVersion": GENERATOR_VERSION, "capsule": vanguard["body"],
              "melee": not vanguard["basicAttack"].get("projectile")}
    return hashlib.sha256(json.dumps(inputs, sort_keys=True).encode("utf-8")).hexdigest()


def stale_assets(kit, vanguards, assets):
    """Every body the manifest does not hold as the kit and Vanguards.json give it now, by name: one whose recorded
    inputs differ or that the kit no longer has, and one the kit has that the manifest lacks (a partial build that kept
    a manifest from before the kit gave a Vanguard a new body)."""
    current = {}
    for spec in entries(kit):
        for body_spec, status, suffix in bodies_of(spec):
            current[(spec["id"], status)] = (input_hash(kit, vanguards, body_spec), body_name(spec["id"], suffix))
    recorded = {(asset["id"], asset.get("status")) for asset in assets}
    changed = [asset["name"] for asset in assets if current.get((asset["id"], asset.get("status")), (None,))[0] != asset.get("inputSha256")]
    missing = [name for key, (_, name) in current.items() if key not in recorded]
    return changed + missing


def stale_in(game):
    """The stale bodies of the project whose Game folder is game: the manifest's against its kit and Vanguards.json."""
    from pathlib import Path
    game = Path(game)
    source = game / "ArtSource" / "Vanguards"
    kit = json.loads((source / "VanguardKit.json").read_text(encoding="utf-8"))
    manifest = json.loads((source / "manifest.json").read_text(encoding="utf-8"))
    vanguards = json.loads((game / "Tuning" / "Vanguards.json").read_text(encoding="utf-8"))["vanguards"]
    abilities = game / "Tuning" / "Abilities.json"
    companions = json.loads(abilities.read_text(encoding="utf-8")).get("companions", {}) if abilities.exists() else {}
    return stale_assets(kit, units(vanguards, companions), manifest["assets"])


if __name__ == "__main__":
    # The import's preflight (BuildVanguardBodies.ps1): python inputs.py <Game folder>. Names the stale bodies and fails.
    import sys
    stale = stale_in(sys.argv[1])
    if stale:
        print("Regenerate these bodies (GenerateVanguardBodies.py): their inputs changed since they were built: " + ", ".join(stale))
        sys.exit(1)
