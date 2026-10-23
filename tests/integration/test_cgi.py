"""CGI (test plan G-xx, step 7).
CGI (테스트 계획 G-xx, 7단계).
"""

import os
import signal
import socket
import struct
import sys
import time
import unittest

import harness
from harness import get, request
from constants import HOST, TIMEOUT_MARGIN_SECONDS, WRAPPER

CGI_TIMEOUT_SECONDS = 10  # limits::kCgiTimeoutSeconds
MAX_CGI_OUTPUT = 16 * 1024 * 1024  # limits::kMaxCgiOutputBytes
MAX_INTERNAL_REDIRECTS = 10  # limits::kMaxInternalRedirects
PHP_CGI = "/usr/bin/php-cgi"
FAST_SECONDS = 2  # an answer that does not wait for a running script
REAP_SECONDS = 3  # time for the server to collect finished children
MAX_OUTPUT_COPIES = 2.5  # peak memory per byte of CGI output

SCRIPTS = {
    # Every meta-variable, the working directory and the body.
    # 모든 메타 변수, 작업 디렉토리, 본문.
    "env.py": r'''
import os, sys
body = sys.stdin.buffer.read()
out = ["Content-Type: text/plain", ""]
out += ["%s=%s" % kv for kv in sorted(os.environ.items())]
out += ["CWD=" + os.getcwd(), "BODY=" + body.decode("latin-1")]
sys.stdout.write("\n".join(out))
''',
    "echo.py": r'''
import os, sys
body = sys.stdin.buffer.read(int(os.environ.get("CONTENT_LENGTH", "0")))
sys.stdout.buffer.write(b"Content-Type: application/octet-stream\r\n\r\n")
sys.stdout.buffer.write(body)
''',
    "relative.py": r'''
print("Content-Type: text/plain\n")
print(open("data.txt").read(), end="")
''',
    "loop.py": "while True:\n    pass\n",
    "crash.py": "import sys\nsys.exit(1)\n",
    "garbage.py": 'print("this is not a header")\nprint()\n',
    "status.py": r'''
print("Status: 418 I'm a teapot")
print("Content-Type: text/plain")
print("Set-Cookie: a=1")
print("Set-Cookie: b=2")
print("Content-Length: 1")
print("Date: Thu, 01 Jan 1970 00:00:00 GMT")
print()
print("short and stout")
''',
    "local.py": 'print("Location: /cgi/env.py?from=local")\nprint()\n',
    "client.py": 'print("Location: http://example.com/x")\nprint()\n',
    "self.py": 'print("Location: /cgi/self.py")\nprint()\n',
    "big.py": r'''
import sys
sys.stdout.write("Content-Type: text/plain\n\n")
sys.stdout.write("x" * 300000)
''',
    "fifteen.py": r'''
import sys
sys.stdout.write("Content-Type: text/plain\n\n")
sys.stdout.write("f" * %d)
''' % (15 * 1024 * 1024),
    # A client redirect with a document (RFC 3875 section 6.2.4).
    # 문서가 있는 클라이언트 리다이렉트 (RFC 3875 6.2.4절).
    "fifteen_redirect.py": r'''
import sys
sys.stdout.write("Status: 302 Found\n")
sys.stdout.write("Location: http://example.com/x\n")
sys.stdout.write("Content-Type: text/plain\n\n")
sys.stdout.write("r" * %d)
''' % (15 * 1024 * 1024),
    "huge.py": r'''
import sys
sys.stdout.write("Content-Type: text/plain\n\n")
chunk = "y" * 65536
for _ in range(%d):
    sys.stdout.write(chunk)
''' % (MAX_CGI_OUTPUT // 65536 + 2),
    "noread.py": 'print("Content-Type: text/plain\\n")\nprint("ignored")\n',
    "sleep.py": "import time\ntime.sleep(20)\n",
    # Answers, closes its output, then keeps running.
    # 답하고, 출력을 닫고, 계속 실행된다.
    "linger.py": r'''
import os, sys, time
sys.stdout.write("Content-Type: text/plain\n\ndone early\n")
sys.stdout.flush()
os.close(1)
time.sleep(20)
''',
    "index.py": 'print("Content-Type: text/plain\\n")\nprint("index script")\n',
    # Descriptors open in the script; listdir() opens one more itself.
    # 스크립트에서 열린 디스크립터. listdir()이 스스로 하나 더 연다.
    "fds.py": r'''
import os
fds = sorted(int(n) for n in os.listdir("/dev/fd"))
print("Content-Type: text/plain\n")
print(" ".join(str(fd) for fd in fds))
''',
}


def cgi_env(body):
    """{name: value} from the output of env.py.
    env.py 출력에서 얻은 {이름: 값}.
    """
    lines = body.decode("latin-1").split("\n")
    return dict(line.split("=", 1) for line in lines if "=" in line)


def post(path, body, extra=()):
    """A POST request with the body and extra header lines.
    본문과 덧붙인 헤더 줄이 있는 POST 요청.
    """
    return request(["POST %s HTTP/1.1" % path, "Host: x",
                    "Content-Length: %d" % len(body)] + list(extra), body)


class CgiTest(unittest.TestCase):
    def setUp(self):
        self.workspace = harness.Workspace()
        self.addCleanup(self.workspace.cleanup)
        w = self.workspace
        for name, text in SCRIPTS.items():
            w.write("www/cgi/" + name, text)
        w.write("www/cgi/data.txt", "relative file\n")
        w.write("www/plain/x.py", "print('never run')\n")
        w.write("www/ro/index.py", SCRIPTS["index.py"])
        php = ""
        if os.path.exists(PHP_CGI):
            w.write("www/cgi/hello.php",
                    '<?php header("X-From: php"); echo "php ",'
                    ' $_SERVER["REQUEST_METHOD"], " ",'
                    ' isset($_GET["q"]) ? $_GET["q"] : "",'
                    ' file_get_contents("php://input"); ?>')
            php = "        cgi .php %s;\n" % PHP_CGI
        self.port = harness.free_port()
        config = ("server {\n"
                  "    listen %s:%d;\n"
                  "    root www;\n"
                  "    location /cgi/ {\n"
                  "        limit_except GET POST DELETE { deny all; }\n"
                  "        index index.py;\n"
                  "        client_max_body_size 10m;\n"
                  "        cgi .py %s;\n%s"
                  "    }\n"
                  "    location /ro/ {\n"
                  "        limit_except GET { deny all; }\n"
                  "        cgi .py %s;\n"
                  "    }\n"
                  "}\n" % (HOST, self.port, sys.executable, php,
                            sys.executable))
        path = w.write("test.conf", config)
        self.server = harness.RunningServer([path], w.path, [self.port])
        self.addCleanup(self.server.stop)

    def fetch(self, raw):
        """Sends raw and returns the parsed response.
        raw를 보내고 해석한 응답을 돌려준다.
        """
        return harness.parse(harness.exchange(self.port, raw))

    def test_g01_meta_variables(self):
        status, _, body = self.fetch(request(
            ["GET /cgi/env.py/a/b%20c?x=1&y=%20 HTTP/1.1", "Host: h:1",
             "X-Twice: 1", "x-twice: 2", "Authorization: secret"]))
        self.assertEqual(200, status)
        env = cgi_env(body)
        expected = {
            "GATEWAY_INTERFACE": "CGI/1.1", "REQUEST_METHOD": "GET",
            "QUERY_STRING": "x=1&y=%20", "SCRIPT_NAME": "/cgi/env.py",
            "PATH_INFO": "/a/b c", "SERVER_PROTOCOL": "HTTP/1.1",
            "SERVER_PORT": str(self.port), "SERVER_NAME": HOST,
            "REMOTE_ADDR": HOST, "SCRIPT_FILENAME": "env.py",
            "REDIRECT_STATUS": "200", "HTTP_HOST": "h:1",
            "HTTP_X_TWICE": "1, 2",
        }
        for name, value in expected.items():
            self.assertEqual(value, env.get(name), name)
        self.assertNotIn("HTTP_AUTHORIZATION", env)
        self.assertNotIn("CONTENT_LENGTH", env)

    def test_g02_post_body_reaches_stdin(self):
        status, _, body = self.fetch(post(
            "/cgi/env.py", b"a=1&b=2",
            ["Content-Type: application/x-www-form-urlencoded"]))
        self.assertEqual(200, status)
        env = cgi_env(body)
        self.assertEqual("POST", env["REQUEST_METHOD"])
        self.assertEqual("7", env["CONTENT_LENGTH"])
        self.assertEqual("application/x-www-form-urlencoded",
                         env["CONTENT_TYPE"])
        self.assertEqual("a=1&b=2", env["BODY"])

    def test_g03_chunked_body_is_decoded(self):
        status, _, body = self.fetch(request(
            ["POST /cgi/env.py HTTP/1.1", "Host: x",
             "Transfer-Encoding: chunked"],
            b"3\r\nabc\r\n4\r\ndefg\r\n0\r\n\r\n"))
        self.assertEqual(200, status)
        env = cgi_env(body)
        self.assertEqual("7", env["CONTENT_LENGTH"])
        self.assertEqual("abcdefg", env["BODY"])
        self.assertNotIn("HTTP_TRANSFER_ENCODING", env)

    def test_g04_script_runs_in_its_directory(self):
        status, _, body = self.fetch(get("/cgi/relative.py"))
        self.assertEqual(200, status)
        self.assertEqual(b"relative file\n", body)

    # The loop must not stop the server: another request is answered
    # while the script runs, and the script gets 504 after the timeout.
    # 루프가 서버를 멈추면 안 된다. 스크립트가 도는 동안 다른 요청에 답하고,
    # 스크립트는 시간 제한 뒤 504를 받는다.
    def test_g05_infinite_loop_is_504_and_does_not_block(self):
        with harness.connect(self.port) as looping, \
                looping.makefile("rb") as stream:
            looping.settimeout(CGI_TIMEOUT_SECONDS + TIMEOUT_MARGIN_SECONDS)
            started = time.monotonic()
            looping.sendall(get("/cgi/loop.py"))
            time.sleep(0.5)
            other = time.monotonic()
            self.assertEqual(200, self.fetch(get("/"))[0])
            self.assertLess(time.monotonic() - other, FAST_SECONDS)
            status = harness.parse(harness.read_response(stream))[0]
            waited = time.monotonic() - started
        self.assertEqual(504, status)
        self.assertGreaterEqual(waited, CGI_TIMEOUT_SECONDS)

    def test_g06_failing_scripts_are_502(self):
        for path in ["/cgi/crash.py", "/cgi/garbage.py"]:
            self.assertEqual(502, self.fetch(get(path))[0], path)
        self.assertEqual(200, self.fetch(get("/"))[0])

    def test_g07_status_and_fields_from_the_script(self):
        status, headers, body = self.fetch(get("/cgi/status.py"))
        raw = harness.exchange(self.port, get("/cgi/status.py"))
        self.assertEqual(418, status)
        self.assertEqual(b"short and stout\n", body)
        self.assertEqual(str(len(body)), headers["content-length"])
        self.assertEqual(2, raw.count(b"Set-Cookie: "))
        self.assertEqual(1, raw.count(b"Content-Length: "))
        self.assertEqual(1, raw.count(b"Date: "))
        self.assertNotIn(b"1970", raw)

    def test_g08_redirects(self):
        status, _, body = self.fetch(get("/cgi/local.py"))
        self.assertEqual(200, status)
        env = cgi_env(body)
        self.assertEqual("from=local", env["QUERY_STRING"])
        self.assertEqual("GET", env["REQUEST_METHOD"])
        status, headers, _ = self.fetch(get("/cgi/client.py"))
        self.assertEqual(302, status)
        self.assertEqual("http://example.com/x", headers["location"])
        # A script redirecting to itself stops at the redirect limit.
        # 자기 자신으로 리다이렉트하는 스크립트는 리다이렉트 상한에서 멈춘다.
        self.assertEqual(500, self.fetch(get("/cgi/self.py"))[0])

    def test_g09_index_script_runs(self):
        status, _, body = self.fetch(get("/cgi/"))
        self.assertEqual(200, status)
        self.assertEqual(b"index script\n", body)

    def test_g10_methods(self):
        status, _, body = self.fetch(get("/cgi/env.py", "DELETE"))
        self.assertEqual(200, status)
        self.assertEqual("DELETE", cgi_env(body)["REQUEST_METHOD"])
        self.assertTrue(os.path.exists(
            os.path.join(self.workspace.path, "www/cgi/env.py")))
        # limit_except applies to scripts as to files.
        # limit_except는 파일처럼 스크립트에도 적용된다.
        status, headers, _ = self.fetch(post("/ro/index.py", b"x"))
        self.assertEqual(405, status)
        self.assertEqual("GET, HEAD", headers["allow"])
        self.assertEqual(200, self.fetch(get("/ro/index.py"))[0])
        # No "cgi" in this location: the file is served, never run.
        # 이 location에는 "cgi"가 없어서, 파일을 보내기만 하고 실행하지 않는다.
        status, _, body = self.fetch(get("/plain/x.py"))
        self.assertEqual(200, status)
        self.assertEqual(b"print('never run')\n", body)

    def test_g11_head_gets_no_body(self):
        response = harness.exchange(self.port, get("/cgi/big.py", "HEAD"))
        _, headers, body = harness.parse(response)
        self.assertEqual("300000", headers["content-length"])
        self.assertEqual(b"", body)

    def test_g12_large_input_and_output(self):
        payload = os.urandom(3 * 1024 * 1024)
        status, _, body = self.fetch(post("/cgi/echo.py", payload))
        self.assertEqual(200, status)
        self.assertEqual(payload, body)
        status, _, body = self.fetch(get("/cgi/big.py"))
        self.assertEqual(300000, len(body))
        self.assertEqual(502, self.fetch(get("/cgi/huge.py"))[0])

    def test_g13_unread_body_is_fine(self):
        status, _, body = self.fetch(post("/cgi/noread.py",
                                          b"z" * (512 * 1024)))
        self.assertEqual(200, status)
        self.assertEqual(b"ignored\n", body)

    def test_g14_connection_stays_open_after_cgi(self):
        with harness.connect(self.port) as sock, sock.makefile("rb") as stream:
            for _ in range(2):
                sock.sendall(get("/cgi/index.py"))
                status, headers, _ = harness.parse(
                    harness.read_response(stream))
                self.assertEqual(200, status)
                self.assertNotIn("connection", headers)

    @unittest.skipUnless(os.path.isdir("/proc/self"),
                         "needs /proc (run on Linux)")
    def test_g15_children_are_collected(self):
        for path in ["/cgi/index.py", "/cgi/crash.py", "/cgi/noread.py"]:
            self.fetch(get(path))
        time.sleep(REAP_SECONDS)
        self.assertEqual([], self.server.children())

    # A client that resets the connection while its script runs: the
    # script is killed and collected instead of running on.
    # 스크립트가 도는 중에 연결을 리셋하는 클라이언트: 스크립트는 계속 돌지
    # 않고 죽고 회수된다.
    @unittest.skipUnless(os.path.isdir("/proc/self"),
                         "needs /proc (run on Linux)")
    def test_g16_reset_client_kills_its_script(self):
        sock = harness.connect(self.port)
        sock.sendall(get("/cgi/sleep.py"))
        time.sleep(0.5)
        self.assertEqual(1, len(self.server.children()))
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER,
                        struct.pack("ii", 1, 0))
        sock.close()
        time.sleep(REAP_SECONDS)
        self.assertEqual([], self.server.children())

    def test_g19_script_inherits_no_other_descriptor(self):
        with harness.connect(self.port) as idle:  # another client's socket
            idle.sendall(b"GET")
            status, _, body = self.fetch(get("/cgi/fds.py"))
        self.assertEqual(200, status)
        fds = [int(fd) for fd in body.split()]
        # 0, 1, 2 and the one listdir() used.
        # 0, 1, 2와 listdir()이 쓴 것 하나.
        self.assertEqual([0, 1, 2], fds[:3])
        self.assertLessEqual(len(fds), 4, fds)

    @unittest.skipUnless(os.path.isdir("/proc/self"),
                         "needs /proc (run on Linux)")
    def test_g20_sigint_ends_running_scripts(self):
        sock = harness.connect(self.port)
        self.addCleanup(sock.close)
        sock.sendall(get("/cgi/loop.py"))
        time.sleep(0.5)
        script_pids = self.server.child_pids()
        self.assertEqual(1, len(script_pids))
        self.assertEqual(0, self.server.send_signal(signal.SIGINT))
        for pid in script_pids:
            self.assertFalse(os.path.exists("/proc/%d" % pid), pid)

    # Output ended but the script runs on: the server only hands it to the
    # reaper, which must still end it when the server stops.
    # 출력은 끝났지만 스크립트는 계속 돈다. 서버는 이를 reaper에 넘기기만
    # 하고, reaper는 서버가 멈출 때 그래도 끝내야 한다.
    @unittest.skipUnless(os.path.isdir("/proc/self"),
                         "needs /proc (run on Linux)")
    def test_g20_sigint_ends_scripts_that_outlive_their_output(self):
        status, _, body = self.fetch(get("/cgi/linger.py"))
        self.assertEqual((200, b"done early\n"), (status, body))
        script_pids = self.server.child_pids()
        self.assertEqual(1, len(script_pids))
        self.assertEqual(0, self.server.send_signal(signal.SIGINT))
        for pid in script_pids:
            self.assertFalse(os.path.exists("/proc/%d" % pid), pid)

    # The output used to be copied about four times on its way to the
    # response; now it is moved, and only the send buffer copies it.
    # 출력은 응답이 되기까지 네 번쯤 복사됐다. 지금은 옮기기만 하고, 전송
    # 버퍼만 복사한다.
    @unittest.skipUnless(os.path.isdir("/proc/self"),
                         "needs /proc (run on Linux)")
    @unittest.skipIf(WRAPPER, "the wrapper's memory counts as the server's")
    def test_g22_large_output_is_not_copied_over_and_over(self):
        before = self.server.peak_memory_kb()
        status, _, body = self.fetch(get("/cgi/fifteen.py"))
        self.assertEqual((200, 15 * 1024 * 1024), (status, len(body)))
        grown = (self.server.peak_memory_kb() - before) * 1024
        self.assertLess(grown, len(body) * MAX_OUTPUT_COPIES)

    # The same for a client redirect with a document, which the parser
    # handles on another path.
    # 문서가 있는 클라이언트 리다이렉트도 같다. 파서는 이를 다른 경로로
    # 처리한다.
    @unittest.skipUnless(os.path.isdir("/proc/self"),
                         "needs /proc (run on Linux)")
    @unittest.skipIf(WRAPPER, "the wrapper's memory counts as the server's")
    def test_g22_large_redirect_document_is_not_copied(self):
        before = self.server.peak_memory_kb()
        status, headers, body = self.fetch(get("/cgi/fifteen_redirect.py"))
        self.assertEqual((302, 15 * 1024 * 1024), (status, len(body)))
        self.assertEqual("http://example.com/x", headers["location"])
        grown = (self.server.peak_memory_kb() - before) * 1024
        self.assertLess(grown, len(body) * MAX_OUTPUT_COPIES)

    @unittest.skipUnless(os.path.exists(PHP_CGI), "needs " + PHP_CGI)
    def test_g17_php_cgi(self):
        status, headers, body = self.fetch(get("/cgi/hello.php?q=hi"))
        self.assertEqual(200, status)
        self.assertEqual("php", headers["x-from"])
        self.assertEqual(b"php GET hi", body)
        status, _, body = self.fetch(post("/cgi/hello.php", b"data"))
        self.assertEqual(200, status)
        self.assertEqual(b"php POST data", body)


if __name__ == "__main__":
    unittest.main()
