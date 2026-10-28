"""Access and error logs (test plan L-xx).
접근 로그와 오류 로그 (테스트 계획 L-xx).
"""

import os
import signal
import sys
import unittest

import harness
from harness import request
from constants import (
    ACCESS_LOG,
    DEFAULT_PAGE,
    ERROR_LOG,
    EXIT_FAILURE,
    EXIT_TIMEOUT_SECONDS,
    HOST,
    LOG_DIR,
    RUN_SLOW,
    TIMEOUT_MARGIN_SECONDS,
)

CGI_TIMEOUT_SECONDS = 10  # limits::kCgiTimeoutSeconds

SCRIPTS = {
    # No blank line after the header: not a CGI response.
    # 헤더 뒤에 빈 줄이 없다: CGI 응답이 아니다.
    "garbage.py": 'print("no header here")\n',
    "loop.py": "while True:\n    pass\n",
}


def get(path):
    """A GET request for path.
    path에 대한 GET 요청.
    """
    return request(["GET %s HTTP/1.1" % path, "Host: x"])


class LogTest(unittest.TestCase):
    def setUp(self):
        self.workspace = harness.Workspace()
        self.addCleanup(self.workspace.cleanup)
        for name, text in SCRIPTS.items():
            self.workspace.write("www/cgi/" + name, text)
        self.port = harness.free_port()
        config = ("server {\n"
                  "    listen %s:%d;\n"
                  "    root www;\n"
                  "    location /cgi/ { cgi .py %s; }\n"
                  "}\n" % (HOST, self.port, sys.executable))
        self.config = self.workspace.write("test.conf", config)
        self.server = harness.RunningServer([self.config],
                                            self.workspace.path, [self.port])
        self.addCleanup(self.server.stop)

    def status_of(self, raw):
        """The status code of the response to raw.
        raw에 대한 응답의 상태 코드.
        """
        return harness.parse(harness.exchange(self.port, raw))[0]

    # One line per response, with the request line as the client sent it.
    # HEAD sends no body, so its size is 0.
    # 응답마다 한 줄이고, 요청 줄은 클라이언트가 보낸 그대로다. HEAD는
    # 본문을 보내지 않으므로 크기가 0이다.
    def test_l01_access_line_per_response(self):
        self.assertEqual(200, self.status_of(get("/")))
        self.assertEqual(404, self.status_of(get("/missing?q=1")))
        self.assertEqual(200, self.status_of(
            request(["HEAD / HTTP/1.1", "Host: x"])))
        lines = self.workspace.log_lines(ACCESS_LOG)
        self.assertEqual(3, len(lines), lines)
        self.assertEqual(
            '127.0.0.1 "GET / HTTP/1.1" 200 %d' % len(DEFAULT_PAGE), lines[0])
        self.assertTrue(lines[1].startswith(
            '127.0.0.1 "GET /missing?q=1 HTTP/1.1" 404 '), lines[1])
        self.assertEqual('127.0.0.1 "HEAD / HTTP/1.1" 200 0', lines[2])

    # A request cut short shows "-" for the parts that never arrived.
    # 중간에 끊긴 요청은 오지 않은 부분을 "-"로 보인다.
    def test_l01_bad_request_is_logged(self):
        self.assertEqual(400, self.status_of(b"\x01\r\n\r\n"))
        line = self.workspace.log_lines(ACCESS_LOG)[-1]
        self.assertTrue(line.startswith('127.0.0.1 "- - -" 400 '), line)

    # SIGINT, not the harness's kill, so that the server shuts down itself.
    # 서버가 스스로 끝나도록 하네스의 kill 대신 SIGINT를 보낸다.
    def test_l02_start_and_stop_are_logged(self):
        self.server.process.send_signal(signal.SIGINT)
        self.server.process.wait(timeout=EXIT_TIMEOUT_SECONDS)
        self.assertEqual(["started: " + self.config, "shutting down"],
                         self.workspace.log_lines(ERROR_LOG))

    def test_l03_invalid_cgi_response_is_logged(self):
        self.assertEqual(502, self.status_of(get("/cgi/garbage.py")))
        self.assertIn("CGI response invalid: /cgi/garbage.py (502)",
                      self.workspace.log_lines(ERROR_LOG))

    @unittest.skipUnless(RUN_SLOW, "set WEBSERV_SLOW_TESTS=1 (takes ~10s)")
    def test_l03_cgi_timeout_is_logged(self):
        with harness.connect(self.port) as sock, \
                sock.makefile("rb") as stream:
            sock.settimeout(CGI_TIMEOUT_SECONDS + TIMEOUT_MARGIN_SECONDS)
            sock.sendall(get("/cgi/loop.py"))
            status = harness.parse(harness.read_response(stream))[0]
        self.assertEqual(504, status)
        self.assertIn("CGI timed out and was killed: /cgi/loop.py (504)",
                      self.workspace.log_lines(ERROR_LOG))


class MissingLogDirectoryTest(unittest.TestCase):
    def test_l04_missing_log_directory_stops_startup(self):
        workspace = harness.Workspace()
        self.addCleanup(workspace.cleanup)
        os.rmdir(os.path.join(workspace.path, LOG_DIR))
        path = workspace.write("test.conf",
                               harness.server_block(harness.free_port()))
        code, stderr = harness.run_to_exit([path], workspace.path)
        self.assertEqual(EXIT_FAILURE, code)
        self.assertIn("cannot open log files", stderr)


if __name__ == "__main__":
    unittest.main()
