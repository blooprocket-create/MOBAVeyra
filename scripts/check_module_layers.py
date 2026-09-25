#!/usr/bin/env python3
"""Check Veyra's Unreal module dependencies against the declared layer map.

Run from any directory:
    python3 scripts/check_module_layers.py

Game/Source/ModuleLayers.json lists the layers bottom to top (ADR-006 §3). Every *.Build.cs
under Game/Source and Game/Plugins is read with comments stripped. A dependency is any name in
Public/PrivateDependencyModuleNames, Public/PrivateIncludePathModuleNames or
DynamicallyLoadedModuleNames. A module may depend only on modules in lower layers: never on
itself, on its own layer, on a higher layer, or on a sealed layer. Engine modules are ignored.

The check fails closed. Dependencies must be literal names added with .Add("Name") or
.AddRange(new[] { "Name", ... }); anything the check cannot read is an error, not a pass.
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
GAME = ROOT / "Game"
LAYER_MAP = Path("Source") / "ModuleLayers.json"
VEYRA_PREFIX = "veyra"

DEPENDENCY_FIELDS = (
    "PublicDependencyModuleNames",
    "PrivateDependencyModuleNames",
    "PublicIncludePathModuleNames",
    "PrivateIncludePathModuleNames",
    "DynamicallyLoadedModuleNames",
)
FIELD_RE = re.compile(r"\b(" + "|".join(DEPENDENCY_FIELDS) + r")\b")
NAME = r'"([A-Za-z_][A-Za-z0-9_]*)"'
ADD_RE = re.compile(r"\s*\.\s*Add\s*\(\s*" + NAME + r"\s*\)")
ADD_RANGE_RE = re.compile(
    r"\s*\.\s*AddRange\s*\(\s*new\s*(?:string\s*)?\[\s*\]\s*\{(?P<items>[^{}]*)\}\s*\)"
)
ITEMS_RE = re.compile(r"\s*" + NAME + r"\s*(?:,\s*" + NAME + r"\s*)*,?\s*")
ITEM_RE = re.compile(NAME)
FORBIDDEN_RE = re.compile(r"\bCircularlyReferencedDependentModules\b")
STRING_START_RE = re.compile(r'[$@]{0,2}"')
CHAR_LITERAL_RE = re.compile(r"'(?:\\.|[^'\\])*'")
LAYER_KEYS = {"name", "description", "modules", "sealed"}
MAP_KEYS = {"description", "layers"}


class CheckError(Exception):
    """A problem that stops the check from reading its inputs."""


@dataclass
class Dependency:
    target: str
    source_field: str
    line: int


@dataclass
class BuildFile:
    module: str
    shown: str  # The path as reported in messages.
    dependencies: list[Dependency] = field(default_factory=list)


@dataclass
class Layer:
    index: int
    name: str
    sealed: bool
    modules: list[str]


def strip_comments(text: str) -> str:
    """Blank out C# comments, keeping string literals and line numbers intact."""
    out: list[str] = []
    i, length = 0, len(text)
    while i < length:
        char = text[i]
        pair = text[i:i + 2]
        if pair == "//":
            end = text.find("\n", i)
            end = length if end == -1 else end
            out.append(" " * (end - i))
            i = end
        elif pair == "/*":
            end = text.find("*/", i + 2)
            if end == -1:
                raise CheckError("unterminated block comment")
            out.append(re.sub(r"[^\n]", " ", text[i:end + 2]))
            i = end + 2
        elif text.startswith('"""', i):
            raise CheckError("raw string literals are not supported")
        elif char in "$@" or char == '"':
            prefix = STRING_START_RE.match(text, i)
            if not prefix:
                out.append(char)
                i += 1
                continue
            verbatim = "@" in prefix.group(0)
            j = i + len(prefix.group(0))
            while j < length:
                if verbatim and text.startswith('""', j):
                    j += 2
                elif not verbatim and text[j] == "\\":
                    j += 2
                elif text[j] == '"' or (not verbatim and text[j] == "\n"):
                    j += 1
                    break
                else:
                    j += 1
            out.append(text[i:j])
            i = j
        elif char == "'":
            end = CHAR_LITERAL_RE.match(text, i)
            span = len(end.group(0)) if end else 1
            out.append(text[i:i + span])
            i += span
        else:
            out.append(char)
            i += 1
    return "".join(out)


def parse_build_file(path: Path, shown: str) -> tuple[BuildFile, list[str]]:
    module = path.name[: -len(".Build.cs")]
    build = BuildFile(module, shown)
    errors: list[str] = []
    try:
        code = strip_comments(path.read_text(encoding="utf-8-sig"))
    except CheckError as error:
        return build, [f"{shown}: {error}"]

    def line_of(offset: int) -> int:
        return code.count("\n", 0, offset) + 1

    for match in FORBIDDEN_RE.finditer(code):
        errors.append(
            f"{shown}:{line_of(match.start())}: CircularlyReferencedDependentModules is not "
            "allowed; break the cycle instead."
        )
    for match in FIELD_RE.finditer(code):
        name, line = match.group(1), line_of(match.start())
        added = ADD_RE.match(code, match.end())
        if added:
            build.dependencies.append(Dependency(added.group(1), name, line))
            continue
        added_range = ADD_RANGE_RE.match(code, match.end())
        if added_range and ITEMS_RE.fullmatch(added_range.group("items")):
            for item in ITEM_RE.finditer(added_range.group("items")):
                build.dependencies.append(Dependency(item.group(1), name, line))
            continue
        errors.append(
            f"{shown}:{line}: unsupported use of {name}. Use only .Add(\"Name\") or "
            f".AddRange(new[] {{ \"Name\", ... }}) with literal module names."
        )
    return build, errors


def load_layers(path: Path, shown: str) -> tuple[list[Layer], list[str]]:
    if not path.is_file():
        return [], [f"Missing layer map: {shown}"]
    try:
        data = json.loads(path.read_text(encoding="utf-8-sig"))
    except json.JSONDecodeError as error:
        return [], [f"{shown}: invalid JSON: {error}"]

    errors: list[str] = []
    if not isinstance(data, dict) or not isinstance(data.get("layers"), list) or not data["layers"]:
        return [], [f"{shown}: expected an object with a non-empty \"layers\" list."]
    for key in sorted(set(data) - MAP_KEYS):
        errors.append(f"{shown}: unknown key \"{key}\".")

    layers: list[Layer] = []
    owner: dict[str, str] = {}
    for index, entry in enumerate(data["layers"]):
        if not isinstance(entry, dict):
            errors.append(f"{shown}: layer {index} is not an object.")
            continue
        for key in sorted(set(entry) - LAYER_KEYS):
            errors.append(f"{shown}: layer {index} has unknown key \"{key}\".")
        name, modules, sealed = entry.get("name"), entry.get("modules"), entry.get("sealed", False)
        if not isinstance(name, str) or not name:
            errors.append(f"{shown}: layer {index} needs a \"name\".")
            continue
        if not isinstance(sealed, bool):
            errors.append(f"{shown}: layer {name} has a non-boolean \"sealed\".")
        if (not isinstance(modules, list) or not modules
                or not all(isinstance(module, str) and module for module in modules)):
            errors.append(f"{shown}: layer {name} needs a non-empty \"modules\" list of names.")
            continue
        for module in modules:
            if module in owner:
                errors.append(f"{shown}: {module} is declared in both {owner[module]} and {name}.")
            owner[module] = name
        layers.append(Layer(index, name, sealed is True, list(modules)))
    return layers, errors


def find_cycle(graph: dict[str, list[str]]) -> list[str] | None:
    """Return one dependency cycle as a path that starts and ends at the same module."""
    state: dict[str, str] = {}
    stack: list[str] = []

    def visit(node: str) -> list[str] | None:
        state[node] = "active"
        stack.append(node)
        for successor in graph.get(node, []):
            if state.get(successor) == "active":
                return stack[stack.index(successor):] + [successor]
            if successor not in state:
                cycle = visit(successor)
                if cycle:
                    return cycle
        stack.pop()
        state[node] = "done"
        return None

    for node in sorted(graph):
        if node not in state:
            cycle = visit(node)
            if cycle:
                return cycle
    return None


def check(game_dir: Path) -> tuple[list[str], str]:
    """Return (errors, summary) for the Unreal project in game_dir."""

    def shown(path: Path) -> str:
        try:
            return path.relative_to(game_dir.parent).as_posix()
        except ValueError:
            return path.as_posix()

    layer_map = shown(game_dir / LAYER_MAP)
    layers, errors = load_layers(game_dir / LAYER_MAP, layer_map)
    layer_of = {module: layer for layer in layers for module in layer.modules}

    builds: dict[str, BuildFile] = {}
    source_modules: set[str] = set()
    for folder in ("Source", "Plugins"):
        root = game_dir / folder
        if not root.is_dir():
            continue
        for path in sorted(root.rglob("*.Build.cs")):
            build, parse_errors = parse_build_file(path, shown(path))
            errors.extend(parse_errors)
            if build.module in builds:
                errors.append(f"Module {build.module} has two Build.cs files: "
                              f"{builds[build.module].shown} and {shown(path)}.")
                continue
            builds[build.module] = build
            if folder == "Source":
                source_modules.add(build.module)

    for module in sorted(set(builds) - set(layer_of)):
        errors.append(f"{builds[module].shown}: module {module} is not declared in {layer_map}.")
    for module in sorted(set(layer_of) - set(builds)):
        errors.append(f"{layer_map} declares {module}, but no {module}.Build.cs exists.")

    projects = sorted(game_dir.glob("*.uproject"))
    if len(projects) != 1:
        errors.append(f"Expected exactly one .uproject in {shown(game_dir)}, found {len(projects)}.")
    else:
        try:
            descriptor = json.loads(projects[0].read_text(encoding="utf-8-sig"))
            listed = {entry["Name"] for entry in descriptor.get("Modules", [])}
        except (json.JSONDecodeError, KeyError, TypeError) as error:
            errors.append(f"{shown(projects[0])}: cannot read its module list: {error}")
        else:
            for module in sorted(source_modules - listed):
                errors.append(f"{projects[0].name} does not list module {module} from Source/.")
            for module in sorted(listed - source_modules):
                errors.append(f"{projects[0].name} lists module {module}, which has no Build.cs in Source/.")

    graph: dict[str, list[str]] = {module: [] for module in builds}
    edge_count = 0
    for build in builds.values():
        source_layer = layer_of.get(build.module)
        for dependency in build.dependencies:
            where = f"{build.shown}:{dependency.line} ({dependency.source_field})"
            target = dependency.target
            if target not in layer_of:
                if target.lower().startswith(VEYRA_PREFIX):
                    errors.append(f"{where}: {build.module} depends on {target}, which is not "
                                  f"declared in {layer_map}.")
                continue  # An engine or third-party module.
            edge_count += 1
            if target in graph and target not in graph[build.module]:
                graph[build.module].append(target)
            target_layer = layer_of[target]
            if source_layer is None:
                continue  # Already reported as undeclared.
            if target == build.module:
                errors.append(f"{where}: {build.module} depends on itself.")
            elif target_layer.sealed:
                errors.append(f"{where}: {build.module} depends on {target}, but layer "
                              f"{target_layer.name} is sealed: nothing may depend on it.")
            elif target_layer.index == source_layer.index:
                errors.append(f"{where}: {build.module} depends on {target}, but both are in "
                              f"layer {source_layer.name}; sideways dependencies are not allowed.")
            elif target_layer.index > source_layer.index:
                errors.append(f"{where}: {build.module} (layer {source_layer.name}) depends on "
                              f"{target} in the higher layer {target_layer.name}.")

    cycle = find_cycle(graph)
    if cycle:
        errors.append("Circular module dependency: " + " -> ".join(cycle))

    summary = (f"{len(builds)} modules in {len(layers)} layers; "
               f"{edge_count} Veyra dependency edges checked.")
    return errors, summary


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--game-dir", type=Path, default=GAME,
                        help="Unreal project folder (default: the repository's Game/).")
    args = parser.parse_args(argv)

    errors, summary = check(args.game_dir.resolve())
    if errors:
        for error in errors:
            print("ERROR:", error, file=sys.stderr)
        return 1
    print(f"OK: {summary}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
