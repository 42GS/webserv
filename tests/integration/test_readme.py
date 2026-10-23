"""README format (test plan B-06). Missing elements mean a grade of 0.
README 형식 (테스트 계획 B-06). 빠진 요소가 있으면 0점이다.

Only the form is checked here; whether the sections say enough is read
by hand before the evaluation (step 9).
여기서는 형식만 검사한다. 각 절의 내용이 충분한지는 평가 전에 직접 읽는다
(9단계).
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
