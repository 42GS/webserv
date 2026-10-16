"""Routing, static files and responses (test plan R-xx, step 4)."""

import os
import re
import unittest

import harness

BIG_FILE_SIZE = 10 * 1024 * 1024
IMF_FIXDATE = re.compile(
    r"^(Mon|Tue|Wed|Thu|Fri|Sat|Sun), \d\d "
    r"(Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|Sep|Oct|Nov|Dec) \d{4} "
    r"\d\d:\d\d:\d\d GMT$")
CUSTOM_404 = "<p>custom not found</p>\n"


def get(path, method="GET"):
    return ("%s %s HTTP/1.1\r\nHost: x\r\n\r\n" % (method, path)).encode()


def parse(response):
    """(status code, {lower-case name: value}, body)."""
    head, _, body = response.partition(b"\r\n\r\n")
    lines = head.decode("latin-1").split("\r\n")
    status = int(lines[0].split(" ")[1])
    headers = {}
    for line in lines[1:]:
        name, _, value = line.partition(":")
        headers[name.strip().lower()] = value.strip()
    return status, headers, body


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
                  "    location /ro/ { alias www/; "
                  "limit_except GET { deny all; } }\n"
                  "}\n"
                  "server {\n"
                  "    listen %s:%d;\n"
                  "    root other;\n"
                  "}\n" % (harness.HOST, self.port,
                           harness.HOST, self.other_port))
        path = w.write("test.conf", config)
        self.server = harness.RunningServer(
            [path], w.path, [self.port, self.other_port])
        self.addCleanup(self.server.stop)

    def fetch(self, raw, port=None):
        return parse(harness.exchange(port or self.port, raw))

    def test_r06_file_with_type_and_length(self):
        status, headers, body = self.fetch(get("/page.html"))
        self.assertEqual(200, status)
        self.assertEqual(b"<h1>page</h1>\n", body)
        self.assertEqual("text/html", headers["content-type"])
        self.assertEqual(str(len(body)), headers["content-length"])

    def test_r07_head_has_get_head_without_body(self):
        _, get_headers, _ = self.fetch(get("/a.txt"))
        status, headers, body = self.fetch(get("/a.txt", "HEAD"))
        self.assertEqual(200, status)
        self.assertEqual(b"", body)
        self.assertEqual(get_headers["content-length"],
                         headers["content-length"])
        self.assertEqual(get_headers["content-type"], headers["content-type"])

    def test_r08_r14_missing_file_uses_configured_page(self):
        status, headers, body = self.fetch(get("/missing.txt"))
        self.assertEqual(404, status)
        self.assertEqual(CUSTOM_404.encode(), body)
        self.assertEqual("text/html", headers["content-type"])

    def test_r14_default_error_page_without_configuration(self):
        status, _, body = self.fetch(get("/missing"), self.other_port)
        self.assertEqual(404, status)
        self.assertIn(b"404 Not Found", body)

    @unittest.skipIf(hasattr(os, "geteuid") and os.geteuid() == 0,
                     "root can read any file")
    def test_r09_unreadable_file_is_403(self):
        os.chmod(os.path.join(self.workspace.path, "www/a.txt"), 0)
        status, _, _ = self.fetch(get("/a.txt"))
        self.assertEqual(403, status)

    def test_r10_directory_without_slash_redirects(self):
        status, headers, _ = self.fetch(get("/dir?x=1"))
        self.assertEqual(301, status)
        self.assertEqual("/dir/?x=1", headers["location"])

    def test_r11_directory_serves_its_index(self):
        status, _, body = self.fetch(get("/dir/"))
        self.assertEqual(200, status)
        self.assertEqual(b"dir index\n", body)

    def test_r12_listing_escapes_names(self):
        status, headers, body = self.fetch(get("/list/"))
        self.assertEqual(200, status)
        self.assertEqual("text/html", headers["content-type"])
        self.assertIn(b'<a href="./b.txt">b.txt</a>', body)
        self.assertIn(b'<a href="./sub/">sub/</a>', body)
        self.assertIn(b'href="./%3Cx&amp;y%3E.txt">&lt;x&amp;y&gt;.txt</a>',
                      body)

    def test_r13_directory_without_index_or_listing_is_403(self):
        status, _, _ = self.fetch(get("/empty/"))
        self.assertEqual(403, status)

    def test_r03_alias_follows_the_subject_example(self):
        status, _, body = self.fetch(get("/kapouet/pouic/toto/pouet"))
        self.assertEqual(200, status)
        self.assertEqual(b"kapouet\n", body)

    def test_r04_return_redirects(self):
        status, headers, _ = self.fetch(get("/old/anything"))
        self.assertEqual(301, status)
        self.assertEqual("/new/", headers["location"])

    def test_r05_r19_method_not_allowed_has_allow(self):
        status, headers, _ = self.fetch(get("/ro/a.txt", "DELETE"))
        self.assertEqual(405, status)
        self.assertEqual("GET, HEAD", headers["allow"])
        status, headers, _ = self.fetch(get("/a.txt", "POST"))
        self.assertEqual(405, status)
        self.assertEqual("GET, HEAD", headers["allow"])

    def test_r01_bad_paths_are_400(self):
        for path in ["/../etc/passwd", "/%2e%2e/etc/passwd", "/a%2Fb",
                     "/a%00b", "/dir/../../x"]:
            status, _, _ = self.fetch(get(path))
            self.assertEqual(400, status, path)

    def test_r15_responses_carry_date(self):
        for path in ["/a.txt", "/missing", "/old/x"]:
            _, headers, _ = self.fetch(get(path))
            self.assertRegex(headers["date"], IMF_FIXDATE)

    def test_r16_large_file_arrives_intact(self):
        status, headers, body = self.fetch(get("/big.bin"))
        self.assertEqual(200, status)
        self.assertEqual(str(BIG_FILE_SIZE), headers["content-length"])
        self.assertEqual(bytes(range(256)) * (BIG_FILE_SIZE // 256), body)

    def test_r17_ports_serve_different_sites(self):
        _, _, first = self.fetch(get("/"))
        _, _, second = self.fetch(get("/"), self.other_port)
        self.assertEqual(harness.DEFAULT_PAGE.encode(), first)
        self.assertEqual(b"other site\n", second)


if __name__ == "__main__":
    unittest.main()
