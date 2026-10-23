"""The demonstration files of the repository (test plan E-xx, step 9).
저장소의 시연 파일 (테스트 계획 E-xx, 9단계).

conf/*.conf is used as written, with only the ports replaced by free
ones, from the repository root, so these tests check what the evaluation
will see.
conf/*.conf를 포트만 빈 포트로 바꾸고 쓴 그대로, 저장소 루트에서 쓴다.
그래서 이 테스트는 평가에서 보게 될 것을 검사한다.
"""

import os
import re
import socket
import sys
import unittest

import harness
from harness import get, request
from constants import (
    REPO_ROOT,
    RUN_SLOW,
    SIMPLE_GET,
    SOCKET_TIMEOUT_SECONDS,
    TIMEOUT_MARGIN_SECONDS,
)

DEMO_PORTS = ["127.0.0.1:8080", "127.0.0.1:8081"]  # in conf/default.conf
PHP_CGI = "/usr/bin/php-cgi"
PYTHON_CGI = "/usr/bin/python3"
BODY_LIMIT = 64  # client_max_body_size of /limit/ in conf/default.conf
UPLOADED = "e05-demo-upload.txt"
CGI_TIMEOUT_SECONDS = 10  # limits::kCgiTimeoutSeconds


def conf_text(name):
    """The text of conf/name.
    conf/name의 텍스트.
    """
    with open(os.path.join(REPO_ROOT, "conf", name)) as conf:
        return conf.read()


def post(path, body, content_type="application/x-www-form-urlencoded"):
    """A POST request with a Content-Type and the body.
    Content-Type과 본문이 있는 POST 요청.
    """
    return request(["POST %s HTTP/1.1" % path, "Host: x",
                    "Content-Type: " + content_type,
                    "Content-Length: %d" % len(body)], body)


@unittest.skipUnless(os.path.exists(PYTHON_CGI), "needs " + PYTHON_CGI)
class DemoTest(unittest.TestCase):
    def setUp(self):
        self.workspace = harness.Workspace()
        self.addCleanup(self.workspace.cleanup)
        self.ports = [harness.free_port() for _ in DEMO_PORTS]
        text = conf_text("default.conf")
        for address, port in zip(DEMO_PORTS, self.ports):
            self.assertIn(address, text)
            text = text.replace(address, "127.0.0.1:%d" % port)
        path = self.workspace.write("demo.conf", text)
        self.server = harness.RunningServer([path], REPO_ROOT, self.ports)
        self.addCleanup(self.server.stop)

    def fetch(self, raw, port=None):
        """Sends raw and returns the parsed response.
        raw를 보내고 해석한 응답을 돌려준다.
        """
        return harness.parse(harness.exchange(port or self.ports[0], raw))

    def test_e02_static_site(self):
        status, headers, body = self.fetch(get("/"))
        self.assertEqual(200, status)
        self.assertIn(b"webserv demo", body)
        self.assertEqual("text/css", self.fetch(get("/style.css"))[1]
                         ["content-type"])
        self.assertEqual("image/svg+xml", self.fetch(get("/logo.svg"))[1]
                         ["content-type"])
        response = harness.exchange(self.ports[0], get("/", "HEAD"))
        self.assertEqual(200, harness.parse(response)[0])

    def test_e03_configuration_features(self):
        status, _, body = self.fetch(get("/files/"))
        self.assertEqual(200, status)
        self.assertIn(b"a.txt", body)
        self.assertIn(b"notes/", body)
        status, _, body = self.fetch(get("/docs/"))
        self.assertEqual(200, status)
        self.assertIn(b"Documents", body)
        status, headers, _ = self.fetch(get("/old/"))
        self.assertEqual((301, "/about.html"), (status, headers["location"]))
        status, _, body = self.fetch(get("/no/such/page"))
        self.assertEqual(404, status)
        self.assertIn(b"error_page 404", body)
        status, headers, _ = self.fetch(get("/index.html", "DELETE"))
        self.assertEqual((405, "GET, HEAD"), (status, headers["allow"]))
        status, _, body = self.fetch(get("/"), self.ports[1])
        self.assertEqual(200, status)
        self.assertIn(b"Second site", body)

    # The evaluation sheet's command: curl -X POST -H "Content-Type:
    # plain/text" --data "BODY IS HERE ..." with a short and a long body.
    # 평가표의 명령: curl -X POST -H "Content-Type: plain/text" --data
    # "BODY IS HERE ..."를 짧은 본문과 긴 본문으로 보낸다.
    def test_e04_body_limit(self):
        short = b"BODY IS HERE"
        status, _, body = self.fetch(post("/limit/echo.py", short,
                                          "plain/text"))
        self.assertEqual(200, status)
        self.assertIn(b"received 12 bytes: BODY IS HERE", body)
        status, _, _ = self.fetch(post("/limit/echo.py",
                                       b"x" * (BODY_LIMIT + 1), "plain/text"))
        self.assertEqual(413, status)

    def test_e05_upload_get_delete(self):
        content = b"uploaded during the demo test\n"
        stored = os.path.join(REPO_ROOT, "www/upload", UPLOADED)
        self.addCleanup(lambda: os.path.exists(stored) and os.remove(stored))
        boundary = "demoBoundary"
        body = ("--%s\r\nContent-Disposition: form-data; name=\"file\"; "
                "filename=\"%s\"\r\n\r\n" % (boundary, UPLOADED)).encode()
        body += content + ("\r\n--%s--\r\n" % boundary).encode()
        status, headers, _ = self.fetch(post(
            "/upload/", body, "multipart/form-data; boundary=" + boundary))
        self.assertEqual(201, status)
        self.assertEqual("/upload/" + UPLOADED, headers["location"])
        self.assertEqual((200, content), self.fetch(
            get("/upload/" + UPLOADED))[::2])
        self.assertEqual(204, self.fetch(get("/upload/" + UPLOADED,
                                             "DELETE"))[0])
        self.assertEqual(404, self.fetch(get("/upload/" + UPLOADED))[0])

    def test_e06_cgi(self):
        status, _, body = self.fetch(get("/cgi-bin/hello.py?name=eval"))
        self.assertEqual(200, status)
        self.assertIn(b"Hello, eval!", body)
        status, _, body = self.fetch(post("/cgi-bin/form.py",
                                          b"name=a&message=b"))
        self.assertEqual(200, status)
        self.assertIn(b"message = b", body)
        status, _, body = self.fetch(get("/cgi-bin/relative.py"))
        self.assertEqual((200, b"This text was read with a relative path.\n"),
                         (status, body))
        status, _, body = self.fetch(get("/cgi-bin/env.py/extra?x=1"))
        self.assertIn(b"PATH_INFO=/extra", body)
        self.assertEqual(502, self.fetch(get("/cgi-bin/error.py"))[0])

    @unittest.skipUnless(RUN_SLOW, "set WEBSERV_SLOW_TESTS=1 (takes ~11s)")
    def test_e06_infinite_loop(self):
        with harness.connect(self.ports[0]) as sock, \
                sock.makefile("rb") as stream:
            sock.settimeout(CGI_TIMEOUT_SECONDS + TIMEOUT_MARGIN_SECONDS)
            sock.sendall(get("/cgi-bin/loop.py"))
            status = harness.parse(harness.read_response(stream))[0]
        self.assertEqual(504, status)

    def test_e07_cookie_session(self):
        status, headers, body = self.fetch(get("/cgi-bin/session.py"))
        self.assertEqual(200, status)
        self.assertIn(b"Visit number 1", body)
        cookie = re.match(r"(webserv_session=[0-9a-f]{32});",
                          headers["set-cookie"]).group(1)
        with_cookie = request(["GET /cgi-bin/session.py HTTP/1.1", "Host: x",
                               "Cookie: " + cookie])
        status, headers, body = self.fetch(with_cookie)
        self.assertIn(b"Visit number 2", body)
        self.assertNotIn("set-cookie", headers)
        # An identifier the server never issued gets a new session.
        # 서버가 발급한 적 없는 식별자는 새 세션을 받는다.
        forged = request(["GET /cgi-bin/session.py HTTP/1.1", "Host: x",
                          "Cookie: webserv_session=" + "0" * 32])
        status, headers, body = self.fetch(forged)
        self.assertIn(b"Visit number 1", body)
        self.assertNotIn("0" * 32, headers["set-cookie"])
        logout = request(["GET /cgi-bin/session.py?logout HTTP/1.1",
                          "Host: x", "Cookie: " + cookie])
        status, headers, _ = self.fetch(logout)
        self.assertIn("Expires=Thu, 01 Jan 1970", headers["set-cookie"])

    @unittest.skipUnless(os.path.exists(PHP_CGI), "needs " + PHP_CGI)
    def test_e08_php(self):
        status, _, body = self.fetch(get("/cgi-bin/hello.php?name=eval"))
        self.assertEqual(200, status)
        self.assertIn(b"Hello from PHP, eval!", body)


class DemoConfigTest(unittest.TestCase):
    def test_e01_same_port_is_refused(self):
        code, stderr = harness.run_to_exit(["conf/same-port.conf"], REPO_ROOT)
        self.assertEqual(1, code)
        self.assertIn("duplicate listen", stderr)

    @unittest.skipUnless(sys.platform.startswith("linux"),
                         "127.0.0.2 is not bound on macOS")
    def test_e01_ports_conf_serves_each_address(self):
        text = conf_text("ports.conf")
        for address in ["127.0.0.1:8080", "127.0.0.1:8081",
                        "127.0.0.2:8080", "listen 8082"]:
            self.assertIn(address, text)
        workspace = harness.Workspace()
        self.addCleanup(workspace.cleanup)
        ports = [harness.free_port() for _ in range(3)]
        text = (text.replace("127.0.0.1:8080", "127.0.0.1:%d" % ports[0])
                .replace("127.0.0.1:8081", "127.0.0.1:%d" % ports[1])
                .replace("127.0.0.2:8080", "127.0.0.2:%d" % ports[0])
                .replace("listen 8082", "listen %d" % ports[2]))
        path = workspace.write("ports.conf", text)
        server = harness.RunningServer([path], REPO_ROOT, ports)
        self.addCleanup(server.stop)
        for host, port, marker in [("127.0.0.1", ports[0], b"webserv demo"),
                                   ("127.0.0.1", ports[1], b"Second site"),
                                   ("127.0.0.2", ports[0], b"Second site"),
                                   ("127.0.0.1", ports[2], b"webserv demo")]:
            with socket.create_connection((host, port),
                                          timeout=SOCKET_TIMEOUT_SECONDS) as sock:
                sock.sendall(SIMPLE_GET)
                body = harness.read_until_closed(sock)
            self.assertIn(marker, body, (host, port))


if __name__ == "__main__":
    unittest.main()
