#!/usr/bin/env python3
"""Check checkout relocation without resolving or upgrading dependencies."""

import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

SCRIPT = Path(__file__).resolve().parents[1] / "rebase_local_lock.py"
SPEC = importlib.util.spec_from_file_location("rebase_local_lock", SCRIPT)
rebase = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(rebase)


class RebaseLocalLockTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="roundwing-lock-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name) / "moved checkout"
        self.firmware = self.root / "firmware"
        self.firmware.mkdir(parents=True)
        self.lock = self.firmware / "dependencies.lock"
        lines = ["# Keep lock formatting and registry pins intact.\n", "dependencies:\n"]
        for name, relative in rebase.LOCAL_COMPONENTS.items():
            directory = self.root / relative
            directory.mkdir(parents=True)
            (directory / "CMakeLists.txt").write_text("# fixture\n")
            (directory / "idf_component.yml").write_text("version: 1.0.0\n")
            lines.extend([
                f"  {name}:\n",
                "    source:\n",
                f"      path: /missing/previous-checkout/{relative}\n",
                "      type: local\n",
                "    version: 1.0.0\n",
            ])
        lines.extend([
            "  example/registry:\n",
            "    component_hash: " + "f" * 64 + "\n",
            "    source:\n",
            "      registry_url: https://components.espressif.com/\n",
            "      type: service\n",
            "    version: '2.3.4~1' # pinned\n",
            "direct_dependencies: [example/registry]\n",
            "manifest_hash: " + "e" * 64 + "\n",
            "target: esp32s3\n",
            "version: 2.0.0\n",
        ])
        self.original = "".join(lines)
        self.lock.write_text(self.original)

    def test_rebases_moved_checkout_without_changing_pins_or_other_bytes(self):
        expected = self.original
        for relative in rebase.LOCAL_COMPONENTS.values():
            expected = expected.replace(
                f"/missing/previous-checkout/{relative}",
                json.dumps(str((self.root / relative).resolve()), ensure_ascii=False),
            )
        self.assertTrue(rebase.rebase_lock(self.firmware))
        self.assertEqual(self.lock.read_text(), expected)

    def test_already_rebased_file_is_not_rewritten(self):
        rebase.rebase_lock(self.firmware)
        before = self.lock.read_bytes(), self.lock.stat().st_mtime_ns
        with patch.object(rebase.os, "replace", side_effect=AssertionError("unexpected write")):
            self.assertFalse(rebase.rebase_lock(self.firmware))
        self.assertEqual((self.lock.read_bytes(), self.lock.stat().st_mtime_ns), before)

    def test_missing_lock_is_left_for_component_manager_to_generate(self):
        self.lock.unlink()
        self.assertFalse(rebase.rebase_lock(self.firmware))
        self.assertFalse(self.lock.exists())

    def test_empty_present_lock_is_rejected_without_writing(self):
        self.lock.write_text("")
        with self.assertRaisesRegex(ValueError, "Expected YAML mapping"):
            rebase.rebase_lock(self.firmware)
        self.assertEqual(self.lock.read_text(), "")

    def test_unknown_local_component_is_rejected_without_writing(self):
        text = self.original.replace("  example/registry:", "  unrecognized/local:")
        text = text.replace(
            "      registry_url: https://components.espressif.com/\n      type: service",
            "      path: /unknown/local\n      type: local",
        )
        self.lock.write_text(text)
        with self.assertRaisesRegex(ValueError, "Unknown local component"):
            rebase.rebase_lock(self.firmware)
        self.assertEqual(self.lock.read_text(), text)

    def test_missing_bundled_directory_is_rejected_without_writing(self):
        path = self.root / rebase.LOCAL_COMPONENTS["brookesia_core"] / "CMakeLists.txt"
        path.unlink()
        with self.assertRaisesRegex(ValueError, "Missing bundled component"):
            rebase.rebase_lock(self.firmware)
        self.assertEqual(self.lock.read_text(), self.original)

    def test_missing_expected_local_entry_is_rejected_without_writing(self):
        text = self.original.replace("      type: local", "      type: service", 1)
        self.lock.write_text(text)
        with self.assertRaisesRegex(ValueError, "Missing local component"):
            rebase.rebase_lock(self.firmware)
        self.assertEqual(self.lock.read_text(), text)

    def test_duplicate_dependency_is_rejected_without_writing(self):
        text = self.original.replace("dependencies:\n", "dependencies:\n  brookesia_core: {}\n", 1)
        self.lock.write_text(text)
        with self.assertRaisesRegex(ValueError, "Duplicate YAML key"):
            rebase.rebase_lock(self.firmware)
        self.assertEqual(self.lock.read_text(), text)


if __name__ == "__main__":
    unittest.main()
