"""Connection handling (test plan N-04, N-05, N-07..N-09, N-19, N-24,
N-26).
연결 처리 (테스트 계획 N-04, N-05, N-07..N-09, N-19, N-24, N-26).
"""

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
    SMALL_FD_LIMIT,
    TIMEOUT_MARGIN_SECONDS,
    TRICKLE_INTERVAL_SECONDS,
    WRAPPER,
)


def reset_on_close(sock):
    """SO_LINGER with a zero timeout: close() sends RST instead of FIN.
    시간이 0인 SO_LINGER: close()가 FIN 대신 RST를 보낸다.
    """
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

    def assert_still_serving(self):
        """Checks that the server runs and answers 200 on every port.
        SIMPLE_GET asks the server to close, so once this returns the server
        holds no descriptor for these requests (N-04 counts them).
        서버가 돌고 모든 포트에서 200으로 답하는지 검사한다. SIMPLE_GET은
        서버에 연결을 닫으라고 하므로, 이것이 끝나면 서버는 이 요청들의
        디스크립터를 갖고 있지 않다 (N-04가 디스크립터를 센다).
        """
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
        # provoke SIGPIPE: the response goes out in one send() (removing
        # SIG_IGN did not make this test fail).
        # 서버의 send()가 리셋된 연결을 만날 수 있다. 이것으로 SIGPIPE가 나지는
        # 않는다. 응답이 send() 한 번에 나간다 (SIG_IGN을 빼도 이 테스트는
        # 실패하지 않았다).
        for _ in range(ABORTED_CLIENTS):
            sock = harness.connect(self.ports[0])
            reset_on_close(sock)
            sock.sendall(SIMPLE_GET)
            sock.close()
        self.assert_still_serving()

    # A large file needs many send() calls. The client empties its buffer
    # and closes with FIN, aiming at this sequence: the server's next
    # send() still succeeds, the client answers it with RST, and the
    # send() after that raises SIGPIPE, which ends the process unless it is
    # ignored (raw/macos-observed-sigpipe-leaks.md:114-117 in the wiki). On
    # macOS and Ubuntu the server saw POLLERR/POLLHUP of the RST first and
    # closed without that send() (test plan N-07), so this checks only that
    # the server survives; signals_test.cpp checks that SIGPIPE is ignored.
    # Closing with unread data would send RST at once; then the first
    # send() fails and the server closes before a second one.
    # 큰 파일은 send()를 여러 번 불러야 한다. 클라이언트는 버퍼를 비우고
    # FIN으로 닫아 다음 순서를 노린다: 서버의 다음 send()는 성공하고,
    # 클라이언트가 RST로 답하고, 그다음 send()가 SIGPIPE를 일으킨다.
    # SIGPIPE는 무시하지 않으면 프로세스를 끝낸다 (위키
    # raw/macos-observed-sigpipe-leaks.md:114-117). macOS와 Ubuntu에서는
    # 서버가 RST의 POLLERR/POLLHUP을 먼저 보고 그 send() 없이 닫았다
    # (테스트 계획 N-07). 그래서 여기서는 서버가 살아남는지만 검사하고,
    # SIGPIPE 무시는 signals_test.cpp가 검사한다. 읽지 않은 데이터가 있는
    # 채로 닫으면 바로 RST가 가서, 첫 send()가 실패하고 서버는 두 번째
    # send() 전에 닫는다.
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
        # recv()가 0을 돌려주면 클라이언트 소켓을 닫아야 한다. 아니면
        # 디스크립터가 헤더 시간 제한까지 열려 있다.
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
        # 보낼 것이 없는데 POLLOUT을 요청하면 poll()이 매번 바로 돌아오고
        # 루프가 CPU를 태운다.
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
        # 몇 초마다 한 바이트씩 와도 헤더 시간 제한이 늘어나면 안 된다.
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
        """Waits until the server has expected descriptors open; fails the
        test after FD_SETTLE_SECONDS.
        서버가 연 디스크립터 수가 expected가 될 때까지 기다린다.
        FD_SETTLE_SECONDS가 지나면 테스트가 실패한다.
        """
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


# Not a ConnectionTest: the server gets a small descriptor limit.
# ConnectionTest가 아니다: 서버에 작은 디스크립터 상한을 준다.
class DescriptorLimitTest(unittest.TestCase):
    def setUp(self):
        self.workspace = harness.Workspace()
        self.addCleanup(self.workspace.cleanup)
        self.port = harness.free_port()
        path = self.workspace.write("test.conf",
                                    harness.server_block(self.port))
        self.server = harness.RunningServer([path], self.workspace.path,
                                            [self.port],
                                            fd_limit=SMALL_FD_LIMIT)
        self.addCleanup(self.server.stop)

    # Once accept() fails for lack of descriptors, the listen socket stays
    # readable; polling it again at once would spin the loop.
    # 디스크립터가 모자라 accept()가 실패하면 listen 소켓은 계속 읽기 가능
    # 상태다. 바로 다시 poll하면 루프가 헛돈다.
    @unittest.skipUnless(os.path.isdir("/proc/self/fd"),
                         "needs /proc (run on Linux)")
    @unittest.skipIf(WRAPPER, "the wrapper needs descriptors of its own")
    def test_n26_full_descriptor_table_does_not_spin(self):
        clients = [harness.connect(self.port)
                   for _ in range(SMALL_FD_LIMIT * 2)]
        time.sleep(FD_SETTLE_SECONDS)
        before = self.server.cpu_seconds()
        time.sleep(IDLE_SECONDS)
        used = self.server.cpu_seconds() - before
        for sock in clients:
            sock.close()
        self.assertLess(used, IDLE_SECONDS * MAX_IDLE_CPU_SHARE)
        with harness.connect(self.port) as sock:
            sock.sendall(SIMPLE_GET)
            response = harness.read_until_closed(sock)
        self.assertIn("200", harness.status_line(response))


if __name__ == "__main__":
    unittest.main()
