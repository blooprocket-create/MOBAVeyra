"""Regression tests for the Unreal module-layer check."""
from __future__ import annotations

import contextlib
import importlib.util
import io
import json
import sys
import tempfile
import textwrap
import unittest
from pathlib import Path


SCRIPT = Path(__file__).resolve().parents[1] / "scripts" / "check_module_layers.py"
spec = importlib.util.spec_from_file_location("check_module_layers", SCRIPT)
assert spec and spec.loader
layers = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = layers  # dataclasses resolve annotations through sys.modules.
spec.loader.exec_module(layers)

DEFAULT_LAYERS = [
    {"name": "Foundation", "modules": ["VeyraCore"]},
    {"name": "Gameplay", "modules": ["VeyraCombat", "VeyraEconomy"]},
    {"name": "Composition", "sealed": True, "modules": ["Veyra"]},
    {"name": "Developer", "sealed": True, "modules": ["VeyraDeveloper"]},
]


def build_cs(module: str, body: str) -> str:
    return textwrap.dedent(f"""\
        using UnrealBuildTool;

        public class {module} : ModuleRules
        {{
        \tpublic {module}(ReadOnlyTargetRules Target) : base(Target)
        \t{{
        {textwrap.indent(textwrap.dedent(body), "        ")}
        \t}}
        }}
        """)


class ModuleLayerTests(unittest.TestCase):
    def setUp(self) -> None:
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.game = Path(self.tmp.name) / "Game"
        (self.game / "Source").mkdir(parents=True)
        self.write_layers(DEFAULT_LAYERS)
        # A valid baseline; each test breaks exactly one rule.
        self.write_module("VeyraCore", 'PublicDependencyModuleNames.AddRange(new string[] { "Core", "GameplayTags" });')
        self.write_module("VeyraCombat", 'PrivateDependencyModuleNames.Add("VeyraCore");')
        self.write_module("VeyraEconomy", 'PrivateDependencyModuleNames.AddRange(new[] { "Core", "VeyraCore" });')
        self.write_module("Veyra", 'PrivateDependencyModuleNames.AddRange(new string[] { "VeyraCombat", "VeyraEconomy" });')
        self.write_module("VeyraDeveloper", """\
            PrivateDependencyModuleNames.AddRange(new string[]
            {
                "Core",
                "CQTest",
                "VeyraCore",
                "VeyraCombat",
            });
            """)
        self.write_uproject(["Veyra", "VeyraCore", "VeyraCombat", "VeyraEconomy", "VeyraDeveloper"])

    def write_layers(self, layer_list: list[dict]) -> None:
        (self.game / "Source" / "ModuleLayers.json").write_text(
            json.dumps({"description": "test", "layers": layer_list}), encoding="utf-8")

    def write_module(self, module: str, body: str, folder: Path | None = None) -> Path:
        directory = (folder or self.game / "Source") / module
        directory.mkdir(parents=True, exist_ok=True)
        path = directory / f"{module}.Build.cs"
        path.write_text(build_cs(module, body), encoding="utf-8")
        return path

    def write_uproject(self, modules: list[str]) -> None:
        descriptor = {"FileVersion": 3, "Modules": [{"Name": m, "Type": "Runtime"} for m in modules]}
        (self.game / "Veyra.uproject").write_text(json.dumps(descriptor), encoding="utf-8")

    def errors(self) -> list[str]:
        return layers.check(self.game)[0]

    def assert_error(self, fragment: str) -> None:
        errors = self.errors()
        self.assertTrue(any(fragment in error for error in errors),
                        f"expected an error containing {fragment!r}, got {errors}")

    def test_valid_tree_passes(self) -> None:
        self.assertEqual(self.errors(), [])

    def test_upward_edge_rejected(self) -> None:
        self.write_module("VeyraCore", 'PublicDependencyModuleNames.Add("VeyraCombat");')
        self.assert_error("depends on VeyraCombat in the higher layer Gameplay")

    def test_sideways_edge_rejected(self) -> None:
        self.write_module("VeyraCombat", 'PrivateDependencyModuleNames.Add("VeyraEconomy");')
        self.assert_error("both are in layer Gameplay; sideways dependencies are not allowed")

    def test_self_edge_rejected(self) -> None:
        self.write_module("VeyraCore", 'PrivateDependencyModuleNames.Add("VeyraCore");')
        self.assert_error("VeyraCore depends on itself")

    def test_sealed_composition_root_rejected(self) -> None:
        self.write_module("VeyraDeveloper", 'PrivateDependencyModuleNames.Add("Veyra");')
        self.assert_error("layer Composition is sealed")

    def test_production_dependency_on_developer_rejected(self) -> None:
        self.write_module("VeyraCombat", 'PrivateDependencyModuleNames.Add("VeyraDeveloper");')
        self.assert_error("layer Developer is sealed")

    def test_cycle_reported_with_path(self) -> None:
        self.write_module("VeyraCombat", 'PrivateDependencyModuleNames.Add("VeyraEconomy");')
        self.write_module("VeyraEconomy", 'PrivateDependencyModuleNames.Add("VeyraCombat");')
        self.assert_error("Circular module dependency: VeyraCombat -> VeyraEconomy -> VeyraCombat")

    def test_undeclared_veyra_dependency_rejected(self) -> None:
        self.write_module("VeyraCombat", 'PrivateDependencyModuleNames.Add("VeyraFlux");')
        self.assert_error("depends on VeyraFlux, which is not declared")

    def test_undeclared_veyra_dependency_rejected_regardless_of_case(self) -> None:
        self.write_module("VeyraCombat", 'PrivateDependencyModuleNames.Add("veyraFlux");')
        self.assert_error("depends on veyraFlux, which is not declared")

    def test_engine_modules_ignored(self) -> None:
        self.write_module("VeyraCore", 'PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "UnrealEd" });')
        self.assertEqual(self.errors(), [])

    def test_include_path_dependencies_count(self) -> None:
        for field in ("PublicIncludePathModuleNames", "PrivateIncludePathModuleNames"):
            with self.subTest(field=field):
                self.write_module("VeyraCore", f'{field}.Add("VeyraCombat");')
                self.assert_error(f"({field}): VeyraCore (layer Foundation) depends on VeyraCombat")

    def test_dynamically_loaded_dependencies_count(self) -> None:
        self.write_module("VeyraCore", 'DynamicallyLoadedModuleNames.Add("VeyraCombat");')
        self.assert_error("(DynamicallyLoadedModuleNames): VeyraCore (layer Foundation) depends on VeyraCombat")

    def test_comments_ignored(self) -> None:
        self.write_module("VeyraCore", """\
            // PrivateDependencyModuleNames.Add("VeyraDeveloper");
            /* PrivateDependencyModuleNames.Add("VeyraCombat");
               DynamicallyLoadedModuleNames.Add(SomeName); */
            PublicDependencyModuleNames.Add("Core"); // "VeyraCombat"
            """)
        self.assertEqual(self.errors(), [])

    def test_comment_markers_inside_strings_are_not_comments(self) -> None:
        self.write_module("VeyraCore", """\
            string Note = "see https://example.invalid/* not a comment";
            PublicDependencyModuleNames.Add("VeyraCombat");
            string Other = @"also not a comment */";
            """)
        self.assert_error("depends on VeyraCombat in the higher layer Gameplay")

    def test_non_literal_usage_rejected(self) -> None:
        bodies = (
            'PrivateDependencyModuleNames.Add(ModuleName);',
            'PrivateDependencyModuleNames.AddRange(Names);',
            'PrivateDependencyModuleNames.AddRange(new string[] { "Core", SomeName });',
            'PrivateDependencyModuleNames.AddRange(["Core"]);',
            'PrivateDependencyModuleNames = new List<string> { "Core" };',
            'PrivateDependencyModuleNames.Insert(0, "Core");',
            'PrivateDependencyModuleNames.Add($"Veyra{Suffix}");',
            'var Names = PrivateDependencyModuleNames;',
        )
        for body in bodies:
            with self.subTest(body=body):
                self.write_module("VeyraCombat", body)
                self.assert_error("unsupported use of PrivateDependencyModuleNames")

    def test_circularly_referenced_modules_rejected(self) -> None:
        self.write_module("VeyraCombat", 'CircularlyReferencedDependentModules.Add("VeyraEconomy");')
        self.assert_error("CircularlyReferencedDependentModules is not allowed")

    def test_undeclared_build_module_rejected(self) -> None:
        self.write_module("VeyraFlux", 'PrivateDependencyModuleNames.Add("VeyraCore");')
        self.write_uproject(["Veyra", "VeyraCore", "VeyraCombat", "VeyraEconomy", "VeyraDeveloper", "VeyraFlux"])
        self.assert_error("module VeyraFlux is not declared in Game/Source/ModuleLayers.json")

    def test_declared_module_without_build_file_rejected(self) -> None:
        self.write_layers(DEFAULT_LAYERS + [{"name": "Extra", "modules": ["VeyraVision"]}])
        self.assert_error("declares VeyraVision, but no VeyraVision.Build.cs exists")

    def test_uproject_module_list_must_match(self) -> None:
        self.write_uproject(["Veyra", "VeyraCore", "VeyraCombat", "VeyraDeveloper", "VeyraItems"])
        errors = self.errors()
        self.assertTrue(any("does not list module VeyraEconomy" in e for e in errors), errors)
        self.assertTrue(any("lists module VeyraItems, which has no Build.cs" in e for e in errors), errors)

    def test_plugin_modules_are_checked(self) -> None:
        plugin_source = self.game / "Plugins" / "VeyraTools" / "Source"
        self.write_module("VeyraToolsRuntime", 'PrivateDependencyModuleNames.Add("VeyraCore");', plugin_source)
        self.assert_error("module VeyraToolsRuntime is not declared")

    def test_duplicate_build_files_rejected(self) -> None:
        self.write_module("VeyraCore", 'PublicDependencyModuleNames.Add("Core");', self.game / "Source" / "Copy")
        self.assert_error("Module VeyraCore has two Build.cs files")

    def test_layer_map_problems_rejected(self) -> None:
        cases = (
            ([{"name": "A", "modules": ["VeyraCore"]}, {"name": "B", "modules": ["VeyraCore"]}],
             "VeyraCore is declared in both A and B"),
            ([{"name": "A", "modules": ["VeyraCore"], "seald": True}], 'unknown key "seald"'),
            ([{"name": "A", "modules": ["VeyraCore"], "sealed": "yes"}], 'non-boolean "sealed"'),
            ([{"name": "A", "modules": []}], 'needs a non-empty "modules" list'),
            ([], 'non-empty "layers" list'),
        )
        for layer_list, fragment in cases:
            with self.subTest(fragment=fragment):
                self.write_layers(layer_list)
                self.assert_error(fragment)

    def test_main_reports_errors_and_exit_code(self) -> None:
        self.write_module("VeyraCore", 'PublicDependencyModuleNames.Add("VeyraCombat");')
        output = io.StringIO()
        with contextlib.redirect_stdout(output), contextlib.redirect_stderr(output):
            status = layers.main(["--game-dir", str(self.game)])
        self.assertEqual(status, 1)
        self.assertIn("ERROR:", output.getvalue())


class RealRepositoryTests(unittest.TestCase):
    def test_repository_layers_pass(self) -> None:
        errors, summary = layers.check(layers.GAME)
        self.assertEqual(errors, [], "\n".join(errors))
        self.assertIn("modules in", summary)


if __name__ == "__main__":
    unittest.main()
