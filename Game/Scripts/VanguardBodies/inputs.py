"""What a generated body is made from (ADR-064 §4): the inputs whose change makes it stale.

Pure Python, shared by the generator (Blender), the importer (Unreal) and CI, so all derive the same answer. A body
is stale when its own kit entry, its archetype's settings, the kit's frame rate, the generator version or its
Vanguard's capsule and attack range class change, whichever Vanguard was rebuilt last; and when the generator's code
or the Blender that built it differ from the manifest's. A full rebuild redoes every body and rewrites only those whose
content changed (GenerateVanguardBodies.py), so it costs little when little changed.
"""
import hashlib
import json
from pathlib import Path

GENERATOR_VERSION = 1


def bodies_of(spec):
    """A Vanguard's bodies, as (spec, status, name suffix): its own, then each it wears while it holds a status, built
    from its own entry with the status body's entries over it (a rider's ride, ADR-064 §1)."""
    yield spec, None, ""
    for status_body in spec.get("statusBodies", []):
        yield dict(spec, **status_body["body"]), status_body["status"], "_" + status_body["name"]


def body_name(vanguard_id, suffix):
    """A body's asset name: SK_, its Vanguard's ID in title case, then its status body's suffix (SK_Raska_Hound)."""
    return "SK_" + vanguard_id.title().replace("_", "") + suffix


def generator_hash(scripts):
    """The hash of the code that builds a body: GenerateVanguardBodies.py and every module of the VanguardBodies package
    in scripts (the Game/Scripts folder), each by name and its text with line endings made alike, so a Windows checkout
    and CI agree."""
    scripts = Path(scripts)
    digest = hashlib.sha256()
    for path in [scripts / "GenerateVanguardBodies.py"] + sorted((scripts / "VanguardBodies").glob("*.py")):
        digest.update(path.name.encode("utf-8") + b"\0" + path.read_bytes().replace(b"\r\n", b"\n") + b"\0")
    return digest.hexdigest()


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


def stale_assets(kit, vanguards, assets, generator=None, blender=None):
    """Every body the manifest does not hold as the kit and Vanguards.json give it now, by name: one whose recorded
    inputs differ, that the kit no longer has, or that the kit now names otherwise (a status body renamed: its name is
    no input of its body); one the kit has that the manifest lacks (a partial build that kept a manifest from before
    the kit gave a Vanguard a new body); and, when given, one built by other generator code than generator or another
    Blender than blender."""
    current = {}
    for spec in kit["vanguards"]:
        for body_spec, status, suffix in bodies_of(spec):
            current[(spec["id"], status)] = (input_hash(kit, vanguards, body_spec), body_name(spec["id"], suffix))
    recorded = {(asset["id"], asset.get("status")) for asset in assets}
    changed = [asset["name"] for asset in assets if current.get((asset["id"], asset.get("status")), (None, None)) != (asset.get("inputSha256"), asset["name"])
               or (generator is not None and asset.get("generatorSha256") != generator)
               or (blender is not None and asset.get("blender") != blender)]
    missing = [name for key, (_, name) in current.items() if key not in recorded]
    return changed + missing


def removed_assets(previous, current, only=None):
    """The bodies a build drops, by their previous manifest assets: those of the Vanguards it made (every one, or only
    those named) that its manifest no longer holds by name, a body the kit removed or renamed. Their FBX and imported
    assets go with them, so nothing unreferenced stays tracked or cooked."""
    names = {asset["name"] for asset in current}
    return [asset for asset in previous if (only is None or asset["id"] in only) and asset["name"] not in names]


def pending_changed(pending, fresh, assets):
    """The Vanguards (by ID) whose bodies are still to import after a build: those an earlier build changed that no
    import has taken yet (a generator run on its own, a failed import), with those this build changed (fresh), less
    any the manifest (its assets) no longer has. A body kept as it was is only current if it was imported."""
    ids = {asset["id"] for asset in assets}
    return sorted((set(pending) | set(fresh)) & ids)


def pending_removed(pending, dropped, assets):
    """The bodies (by name) whose imported assets are still to delete after a build: those dropped earlier and not yet
    deleted, with those this build dropped, less any the manifest holds again (its import replaces their folder)."""
    names = {asset["name"] for asset in assets}
    return sorted((set(pending) | set(dropped)) - names)


def pinned_blender(kit, version):
    """Whether version (as Blender gives it, "5.2.0") is the release the kit pins (its "blender", major.minor)."""
    return version is not None and version.split(".")[:2] == kit["blender"].split(".")[:2]


def stale_in(game):
    """The stale bodies of the project whose Game folder is game: the manifest's against its kit, Vanguards.json, the
    generator's code beside them and the Blender the manifest was last built by, which must be the one the kit pins:
    built by any other, every body is stale."""
    game = Path(game)
    source = game / "ArtSource" / "Vanguards"
    kit = json.loads((source / "VanguardKit.json").read_text(encoding="utf-8"))
    manifest = json.loads((source / "manifest.json").read_text(encoding="utf-8"))
    vanguards = json.loads((game / "Tuning" / "Vanguards.json").read_text(encoding="utf-8"))["vanguards"]
    stale = stale_assets(kit, vanguards, manifest["assets"], generator_hash(game / "Scripts"), manifest.get("blender"))
    if not pinned_blender(kit, manifest.get("blender")):
        stale = sorted(set(stale) | {asset["name"] for asset in manifest["assets"]})
    return stale


# How a Git LFS pointer begins: the file is not checked out (CI does not fetch large files), only its object's id.
LFS_POINTER = b"version https://git-lfs.github.com/spec/v1"


def committed_mismatches(game):
    """Every manifest asset whose FBX is missing or is not the file the manifest recorded, by name. A file Git LFS has
    not checked out is its pointer, which names its object by the same SHA-256, so CI need not fetch the FBX."""
    game = Path(game)
    source = game / "ArtSource" / "Vanguards"
    manifest = json.loads((source / "manifest.json").read_text(encoding="utf-8"))
    mismatches = []
    for asset in manifest["assets"]:
        path = source / asset["file"]
        if not path.is_file():
            mismatches.append(asset["name"])
            continue
        data = path.read_bytes()
        if data.startswith(LFS_POINTER):
            oid = next((line.split(":", 1)[1] for line in data.decode("utf-8").splitlines() if line.startswith("oid sha256:")), None)
        else:
            oid = hashlib.sha256(data).hexdigest()
        if oid != asset.get("sha256"):
            mismatches.append(asset["name"])
    return mismatches


if __name__ == "__main__":
    # The import's preflight (BuildVanguardBodies.ps1): python inputs.py <Game folder>. Names the stale bodies and fails.
    import sys
    stale = stale_in(sys.argv[1])
    if stale:
        print("Regenerate these bodies (GenerateVanguardBodies.py; a full build rewrites only those that changed): their inputs,"
              " generator code or Blender changed since they were built, or their Blender is not the kit's: " + ", ".join(stale))
    mismatched = committed_mismatches(sys.argv[1])
    if mismatched:
        print("These bodies' FBX are missing or are not what the manifest recorded: " + ", ".join(mismatched))
    if stale or mismatched:
        sys.exit(1)
