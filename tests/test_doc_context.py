"""Regression tests for agent-facing section-map maintenance."""
from __future__ import annotations

import contextlib
import importlib.util
import io
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch


SCRIPT = Path(__file__).resolve().parents[1] / "scripts" / "check_doc_context.py"
spec = importlib.util.spec_from_file_location("check_doc_context", SCRIPT)
assert spec and spec.loader
index = importlib.util.module_from_spec(spec)
spec.loader.exec_module(index)


class ContextIndexTests(unittest.TestCase):
    def setUp(self) -> None:
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        root = Path(self.tmp.name)
        design = root / "Docs" / "Design"
        routes = root / "Docs" / "CONTEXT_MAP.md"
        sections = root / "Docs" / "Index" / "sections"
        design.mkdir(parents=True)
        sections.mkdir(parents=True)
        routes.parent.mkdir(parents=True, exist_ok=True)
        links = []
        for filename in index.MAPPED_BIBLES:
            (design / filename).write_text("# Sample bible\n## Sample section\n", encoding="utf-8")
            links.append(f"[Owner](Design/{filename})")
        links.extend((
            "[Entities](ADR/ADR-003-owned-field-entities.md)",
            "[Client](ADR/ADR-003-unified-unreal-client-states.md)",
        ))
        routes.write_text("\n".join(links), encoding="utf-8")
        self.sections = sections
        for key, value in {
            "ROOT": root,
            "DESIGN": design,
            "ROUTES": routes,
            "SECTIONS": sections,
        }.items():
            patcher = patch.object(index, key, value)
            patcher.start()
            self.addCleanup(patcher.stop)

    def invoke(self, mode: str) -> tuple[int, str]:
        output = io.StringIO()
        with patch.object(sys, "argv", [str(SCRIPT), mode]), \
             contextlib.redirect_stdout(output), contextlib.redirect_stderr(output):
            status = index.main()
        return status, output.getvalue()

    def test_obsolete_generated_locator_removed_by_write(self) -> None:
        self.assertEqual(self.invoke("--write")[0], 0)
        self.assertEqual(self.invoke("--check")[0], 0)
        obsolete = self.sections / "Veyra_Combat_Bible_v0.4.md"
        obsolete.write_text(index.section_map(index.MAPPED_BIBLES[1]), encoding="utf-8")

        result, diagnostics = self.invoke("--check")
        self.assertEqual(result, 1)
        self.assertIn("Stale generated section map:", diagnostics)

        result, diagnostics = self.invoke("--write")
        self.assertEqual(result, 0, diagnostics)
        self.assertFalse(obsolete.exists())
        self.assertIn("Removed obsolete section map:", diagnostics)
        self.assertEqual(self.invoke("--check")[0], 0)

    def test_write_does_not_delete_unexpected_hand_authored_file(self) -> None:
        unexpected = self.sections / "Veyra_Combat_Bible_v0.4.md"
        unexpected.write_text("# Hand-authored note\n", encoding="utf-8")
        status, diagnostics = self.invoke("--write")
        self.assertEqual(status, 1)
        self.assertTrue(unexpected.exists())
        self.assertIn("Unexpected non-generated section file", diagnostics)


if __name__ == "__main__":
    unittest.main()
