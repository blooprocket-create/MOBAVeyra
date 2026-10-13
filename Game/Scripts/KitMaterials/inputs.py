"""What a generated kit material is built from (ADR-040, ADR-064 §4): the inputs whose change means it must be rebuilt.

Pure Python, so CI checks it without Unreal (tests/test_kit_materials_inputs.py). An importer records the hash on the
material it builds (the asset's metadata, under INPUTS_TAG) and keeps the material as it is while the hash it would
record now is the one there: a rebuild would make the same graph with new expression IDs, a changed file for no change.
"""
import hashlib
import json
from pathlib import Path

# The asset metadata tag an importer records a material's input hash under.
INPUTS_TAG = "VeyraInputsSha256"


def _text(path):
    """A source file's text, its line endings made alike, so a Windows checkout and CI agree."""
    return Path(path).read_bytes().replace(b"\r\n", b"\n")


def material_inputs_hash(values, sources, bindings=None):
    """The hash of everything a material is built from: its values (the kit's section, as JSON), the text of each source
    file that builds its graph (sources, by name: the file's name is part of it, so moving code between files counts),
    and bindings, what of other assets its graph links to by identity (a parameter collection's path and parameter IDs:
    a collection made anew gives its parameters new IDs, which the material must link to again)."""
    digest = hashlib.sha256()
    digest.update(json.dumps({"values": values, "bindings": bindings}, sort_keys=True).encode("utf-8") + b"\0")
    for path in sorted((Path(source) for source in sources), key=lambda path: path.name):
        digest.update(path.name.encode("utf-8") + b"\0" + _text(path) + b"\0")
    return digest.hexdigest()


def kept(recorded, current):
    """Whether a material whose recorded input hash is recorded (empty when it has none) is current, given current."""
    return bool(recorded) and recorded == current
