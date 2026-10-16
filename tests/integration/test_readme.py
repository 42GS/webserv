"""README format (test plan B-06). Missing elements mean a grade of 0.

Only the form is checked here; whether the sections say enough is read
by hand before the evaluation (step 9).
"""

import os
import re
import unittest

import harness

README = os.path.join(harness.REPO_ROOT, "README.md")
FIRST_LINE = re.compile(
    r"^\*This project has been created as part of the 42 curriculum by "
    r"[a-z0-9-]+(, [a-z0-9-]+)*\.\*$")
REQUIRED_SECTIONS = ["## Description", "## Instructions", "## Resources"]


class ReadmeTest(unittest.TestCase):
    def setUp(self):
        with open(README) as readme:
            self.lines = readme.read().splitlines()

    def test_b06_first_line_is_the_italic_credit(self):
        self.assertRegex(self.lines[0], FIRST_LINE)

    def test_b06_required_sections_exist(self):
        for section in REQUIRED_SECTIONS:
            self.assertIn(section, self.lines)


if __name__ == "__main__":
    unittest.main()
