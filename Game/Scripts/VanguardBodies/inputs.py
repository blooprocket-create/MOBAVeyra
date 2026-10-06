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
    """Every manifest asset whose recorded inputs are not what the kit and Vanguards.json give it now, by name."""
    current = {}
    for spec in kit["vanguards"]:
        for body_spec, status, _ in bodies_of(spec):
            current[(spec["id"], status)] = input_hash(kit, vanguards, body_spec)
    return [asset["name"] for asset in assets if current.get((asset["id"], asset.get("status"))) != asset.get("inputSha256")]
