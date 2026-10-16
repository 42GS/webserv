"""README format (test plan B-06). Missing elements mean a grade of 0.

Only the form is checked here; whether the sections say enough is read
by hand before the evaluation (step 9).
"""

import unittest

import harness
from constants import (
    README,
    README_FIRST_LINE,
    README_SECTIONS,
)


class ReadmeTest(unittest.TestCase):
    def setUp(self):
        with open(README) as readme:
            self.lines = readme.read().splitlines()

    def test_b06_first_line_is_the_italic_credit(self):
        self.assertRegex(self.lines[0], README_FIRST_LINE)

    def test_b06_required_sections_exist(self):
        for section in README_SECTIONS:
            self.assertIn(section, self.lines)


if __name__ == "__main__":
    unittest.main()
