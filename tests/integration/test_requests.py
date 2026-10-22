"""Responses to parsed and malformed requests (test plan P-xx, step 3)."""

import time
import unittest

import harness
from harness import request
from constants import (
    HEADER_TIMEOUT_SECONDS,
    HOST,
    KEEP_ALIVE_TIMEOUT_SECONDS,
    RUN_SLOW,
    SMALL_LIMIT,
    TIMEOUT_MARGIN_SECONDS,
)


class RequestTest(unittest.TestCase):
    def setUp(self):
        self.workspace = harness.Workspace()
        self.addCleanup(self.workspace.cleanup)
        self.port = harness.free_port()
        config = ("server {\n"
                  "    listen %s:%d;\n"
                  "    root www;\n"
                  "    location /small/ { client_max_body_size %d; }\n"
                  "}\n" % (HOST, self.port, SMALL_LIMIT))
        path = self.workspace.write("test.conf", config)
        self.server = harness.RunningServer([path], self.workspace.path,
                                            [self.port])
        self.addCleanup(self.server.stop)

    def status_for(self, raw):
        return harness.status_line(harness.exchange(self.port, raw))

    def assert_status(self, code, raw):
        line = self.status_for(raw)
        self.assertTrue(line.startswith("HTTP/1.1 %d " % code), line)
        self.assertTrue(self.server.is_running())

    def test_p06_unknown_method_is_501(self):
        self.assert_status(501, request(["UNKNOWN / HTTP/1.1", "Host: x"]))
        self.assert_status(501, request(["PUT / HTTP/1.1", "Host: x"]))

    def test_p07_long_target_is_414(self):
        self.assert_status(414, request(["GET /" + "a" * 9000 + " HTTP/1.1",
                                         "Host: x"]))

    def test_p08_unsupported_major_version_is_505(self):
        self.assert_status(505, request(["GET / HTTP/2.0", "Host: x"]))

    def test_p12_missing_host_is_400(self):
        self.assert_status(400, request(["GET / HTTP/1.1"]))

    def test_p13_large_header_section_is_431(self):
        self.assert_status(431, request(["GET / HTTP/1.1", "Host: x",
                                         "X: " + "a" * 17000]))

    def test_p03_lf_only_request_is_accepted(self):
        self.assert_status(200, b"GET / HTTP/1.1\nHost: x\n\n")

    # A body within the limit is read completely and then routed. Without
    # limit_except a location answers POST with 405 (decision 16).
    def test_p16_body_limit_follows_the_location(self):
        self.assert_status(413, request(["POST /small/a HTTP/1.1", "Host: x",
                                         "Content-Length: 11"]))
        self.assert_status(405, request(["POST /small/a HTTP/1.1", "Host: x",
                                         "Content-Length: 10"], b"0123456789"))
        self.assert_status(405, request(["POST /other HTTP/1.1", "Host: x",
                                         "Content-Length: 11"], b"x" * 11))

    def test_p17_chunked_body_limit(self):
        chunked = ["POST /small/a HTTP/1.1", "Host: x",
                   "Transfer-Encoding: chunked"]
        self.assert_status(405, request(chunked, b"5\r\nhello\r\n0\r\n\r\n"))
        self.assert_status(413, request(chunked, b"B\r\n"))

    def test_r21_head_error_has_no_body(self):
        response = harness.exchange(self.port, request(
            ["HEAD /small/a HTTP/1.1", "Host: x", "Content-Length: 11"]))
        status, headers, body = harness.parse(response)
        self.assertEqual(413, status)
        self.assertNotEqual("0", headers["content-length"])
        self.assertEqual(b"", body)

    def test_p19_error_response_format_and_close(self):
        with harness.connect(self.port) as sock:
            sock.sendall(request(["GET / HTTP/1.1"]))
            response = harness.read_until_closed(sock)
        head = response.split(b"\r\n\r\n", 1)[0].split(b"\r\n")
        self.assertEqual(b"HTTP/1.1 400 Bad Request", head[0])
        self.assertIn(b"Connection: close", head)
        self.assertTrue(any(line.startswith(b"Content-Length: ")
                            for line in head))

    @unittest.skipUnless(RUN_SLOW, "set WEBSERV_SLOW_TESTS=1 (takes ~35s)")
    def test_p18_unfinished_body_gets_408(self):
        with harness.connect(self.port) as sock:
            sock.settimeout(HEADER_TIMEOUT_SECONDS + TIMEOUT_MARGIN_SECONDS)
            sock.sendall(request(["POST / HTTP/1.1", "Host: x",
                                  "Content-Length: 5"], b"ab"))
            response = harness.read_until_closed(sock)
        self.assertTrue(response.startswith(b"HTTP/1.1 408 "), response)

    # A connection that has not sent a byte waits like a keep-alive one.
    @unittest.skipUnless(RUN_SLOW, "set WEBSERV_SLOW_TESTS=1 (takes ~15s)")
    def test_p18_silent_connection_is_closed_without_response(self):
        with harness.connect(self.port) as sock:
            sock.settimeout(KEEP_ALIVE_TIMEOUT_SECONDS + TIMEOUT_MARGIN_SECONDS)
            started = time.monotonic()
            received = harness.read_until_closed(sock)
            waited = time.monotonic() - started
        self.assertEqual(b"", received)
        self.assertGreaterEqual(waited, KEEP_ALIVE_TIMEOUT_SECONDS - 1)


if __name__ == "__main__":
    unittest.main()
