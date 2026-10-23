"""Routing, static files and responses (test plan R-xx, step 4).
라우팅, 정적 파일, 응답 (테스트 계획 R-xx, 4단계).
"""

import os
import unittest

import harness
from constants import (
    BIG_FILE_SIZE,
    CUSTOM_404,
    DEFAULT_PAGE,
    HOST,
    IMF_FIXDATE,
)


class StaticTest(unittest.TestCase):
    def setUp(self):
        self.workspace = harness.Workspace()
        self.addCleanup(self.workspace.cleanup)
        w = self.workspace
        w.write("www/a.txt", "plain text\n")
        w.write("www/page.html", "<h1>page</h1>\n")
        w.write("www/dir/index.html", "dir index\n")
        w.write("www/list/b.txt", "b")
        w.write("www/list/<x&y>.txt", "x")
        os.mkdir(os.path.join(w.path, "www/list/sub"))
        os.mkdir(os.path.join(w.path, "www/empty"))
        w.write("www/errors/404.html", CUSTOM_404)
        w.write("tmp-www/pouic/toto/pouet", "kapouet\n")
        w.write("other/index.html", "other site\n")
        with open(os.path.join(w.path, "www/big.bin"), "wb") as big:
            big.write(bytes(range(256)) * (BIG_FILE_SIZE // 256))
        self.port = harness.free_port()
        self.other_port = harness.free_port()
        config = ("server {\n"
                  "    listen %s:%d;\n"
                  "    root www;\n"
                  "    error_page 404 /errors/404.html;\n"
                  "    location /list/ { autoindex on; }\n"
                  "    location /old/ { return 301 /new/; }\n"
                  "    location /kapouet { alias tmp-www; }\n"
                  "    location /slashed { alias tmp-www/; }\n"
                  "    location /ro/ { alias www/; "
                  "limit_except GET { deny all; } }\n"
                  "}\n"
                  "server {\n"
                  "    listen %s:%d;\n"
                  "    root other;\n"
                  "}\n" % (HOST, self.port,
                           HOST, self.other_port))
        path = w.write("test.conf", config)
        self.server = harness.RunningServer(
            [path], w.path, [self.port, self.other_port])
        self.addCleanup(self.server.stop)

    def fetch(self, raw, port=None):
        """Sends raw and returns the parsed response.
        raw를 보내고 해석한 응답을 돌려준다.
        """
        return harness.parse(harness.exchange(port or self.port, raw))

    def test_r06_file_with_type_and_length(self):
        status, headers, body = self.fetch(harness.get("/page.html"))
        self.assertEqual(200, status)
        self.assertEqual(b"<h1>page</h1>\n", body)
        self.assertEqual("text/html", headers["content-type"])
        self.assertEqual(str(len(body)), headers["content-length"])

    def test_r07_head_has_get_head_without_body(self):
        _, get_headers, _ = self.fetch(harness.get("/a.txt"))
        status, headers, body = self.fetch(harness.get("/a.txt", "HEAD"))
        self.assertEqual(200, status)
        self.assertEqual(b"", body)
        self.assertEqual(get_headers["content-length"],
                         headers["content-length"])
        self.assertEqual(get_headers["content-type"], headers["content-type"])

    def test_r08_r14_missing_file_uses_configured_page(self):
        status, headers, body = self.fetch(harness.get("/missing.txt"))
        self.assertEqual(404, status)
        self.assertEqual(CUSTOM_404.encode(), body)
        self.assertEqual("text/html", headers["content-type"])

    def test_r14_default_error_page_without_configuration(self):
        status, _, body = self.fetch(harness.get("/missing"), self.other_port)
        self.assertEqual(404, status)
        self.assertIn(b"404 Not Found", body)

    @unittest.skipIf(hasattr(os, "geteuid") and os.geteuid() == 0,
                     "root can read any file")
    def test_r09_unreadable_file_is_403(self):
        os.chmod(os.path.join(self.workspace.path, "www/a.txt"), 0)
        status, _, _ = self.fetch(harness.get("/a.txt"))
        self.assertEqual(403, status)

    def test_r10_directory_without_slash_redirects(self):
        status, headers, _ = self.fetch(harness.get("/dir?x=1"))
        self.assertEqual(301, status)
        self.assertEqual("/dir/?x=1", headers["location"])

    def test_r11_directory_serves_its_index(self):
        status, _, body = self.fetch(harness.get("/dir/"))
        self.assertEqual(200, status)
        self.assertEqual(b"dir index\n", body)

    def test_r12_listing_escapes_names(self):
        status, headers, body = self.fetch(harness.get("/list/"))
        self.assertEqual(200, status)
        self.assertEqual("text/html", headers["content-type"])
        self.assertIn(b'<a href="./b.txt">b.txt</a>', body)
        self.assertIn(b'<a href="./sub/">sub/</a>', body)
        self.assertIn(b'href="./%3Cx&amp;y%3E.txt">&lt;x&amp;y&gt;.txt</a>',
                      body)

    def test_r13_directory_without_index_or_listing_is_403(self):
        status, _, _ = self.fetch(harness.get("/empty/"))
        self.assertEqual(403, status)

    def test_r03_alias_follows_the_subject_example(self):
        status, _, body = self.fetch(harness.get("/kapouet/pouic/toto/pouet"))
        self.assertEqual(200, status)
        self.assertEqual(b"kapouet\n", body)

    def test_r20_alias_cannot_reach_outside(self):
        self.workspace.write("secret/s.txt", "SECRET\n")
        status, _, body = self.fetch(harness.get("/slashed../secret/s.txt"))
        self.assertEqual(404, status)
        self.assertNotIn(b"SECRET", body)
        status, _, body = self.fetch(harness.get("/slashed/pouic/toto/pouet"))
        self.assertEqual(200, status)

    def test_r04_return_redirects(self):
        status, headers, _ = self.fetch(harness.get("/old/anything"))
        self.assertEqual(301, status)
        self.assertEqual("/new/", headers["location"])

    def test_r05_r19_method_not_allowed_has_allow(self):
        status, headers, _ = self.fetch(harness.get("/ro/a.txt", "DELETE"))
        self.assertEqual(405, status)
        self.assertEqual("GET, HEAD", headers["allow"])
        status, headers, _ = self.fetch(harness.get("/a.txt", "POST"))
        self.assertEqual(405, status)
        self.assertEqual("GET, HEAD", headers["allow"])

    def test_r01_bad_paths_are_400(self):
        for path in ["/../etc/passwd", "/%2e%2e/etc/passwd", "/a%2Fb",
                     "/a%00b", "/dir/../../x"]:
            status, _, _ = self.fetch(harness.get(path))
            self.assertEqual(400, status, path)

    def test_r15_responses_carry_date(self):
        for path in ["/a.txt", "/missing", "/old/x"]:
            _, headers, _ = self.fetch(harness.get(path))
            self.assertRegex(headers["date"], IMF_FIXDATE)

    def test_r16_large_file_arrives_intact(self):
        status, headers, body = self.fetch(harness.get("/big.bin"))
        self.assertEqual(200, status)
        self.assertEqual(str(BIG_FILE_SIZE), headers["content-length"])
        self.assertEqual(bytes(range(256)) * (BIG_FILE_SIZE // 256), body)

    def test_r17_ports_serve_different_sites(self):
        _, _, first = self.fetch(harness.get("/"))
        _, _, second = self.fetch(harness.get("/"), self.other_port)
        self.assertEqual(DEFAULT_PAGE.encode(), first)
        self.assertEqual(b"other site\n", second)


if __name__ == "__main__":
    unittest.main()
