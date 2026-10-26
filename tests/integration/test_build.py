"""Makefile rules (test plan B-01..B-03), run on a copy of the sources.
Makefile 규칙 (테스트 계획 B-01..B-03). 소스 복사본에서 실행한다.
"""

import os
import shutil
import subprocess
import tempfile
import unittest

import harness
from constants import (
    BUILD_TIMEOUT_SECONDS,
    COMPILER,
    REPO_ROOT,
    RUN_SLOW,
    SOURCE_ENTRIES,
)


@unittest.skipUnless(RUN_SLOW, "set WEBSERV_SLOW_TESTS=1 (full builds)")
class BuildTest(unittest.TestCase):
    def setUp(self):
        self.tree = tempfile.mkdtemp(prefix="webserv-build-")
        self.addCleanup(shutil.rmtree, self.tree)
        for entry in SOURCE_ENTRIES:
            source = os.path.join(REPO_ROOT, entry)
            target = os.path.join(self.tree, entry)
            if os.path.isdir(source):
                shutil.copytree(source, target)
            else:
                shutil.copy(source, target)

    def make(self, *targets):
        """Runs make; fails the test if it fails. Returns its output.
        make를 실행한다. 실패하면 테스트도 실패한다. 출력을 돌려준다.
        """
        finished = subprocess.run(["make"] + list(targets), cwd=self.tree,
                                  capture_output=True, text=True,
                                  timeout=BUILD_TIMEOUT_SECONDS)
        self.assertEqual(0, finished.returncode, finished.stderr)
        return finished.stdout

    def exists(self, relative_path):
        """True if the path exists in the copy.
        복사본에 그 경로가 있으면 True.
        """
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

    # The subject asks for c++ with -Wall -Wextra -Werror, and for the code
    # to still build when -std=c++98 is added; so it must also build with
    # the compiler's default standard, where C++98-only features such as
    # std::auto_ptr are deprecated.
    # subject는 -Wall -Wextra -Werror로 c++ 컴파일을 요구하고, -std=c++98을
    # 더해도 빌드돼야 한다고 한다. 그래서 std::auto_ptr 같은 C++98 기능이
    # deprecated인 컴파일러 기본 표준으로도 빌드돼야 한다.
    def test_b03_builds_without_std_flag(self):
        output = self.make("CXXFLAGS=-Wall -Wextra -Werror", "all")
        self.assertNotIn("-std=", output)
        self.assertTrue(self.exists("webserv"))

    def test_b02_second_make_does_not_relink(self):
        self.make()
        self.assertNotIn(COMPILER, self.make())


if __name__ == "__main__":
    unittest.main()
