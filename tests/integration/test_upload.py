"""Uploads and DELETE (test plan F-xx and D-xx, step 6)."""

import os
import time
import unittest

import harness
from harness import get, request
from constants import HOST, WRAPPER_SLOWDOWN

BOUNDARY = "----WebKitFormBoundary7MA4YWxkTrZu0gW"  # as browsers make them
# Ends with CRLF and holds a near-delimiter, which a parser that keeps the
# CRLF before the delimiter or stops at "--" would get wrong.
MAX_UPLOAD_FILES = 5  # limits::kMaxUploadFiles
MAX_NAME_BYTES = 255  # kMaxFileNameBytes of UploadHandler.cpp
PARSE_SECONDS = 2  # a 1 MB body took 8 s when the parser was quadratic
BINARY = bytes(range(256)) + b"\r\n--" + BOUNDARY[:-1].encode() + b"\r\n"


def file_part(filename, content, name="file"):
    return ('Content-Disposition: form-data; name="%s"; filename="%s"\r\n'
            "Content-Type: application/octet-stream\r\n\r\n"
            % (name, filename)).encode("utf-8") + content


def field_part(name, value):
    return ('Content-Disposition: form-data; name="%s"\r\n\r\n'
            % name).encode() + value


def multipart(parts):
    delimiter = ("--" + BOUNDARY).encode()
    body = b"".join(delimiter + b"\r\n" + part + b"\r\n" for part in parts)
    return body + delimiter + b"--\r\n"


def post(path, body, content_type="multipart/form-data; boundary=" + BOUNDARY,
         chunked=False):
    head = ["POST %s HTTP/1.1" % path, "Host: x"]
    if content_type is not None:
        head.append("Content-Type: " + content_type)
    if chunked:
        head.append("Transfer-Encoding: chunked")
        body = b"%x\r\n%s\r\n0\r\n\r\n" % (len(body), body)
    else:
        head.append("Content-Length: %d" % len(body))
    return request(head, body)


class UploadTest(unittest.TestCase):
    def setUp(self):
        self.workspace = harness.Workspace()
        self.addCleanup(self.workspace.cleanup)
        w = self.workspace
        w.write("www/upload/kept.txt", "kept")
        os.mkdir(os.path.join(w.path, "www/upload/sub"))
        w.write("www/ro/a.txt", "read only")
        w.write("www/locked/a.txt", "locked")
        self.port = harness.free_port()
        config = ("server {\n"
                  "    listen %s:%d;\n"
                  "    root www;\n"
                  "    location /upload/ {\n"
                  "        limit_except GET POST DELETE { deny all; }\n"
                  "        upload_store www/upload;\n"
                  "    }\n"
                  "    location /ro/ { limit_except GET { deny all; } }\n"
                  "    location /locked/ {\n"
                  "        limit_except GET DELETE { deny all; }\n"
                  "    }\n"
                  "    location /nostore/ {\n"
                  "        limit_except GET POST { deny all; }\n"
                  "    }\n"
                  "    location /elsewhere/ {\n"
                  "        limit_except POST { deny all; }\n"
                  "        upload_store www/upload;\n"
                  "    }\n"
                  "    location /broken/ {\n"
                  "        limit_except POST { deny all; }\n"
                  "        upload_store missing-dir;\n"
                  "    }\n"
                  "}\n" % (HOST, self.port))
        path = w.write("test.conf", config)
        self.server = harness.RunningServer([path], w.path, [self.port])
        self.addCleanup(self.server.stop)

    def fetch(self, raw):
        return harness.parse(harness.exchange(self.port, raw))

    def stored(self, name):
        return os.path.join(self.workspace.path, "www/upload", name)

    def test_f01_uploaded_file_comes_back_byte_for_byte(self):
        status, _, _ = self.fetch(post("/upload/",
                                       multipart([file_part("b.bin", BINARY)])))
        self.assertEqual(201, status)
        with open(self.stored("b.bin"), "rb") as saved:
            self.assertEqual(BINARY, saved.read())
        status, _, body = self.fetch(get("/upload/b.bin"))
        self.assertEqual(200, status)
        self.assertEqual(BINARY, body)

    def test_f02_created_names_the_files(self):
        status, headers, body = self.fetch(post("/upload/", multipart(
            [file_part("a.txt", b"a"), file_part("c d.txt", b"c")])))
        self.assertEqual(201, status)
        self.assertEqual("/upload/a.txt", headers["location"])
        self.assertEqual(b"/upload/a.txt\n/upload/c%20d.txt\n", body)
        self.assertNotIn("connection", headers)

    def test_f02_no_location_when_get_cannot_reach_the_file(self):
        status, headers, body = self.fetch(post("/elsewhere/", multipart(
            [file_part("e.txt", b"e")])))
        self.assertEqual(201, status)
        self.assertNotIn("location", headers)
        self.assertEqual(b"e.txt\n", body)
        self.assertTrue(os.path.exists(self.stored("e.txt")))

    def test_f03_fields_and_empty_file_inputs_are_skipped(self):
        status, _, _ = self.fetch(post("/upload/", multipart(
            [field_part("note", b"hi"), file_part("", b""),
             file_part("a.txt", b"a")])))
        self.assertEqual(201, status)
        self.assertEqual(["a.txt", "kept.txt", "sub"],
                         sorted(os.listdir(self.stored(""))))
        status, _, _ = self.fetch(post("/upload/", multipart(
            [field_part("note", b"hi")])))
        self.assertEqual(400, status)

    def test_f05_other_media_types_are_415(self):
        for content_type in ["application/x-www-form-urlencoded",
                             "text/plain", None,
                             "multipart/mixed; boundary=" + BOUNDARY]:
            status, _, _ = self.fetch(post("/upload/", b"a=b", content_type))
            self.assertEqual(415, status, content_type)

    def test_f06_malformed_bodies_are_400(self):
        body = multipart([file_part("a.txt", b"a")])
        for raw in [post("/upload/", body, "multipart/form-data"),
                    post("/upload/", body[:-len(b"--\r\n") - 2]),
                    post("/upload/", b"no delimiter at all")]:
            status, _, _ = self.fetch(raw)
            self.assertEqual(400, status)
        self.assertFalse(os.path.exists(self.stored("a.txt")))

    # In a quoted-string "\" quotes the next octet (RFC 9110 section
    # 5.6.4), so a Windows path arrives as "C:\\Users\\win.txt"; a bare
    # "\" is just dropped.
    def test_f07_directory_part_of_filename_is_dropped(self):
        status, _, _ = self.fetch(post("/upload/", multipart(
            [file_part("../../evil.txt", b"e"),
             file_part("C:\\\\Users\\\\me\\\\win.txt", b"w"),
             file_part("C:\\Users\\bare.txt", b"b")])))
        self.assertEqual(201, status)
        self.assertTrue(os.path.exists(self.stored("evil.txt")))
        self.assertTrue(os.path.exists(self.stored("win.txt")))
        self.assertTrue(os.path.exists(self.stored("C:Usersbare.txt")))
        self.assertFalse(os.path.exists(
            os.path.join(self.workspace.path, "evil.txt")))
        for filename in ["..", "dir/", "x/.."]:
            status, _, _ = self.fetch(post("/upload/", multipart(
                [file_part(filename, b"x")])))
            self.assertEqual(400, status, filename)

    def test_f07_name_length_is_limited(self):
        status, _, _ = self.fetch(post("/upload/", multipart(
            [file_part("n" * MAX_NAME_BYTES, b"n")])))
        self.assertEqual(201, status)
        status, _, _ = self.fetch(post("/upload/", multipart(
            [file_part("m" * (MAX_NAME_BYTES + 1), b"m")])))
        self.assertEqual(400, status)

    def test_f15_too_many_files_is_413(self):
        names = ["f%d" % i for i in range(MAX_UPLOAD_FILES + 1)]
        status, _, _ = self.fetch(post("/upload/", multipart(
            [file_part(name, b"x") for name in names])))
        self.assertEqual(413, status)
        self.assertFalse(os.path.exists(self.stored(names[0])))
        status, _, _ = self.fetch(post("/upload/", multipart(
            [file_part(name, b"x") for name in names[:-1]])))
        self.assertEqual(201, status)

    # Parts without an empty line made the parser scan the rest of the
    # body once per part; the event loop waited 8 s for 1 MB.
    def test_f16_many_parts_parse_in_linear_time(self):
        part = b'Content-Disposition: form-data; name="a"'
        body = multipart([part] * (1000000 // (len(part) + 48)))
        started = time.monotonic()
        status, _, _ = self.fetch(post("/upload/", body))
        self.assertEqual(400, status)  # fields only, no file
        self.assertLess(time.monotonic() - started,
                        PARSE_SECONDS * WRAPPER_SLOWDOWN)

    def test_f08_existing_or_repeated_names_are_409(self):
        status, _, body = self.fetch(post("/upload/", multipart(
            [file_part("new.txt", b"n"), file_part("kept.txt", b"other")])))
        self.assertEqual(409, status)
        self.assertIn(b'"kept.txt"', body)
        self.assertFalse(os.path.exists(self.stored("new.txt")))
        with open(self.stored("kept.txt")) as kept:
            self.assertEqual("kept", kept.read())
        status, _, _ = self.fetch(post("/upload/", multipart(
            [file_part("twice.txt", b"1"), file_part("twice.txt", b"2")])))
        self.assertEqual(409, status)
        self.assertFalse(os.path.exists(self.stored("twice.txt")))
        status, _, _ = self.fetch(post("/upload/", multipart(
            [file_part("sub", b"s")])))
        self.assertEqual(409, status)

    def test_f09_post_without_upload_store_is_405(self):
        status, headers, _ = self.fetch(post("/nostore/", multipart(
            [file_part("a.txt", b"a")])))
        self.assertEqual(405, status)
        self.assertEqual("GET, HEAD", headers["allow"])

    def test_f10_chunked_upload(self):
        status, _, _ = self.fetch(post("/upload/", multipart(
            [file_part("chunked.bin", BINARY)]), chunked=True))
        self.assertEqual(201, status)
        with open(self.stored("chunked.bin"), "rb") as saved:
            self.assertEqual(BINARY, saved.read())

    def test_f12_writes_need_limit_except(self):
        status, headers, _ = self.fetch(post("/", multipart(
            [file_part("a.txt", b"a")])))
        self.assertEqual(405, status)
        self.assertEqual("GET, HEAD", headers["allow"])
        status, headers, _ = self.fetch(get("/index.html", "DELETE"))
        self.assertEqual(405, status)
        self.assertEqual("GET, HEAD", headers["allow"])
        self.assertTrue(os.path.exists(
            os.path.join(self.workspace.path, "www/index.html")))

    def test_f13_missing_store_is_500(self):
        status, _, _ = self.fetch(post("/broken/", multipart(
            [file_part("a.txt", b"a")])))
        self.assertEqual(500, status)

    def test_d01_deleted_file_is_gone(self):
        self.fetch(post("/upload/", multipart([file_part("gone.txt", b"g")])))
        with harness.connect(self.port) as sock, \
                sock.makefile("rb") as stream:
            sock.sendall(get("/upload/gone.txt", "DELETE") +
                         get("/upload/gone.txt"))
            first = harness.read_response(stream)
            status, headers, body = harness.parse(first)
            self.assertEqual(204, status)
            self.assertNotIn("content-length", headers)
            self.assertEqual(b"", body)
            self.assertEqual(404, harness.parse(
                harness.read_response(stream))[0])
        self.assertFalse(os.path.exists(self.stored("gone.txt")))

    def test_d02_d03_missing_file_and_directory(self):
        self.assertEqual(404, self.fetch(get("/upload/none.txt", "DELETE"))[0])
        self.assertEqual(403, self.fetch(get("/upload/sub/", "DELETE"))[0])
        self.assertEqual(403, self.fetch(get("/upload/sub", "DELETE"))[0])
        self.assertTrue(os.path.isdir(self.stored("sub")))

    def test_d04_delete_without_permission_is_405(self):
        status, headers, _ = self.fetch(get("/ro/a.txt", "DELETE"))
        self.assertEqual(405, status)
        self.assertEqual("GET, HEAD", headers["allow"])
        self.assertTrue(os.path.exists(
            os.path.join(self.workspace.path, "www/ro/a.txt")))

    @unittest.skipIf(os.geteuid() == 0, "root may change any directory")
    def test_d06_file_in_unwritable_directory_is_403(self):
        locked = os.path.join(self.workspace.path, "www/locked")
        os.chmod(locked, 0o555)
        self.addCleanup(os.chmod, locked, 0o755)
        self.assertEqual(403, self.fetch(get("/locked/a.txt", "DELETE"))[0])
        self.assertTrue(os.path.exists(os.path.join(locked, "a.txt")))


if __name__ == "__main__":
    unittest.main()
