"""Makefile rules (test plan B-01..B-03), run on a copy of the sources."""

import os
import shutil
import subprocess
import tempfile
import unittest

import harness

SOURCE_ENTRIES = ["Makefile", "include", "src"]
COMPILER = "c++ "
BUILD_TIMEOUT_SECONDS = 600
RUN_SLOW = os.environ.get("WEBSERV_SLOW_TESTS") == "1"


@unittest.skipUnless(RUN_SLOW, "set WEBSERV_SLOW_TESTS=1 (full builds)")
class BuildTest(unittest.TestCase):
    def setUp(self):
        self.tree = tempfile.mkdtemp(prefix="webserv-build-")
        self.addCleanup(shutil.rmtree, self.tree)
        for entry in SOURCE_ENTRIES:
            source = os.path.join(harness.REPO_ROOT, entry)
            target = os.path.join(self.tree, entry)
            if os.path.isdir(source):
                shutil.copytree(source, target)
            else:
                shutil.copy(source, target)

    def make(self, *targets):
        """Runs make; fails the test if it fails. Returns its output."""
        finished = subprocess.run(["make"] + list(targets), cwd=self.tree,
                                  capture_output=True, text=True,
                                  timeout=BUILD_TIMEOUT_SECONDS)
        self.assertEqual(0, finished.returncode, finished.stderr)
        return finished.stdout

    def exists(self, relative_path):
        return os.path.exists(os.path.join(self.tree, relative_path))

    def test_b01_b03_rules_build_and_clean(self):
        self.assertIn("-Wall -Wextra -Werror -std=c++98", self.make("all"))
        self.assertTrue(self.exists("webserv"))
        self.make("clean")
        self.assertFalse(self.exists("obj"))
        self.assertTrue(self.exists("webserv"))
        self.make("fclean")
        self.assertFalse(self.exists("webserv"))
        self.make("re")
        self.assertTrue(self.exists("webserv"))
        self.make("webserv")

    def test_b02_second_make_does_not_relink(self):
        self.make()
        self.assertNotIn(COMPILER, self.make())


if __name__ == "__main__":
    unittest.main()
