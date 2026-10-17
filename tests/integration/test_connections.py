"""Connection handling (test plan N-04, N-05, N-07..N-09, N-19, N-24)."""

import os
import socket
import struct
import threading
import time
import unittest

import harness
from constants import (
    ABORTED_CLIENTS,
    CONCURRENT_CLIENTS,
    FD_SETTLE_SECONDS,
    HEADER_TIMEOUT_SECONDS,
    HEAD_LIMIT_BYTES,
    IDLE_CLIENTS,
    IDLE_SECONDS,
    LARGE_RESPONSE_SIZE,
    MAX_IDLE_CPU_SHARE,
    RECV_SIZE,
    RESET_DURING_SEND_CLIENTS,
    RESET_SETTLE_SECONDS,
    RETRY_INTERVAL_SECONDS,
    RUN_SLOW,
    SIMPLE_GET,
    TIMEOUT_MARGIN_SECONDS,
    TRICKLE_INTERVAL_SECONDS,
)


def reset_on_close(sock):
    """SO_LINGER with a zero timeout: close() sends RST instead of FIN."""
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER,
                    struct.pack("ii", 1, 0))


class ConnectionTest(unittest.TestCase):
    def setUp(self):
        self.workspace = harness.Workspace()
        self.addCleanup(self.workspace.cleanup)
        self.ports = [harness.free_port(), harness.free_port()]
        config = "".join(harness.server_block(port) for port in self.ports)
        path = self.workspace.write("test.conf", config)
        self.server = harness.RunningServer([path], self.workspace.path,
                                            self.ports)
        self.addCleanup(self.server.stop)

    # SIMPLE_GET asks the server to close, so once this returns the server
    # holds no descriptor for these requests (N-04 counts them).
    def assert_still_serving(self):
        self.assertTrue(self.server.is_running())
        for port in self.ports:
            with harness.connect(port) as sock:
                sock.sendall(SIMPLE_GET)
                response = harness.read_until_closed(sock)
            self.assertIn("200", harness.status_line(response))

    def test_n05_concurrent_clients_on_two_ports(self):
        statuses = []
        lock = threading.Lock()

        def client(port):
            line = harness.status_line(harness.exchange(port))
            with lock:
                statuses.append(line)

        threads = [threading.Thread(target=client,
                                    args=(self.ports[i % 2],))
                   for i in range(CONCURRENT_CLIENTS)]
        for thread in threads:
            thread.start()
        for thread in threads:
            thread.join()
        self.assertEqual(CONCURRENT_CLIENTS, len(statuses))
        for line in statuses:
            self.assertIn("200", line)

    def test_n07_client_leaves_in_the_middle_of_a_request(self):
        with harness.connect(self.ports[0]) as sock:
            sock.sendall(b"GET / HTTP/1.1\r\nHost: loc")
        self.assert_still_serving()

    def test_n07_client_resets_before_reading_the_response(self):
        # The server's send() may hit a reset connection. This does not
        # provoke SIGPIPE yet: the fixed response goes out in one send()
        # (removing SIG_IGN did not make this test fail). Large responses
        # in step 4 need a test that keeps sending after the reset.
        for _ in range(ABORTED_CLIENTS):
            sock = harness.connect(self.ports[0])
            reset_on_close(sock)
            sock.sendall(SIMPLE_GET)
            sock.close()
        self.assert_still_serving()

    # A large file needs many send() calls. The client empties its buffer
    # and closes with FIN, so the server's next send() still succeeds, the
    # client answers it with RST, and the send() after that raises SIGPIPE,
    # which ends the process unless it is ignored (raw/macos-observed-
    # sigpipe-leaks.md:114-117 in the wiki). Closing with unread data
    # would send RST at once; then the first send() fails and the server
    # closes before a second one.
    def test_n07_client_leaves_while_a_large_response_is_sent(self):
        with open(os.path.join(self.workspace.path, "www/large.bin"),
                  "wb") as out:
            out.write(b"L" * LARGE_RESPONSE_SIZE)
        for _ in range(RESET_DURING_SEND_CLIENTS):
            sock = harness.connect(self.ports[0])
            sock.sendall(harness.get("/large.bin"))
            sock.recv(RECV_SIZE)  # the response has started
            sock.setblocking(False)
            try:
                while sock.recv(RECV_SIZE):
                    pass
            except BlockingIOError:
                pass  # nothing unread left: close() sends FIN
            sock.close()
        time.sleep(RESET_SETTLE_SECONDS)
        self.assert_still_serving()

    @unittest.skipUnless(os.path.isdir("/proc/self/fd"),
                         "needs /proc (run on Linux)")
    def test_n04_peer_close_releases_the_socket(self):
        # recv() returning 0 must close the client socket. If it did not,
        # the descriptor would stay open until the header timeout.
        self.assert_still_serving()
        baseline = self.server.open_fd_count()
        clients = [harness.connect(self.ports[0])
                   for _ in range(ABORTED_CLIENTS)]
        self.wait_for_fd_count(baseline + ABORTED_CLIENTS)
        for sock in clients:
            sock.close()
        self.wait_for_fd_count(baseline)

    @unittest.skipUnless(os.path.isdir("/proc/self/fd"),
                         "needs /proc (run on Linux)")
    def test_n19_idle_connections_do_not_spin(self):
        # With POLLOUT requested while there is nothing to send, poll()
        # would return at once every time and the loop would burn CPU.
        clients = [harness.connect(self.ports[0])
                   for _ in range(IDLE_CLIENTS)]
        self.addCleanup(lambda: [sock.close() for sock in clients])
        before = self.server.cpu_seconds()
        time.sleep(IDLE_SECONDS)
        used = self.server.cpu_seconds() - before
        self.assertLess(used, IDLE_SECONDS * MAX_IDLE_CPU_SHARE)

    @unittest.skipUnless(RUN_SLOW, "set WEBSERV_SLOW_TESTS=1 (takes ~35s)")
    def test_n24_trickled_head_still_times_out(self):
        # One byte every few seconds must not extend the header timeout.
        head = b"GET / HTTP/1.1\r\nX: " + b"a" * 100
        with harness.connect(self.ports[0]) as sock:
            started = time.monotonic()
            sent = 0
            sock.settimeout(TRICKLE_INTERVAL_SECONDS)
            response = b""
            while time.monotonic() - started < (HEADER_TIMEOUT_SECONDS
                                                + TIMEOUT_MARGIN_SECONDS):
                sock.sendall(head[sent:sent + 1])
                sent += 1
                try:
                    response = sock.recv(RECV_SIZE)
                    break
                except socket.timeout:
                    continue
            waited = time.monotonic() - started
        self.assertTrue(response.startswith(b"HTTP/1.1 408 "), response)
        self.assertLess(waited,
                        HEADER_TIMEOUT_SECONDS + TIMEOUT_MARGIN_SECONDS)

    def wait_for_fd_count(self, expected):
        deadline = time.monotonic() + FD_SETTLE_SECONDS
        while self.server.open_fd_count() != expected:
            if time.monotonic() > deadline:
                self.fail("server has %d descriptors open, expected %d"
                          % (self.server.open_fd_count(), expected))
            time.sleep(RETRY_INTERVAL_SECONDS)

    def test_n09_oversized_head_gets_431(self):
        with harness.connect(self.ports[0]) as sock:
            sock.sendall(b"GET / HTTP/1.1\r\nX: " + b"a" * HEAD_LIMIT_BYTES)
            received = harness.read_until_closed(sock)
        self.assertTrue(received.startswith(b"HTTP/1.1 431 "), received[:40])
        self.assert_still_serving()

    @unittest.skipUnless(RUN_SLOW, "set WEBSERV_SLOW_TESTS=1 (takes ~35s)")
    def test_n08_unfinished_head_gets_408(self):
        with harness.connect(self.ports[0]) as sock:
            sock.settimeout(HEADER_TIMEOUT_SECONDS + TIMEOUT_MARGIN_SECONDS)
            sock.sendall(b"GET / HTTP/1.1\r\n")
            started = time.monotonic()
            received = harness.read_until_closed(sock)
            waited = time.monotonic() - started
        self.assertTrue(received.startswith(b"HTTP/1.1 408 "), received)
        self.assertGreaterEqual(waited, HEADER_TIMEOUT_SECONDS - 1)


if __name__ == "__main__":
    unittest.main()
