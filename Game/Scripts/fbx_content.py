"""What a generated binary FBX holds, whenever it was written (ADR-040; ADR-064 §4 keeps bodies the same way).

Blender's FBX export stamps every file with the time it was written and numbers its objects afresh in each process, so
the same mesh exported twice gives different bytes. A generator that rewrote every FBX on every run would churn each one
through Git LFS for nothing. Instead it exports to a scratch file and keeps the existing FBX, its bytes and its recorded
hash, when the two hold the same content: the files read as FBX node trees, without the header's time stamp, file ID and
creation time, and with each object's number replaced by its order of first appearance. A material's own values are
not content either: every Veyra importer builds its materials from the kit and takes from an FBX only its material slots
by name, so a kit's material value (a glow's strength, a preview colour) that the generator also writes into its Blender
scene does not make a mesh new. Everything else (vertices, faces, normals, UVs, colours, slot names, sockets, transforms,
properties and connections) must match exactly.

Pure Python, so CI tests it on real exports without Blender (tests/test_fbx_content.py). A file that does not read as a
binary FBX (an LFS pointer not checked out, a truncated export) raises: it is never taken as unchanged.
"""
import hashlib
import os
import struct
import zlib
from pathlib import Path

MAGIC = b"Kaydara FBX Binary  \x00\x1a\x00"
# From this version on, node records use 64-bit offsets and counts.
WIDE_VERSION = 7500
# The top-level nodes that record when and as what the file was written, not what it holds.
VOLATILE = {b"FBXHeaderExtension", b"FileId", b"CreationTime"}
# The top-level nodes whose children's first property is an object's number.
NUMBERED = {b"Documents", b"Objects"}
# The objects whose values no importer reads: a material counts by its name (its slot), not by what it holds.
VALUELESS = {b"Material"}
SCALARS = {b"Y": "<h", b"C": "<?", b"I": "<i", b"F": "<f", b"D": "<d", b"L": "<q"}
ARRAYS = {b"f": 4, b"d": 8, b"l": 8, b"i": 4, b"b": 1}


class FbxError(ValueError):
    """The file does not read as a binary FBX."""


def _read_property(data, at):
    code = data[at:at + 1]
    at += 1
    if code in SCALARS:
        size = struct.calcsize(SCALARS[code])
        return (code, struct.unpack_from(SCALARS[code], data, at)[0]), at + size
    if code in ARRAYS:
        length, encoding, stored = struct.unpack_from("<III", data, at)
        at += 12
        raw = data[at:at + stored]
        if encoding == 1:
            raw = zlib.decompress(raw)
        elif encoding != 0:
            raise FbxError(f"unknown array encoding {encoding}")
        if len(raw) != length * ARRAYS[code]:
            raise FbxError("an array's length does not match its data")
        return (code, raw), at + stored
    if code in (b"S", b"R"):
        (length,) = struct.unpack_from("<I", data, at)
        at += 4
        return (code, bytes(data[at:at + length])), at + length
    raise FbxError(f"unknown property type {code!r}")


def _read_nodes(data, at, wide, end):
    """The nodes from at up to and including the null record that closes their list."""
    head = "<QQQB" if wide else "<IIIB"
    size = struct.calcsize(head)
    nodes = []
    while True:
        if at + size > end:
            raise FbxError("the file ends inside a node list")
        node_end, count, _, name_length = struct.unpack_from(head, data, at)
        if node_end == 0:
            return nodes, at + size
        if node_end > end or node_end <= at:
            raise FbxError("a node ends outside the file")
        at += size
        name = bytes(data[at:at + name_length])
        at += name_length
        props = []
        for _ in range(count):
            prop, at = _read_property(data, at)
            props.append(prop)
        children = []
        if at < node_end:
            children, at = _read_nodes(data, at, wide, node_end)
        if at != node_end:
            raise FbxError(f"node {name!r} does not end where it says")
        nodes.append((name, props, children))


def read(path):
    """The top-level nodes of the binary FBX at path, each (name, [(type, value)], children)."""
    data = Path(path).read_bytes()
    if not data.startswith(MAGIC) or len(data) < len(MAGIC) + 4:
        raise FbxError(f"{path} is not a binary FBX (an LFS pointer not checked out?)")
    (version,) = struct.unpack_from("<I", data, len(MAGIC))
    nodes, _ = _read_nodes(data, len(MAGIC) + 4, version >= WIDE_VERSION, len(data))
    return nodes


def content_sha256(path):
    """The hash of what the FBX at path holds, the same however often and whenever it was exported."""
    nodes = [node for node in read(path) if node[0] not in VOLATILE]
    numbers = {}
    for name, _, children in nodes:
        if name in NUMBERED:
            for _, props, _ in children:
                if props and props[0][0] == b"L":
                    numbers.setdefault(props[0][1], len(numbers))
    digest = hashlib.sha256()

    def add(node, parent=None):
        name, props, children = node
        if parent == b"Objects" and name in VALUELESS:
            children = []
        digest.update(b"N" + struct.pack("<I", len(name)) + name + struct.pack("<II", len(props), len(children)))
        for code, value in props:
            if code == b"L" and value in numbers:
                digest.update(b"#" + struct.pack("<q", numbers[value]))
            elif isinstance(value, bytes):
                digest.update(code + struct.pack("<Q", len(value)) + value)
            else:
                digest.update(code + repr(value).encode())
        for child in children:
            add(child, name)

    for node in nodes:
        add(node)
    return digest.hexdigest()


def export_keeping_unchanged(export, target):
    """Exports through export(path) to a scratch file beside target, then keeps target, its bytes untouched, when it
    holds the same content; otherwise the new export replaces it. Returns whether target was kept."""
    target = Path(target)
    scratch = target.with_name(target.stem + ".new" + target.suffix)
    export(scratch)
    try:
        if target.exists() and content_sha256(target) == content_sha256(scratch):
            return True
        os.replace(scratch, target)
        return False
    finally:
        if scratch.exists():
            scratch.unlink()
