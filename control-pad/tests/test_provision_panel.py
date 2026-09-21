#!/usr/bin/env python3
"""Isolated host regression tests for control-pad profile provisioning."""

import contextlib
import importlib.util
import io
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent.parent
SOURCE = ROOT / "control-pad" / "tools" / "provision_panel.py"


def load_module():
    spec = importlib.util.spec_from_file_location("provision_panel", SOURCE)
    if spec is None or spec.loader is None:
        raise RuntimeError("Unable to load provision_panel.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class ProvisionPanelTest(unittest.TestCase):
    def setUp(self):
        self.module = load_module()
        self.temporary = tempfile.TemporaryDirectory()
        root = Path(self.temporary.name)
        setattr(self.module, "LOCAL_PROFILE", root / "platformio.local.ini")
        setattr(self.module, "ARTIFACTS", root / "artifacts")
        self.module.secrets.token_bytes = lambda size: bytes(range(size))

    def tearDown(self):
        self.temporary.cleanup()

    def run_provision(self, name):
        previous_argv = sys.argv
        output = io.StringIO()
        try:
            sys.argv = ["provision_panel.py", name]
            with contextlib.redirect_stdout(output), contextlib.redirect_stderr(output):
                return self.module.main()
        finally:
            sys.argv = previous_argv

    def test_committed_environment_names_are_rejected_without_writes(self):
        for name in self.module.committed_environment_names():
            with self.subTest(name=name):
                self.assertEqual(self.run_provision(name), 2)
                self.assertFalse(self.module.LOCAL_PROFILE.exists())
                self.assertFalse((self.module.ARTIFACTS / (name + ".pairing.txt")).exists())

    def test_valid_profile_uses_profile_environment(self):
        self.assertEqual(self.run_provision("panel2"), 0)
        self.assertIn("[env:panel2]\nextends = env:profile\n", self.module.LOCAL_PROFILE.read_text())


if __name__ == "__main__":
    unittest.main()
