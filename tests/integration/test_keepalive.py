"""Keep-alive, pipelining, draining and timeouts (test plan K-xx, step 5)."""

import socket
import time
import unittest

import harness
from harness import get, request
from constants import (
    BIG_FILE_SIZE,
    DRAIN_POLL_SECONDS,
    DRAIN_TOTAL_SECONDS,
    HEADER_TIMEOUT_SECONDS,
    HOST,
    KEEP_ALIVE_PAUSE_SECONDS,
    KEEP_ALIVE_TIMEOUT_SECONDS,
    RUN_SLOW,
    SEND_TIMEOUT_SECONDS,
    SMALL_LIMIT,
    TIMEOUT_MARGIN_SECONDS,
    UNREAD_FILE_SIZE,
)


def responses_in(received):
    """Number of status lines in everything a connection received."""
    return received.count(b"HTTP/1.1 ")


class KeepAliveTest(unittest.TestCase):
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

    def connect(self):
        sock = harness.connect(self.port)
        stream = sock.makefile("rb")
        self.addCleanup(sock.close)
        self.addCleanup(stream.close)
        return sock, stream

    def test_k01_connection_stays_open_after_a_response(self):
        sock, stream = self.connect()
        for path, expected in [("/", 200), ("/missing", 404), ("/", 200)]:
            sock.sendall(get(path))
            status, headers, _ = harness.parse(harness.read_response(stream))
            self.assertEqual(expected, status)
            self.assertNotIn("connection", headers)

    def test_k02_close_option_ends_the_connection(self):
        sock, _ = self.connect()
        sock.sendall(request(["GET / HTTP/1.1", "Host: x",
                              "Connection: Keep-Alive, CLOSE"]) + get("/"))
        received = harness.read_until_closed(sock)
        _, headers, _ = harness.parse(received)
        self.assertEqual("close", headers["connection"])
        self.assertEqual(1, responses_in(received))

    def test_k03_http10_closes_even_with_keep_alive(self):
        sock, _ = self.connect()
        sock.sendall(request(["GET / HTTP/1.0", "Connection: keep-alive"]))
        received = harness.read_until_closed(sock)
        status, headers, _ = harness.parse(received)
        self.assertEqual(200, status)
        self.assertEqual("close", headers["connection"])

    def test_k04_pipelined_requests_are_answered_in_order(self):
        sock, stream = self.connect()
        sock.sendall(get("/") + get("/missing") + get("/", "HEAD") + get("/"))
        statuses = []
        for head_only in [False, False, True, False]:
            response = harness.read_response(stream, head_only)
            statuses.append(harness.parse(response)[0])
        self.assertEqual([200, 404, 200, 200], statuses)

    def test_k05_untrusted_requests_close_the_connection(self):
        bad_requests = [
            request(["GET / HTTP/1.1"]),  # parser: no Host
            get("/../x"),  # router: above the root (decision 2)
            request(["POST /small/ HTTP/1.1", "Host: x",
                     "Content-Length: %d" % (SMALL_LIMIT + 1)]),
        ]
        for bad in bad_requests:
            sock, _ = self.connect()
            sock.sendall(bad + get("/"))
            received = harness.read_until_closed(sock)
            _, headers, _ = harness.parse(received)
            self.assertEqual("close", headers["connection"])
            self.assertEqual(1, responses_in(received), received[:200])

    # Like the client of raw/close-after-413-observed.md in the wiki: it
    # sends the whole body before reading. Without draining, the server's
    # close made that send fail with a broken pipe.
    def test_k06_whole_body_can_be_sent_after_413(self):
        sock, _ = self.connect()
        sock.sendall(request(["POST /small/ HTTP/1.1", "Host: x",
                              "Content-Length: %d" % BIG_FILE_SIZE]))
        sock.sendall(b"x" * BIG_FILE_SIZE)
        received = harness.read_until_closed(sock)
        self.assertTrue(received.startswith(b"HTTP/1.1 413 "), received)

    def wait_for_close_while_sending(self, sock, keep_sending):
        """Seconds until the server closes; sends a byte now and then if
        keep_sending."""
        sock.settimeout(DRAIN_POLL_SECONDS)
        started = time.monotonic()
        deadline = started + DRAIN_TOTAL_SECONDS + TIMEOUT_MARGIN_SECONDS
        while time.monotonic() < deadline:
            try:
                if keep_sending:
                    sock.send(b"x")
                if sock.recv(1) == b"":
                    break
            except socket.timeout:
                continue
            except OSError:
                break
        return time.monotonic() - started

    def start_drain(self):
        sock, stream = self.connect()
        sock.sendall(request(["POST /small/ HTTP/1.1", "Host: x",
                              "Content-Length: %d" % (SMALL_LIMIT + 1)]))
        self.assertEqual(413, harness.parse(harness.read_response(stream))[0])
        return sock

    def test_k07_drain_ends_soon_without_input(self):
        waited = self.wait_for_close_while_sending(self.start_drain(), False)
        self.assertLess(waited, DRAIN_TOTAL_SECONDS - 1)

    def test_k07_drain_ends_after_its_total_time(self):
        waited = self.wait_for_close_while_sending(self.start_drain(), True)
        self.assertGreaterEqual(waited, DRAIN_TOTAL_SECONDS - 1)
        self.assertLess(waited, DRAIN_TOTAL_SECONDS + TIMEOUT_MARGIN_SECONDS)

    @unittest.skipUnless(RUN_SLOW, "set WEBSERV_SLOW_TESTS=1 (takes ~15s)")
    def test_k08_idle_keep_alive_connection_is_closed(self):
        sock, stream = self.connect()
        sock.sendall(get("/"))
        self.assertEqual(200, harness.parse(harness.read_response(stream))[0])
        sock.settimeout(KEEP_ALIVE_TIMEOUT_SECONDS + TIMEOUT_MARGIN_SECONDS)
        started = time.monotonic()
        received = harness.read_until_closed(sock)
        waited = time.monotonic() - started
        self.assertEqual(b"", received)
        self.assertGreaterEqual(waited, KEEP_ALIVE_TIMEOUT_SECONDS - 1)

    # The send timeout counts from the last send() that made progress. TCP
    # may still take a little after the buffers fill (on macOS one more
    # send() succeeded 5 seconds later), so the wait has a double margin.
    @unittest.skipUnless(RUN_SLOW, "set WEBSERV_SLOW_TESTS=1 (takes ~40s)")
    def test_k09_unread_response_times_out(self):
        with open(self.workspace.path + "/www/unread.bin", "wb") as out:
            out.write(b"u" * UNREAD_FILE_SIZE)
        sock, _ = self.connect()
        sock.sendall(get("/unread.bin"))
        time.sleep(SEND_TIMEOUT_SECONDS + 2 * TIMEOUT_MARGIN_SECONDS)
        received = harness.read_until_closed(sock)
        self.assertTrue(received.startswith(b"HTTP/1.1 200 "))
        self.assertLess(len(received), UNREAD_FILE_SIZE)

    @unittest.skipUnless(RUN_SLOW, "set WEBSERV_SLOW_TESTS=1 (takes ~40s)")
    def test_k10_head_timeout_starts_at_the_first_byte(self):
        sock, stream = self.connect()
        sock.sendall(get("/"))
        self.assertEqual(200, harness.parse(harness.read_response(stream))[0])
        time.sleep(KEEP_ALIVE_PAUSE_SECONDS)
        sock.sendall(b"GET / HTTP/1.1\r\n")
        sock.settimeout(HEADER_TIMEOUT_SECONDS + TIMEOUT_MARGIN_SECONDS)
        started = time.monotonic()
        received = harness.read_until_closed(sock)
        waited = time.monotonic() - started
        self.assertTrue(received.startswith(b"HTTP/1.1 408 "), received)
        self.assertGreaterEqual(waited, HEADER_TIMEOUT_SECONDS - 1)


if __name__ == "__main__":
    unittest.main()
