"""Start webserv with a generated configuration and talk to it over TCP.
만든 설정으로 webserv를 띄우고 TCP로 주고받는다.

Only the Python standard library is used, so the tests run on the
evaluation machine (Ubuntu 22.04, Python 3.10) without installing anything.
Python 표준 라이브러리만 쓰므로, 평가 장비(Ubuntu 22.04, Python 3.10)에서
아무것도 설치하지 않고 테스트가 돈다.
"""

import os
import resource
import signal
import socket
import subprocess
import tempfile
import time
from constants import (
    DEFAULT_PAGE,
    EXIT_TIMEOUT_SECONDS,
    HOST,
    RECV_SIZE,
    RETRY_INTERVAL_SECONDS,
    SIMPLE_GET,
    SOCKET_TIMEOUT_SECONDS,
    STARTUP_TIMEOUT_SECONDS,
    WEBSERV,
    WRAPPER,
)


def free_port():
    """A port that was free a moment ago (the kernel picks it).
    조금 전에 비어 있던 포트 (커널이 고른다).
    """
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as probe:
        probe.bind((HOST, 0))
        return probe.getsockname()[1]


def server_block(*ports):
    """A server block listening on the ports, with "root www".
    포트들에서 listen하고 "root www"를 쓰는 server 블록.
    """
    listens = "".join("    listen %s:%d;\n" % (HOST, port) for port in ports)
    return "server {\n%s    root www;\n}\n" % listens


class Workspace:
    """A temporary directory used as the server's working directory, with
    www/index.html so that "root www" serves "/".
    서버의 작업 디렉토리로 쓰는 임시 디렉토리. "root www"가 "/"를 보내도록
    www/index.html이 들어 있다.
    """

    def __init__(self):
        """Creates the directory and www/index.html.
        디렉토리와 www/index.html을 만든다.
        """
        self._dir = tempfile.TemporaryDirectory(prefix="webserv-test-")
        self.path = self._dir.name
        self.write("www/index.html", DEFAULT_PAGE)

    def write(self, relative_path, text):
        """Writes a file under the workspace, making its directories;
        returns the full path.
        작업 공간 아래에 파일을 쓰고, 필요한 디렉토리를 만든다. 전체 경로를
        돌려준다.
        """
        full_path = os.path.join(self.path, relative_path)
        os.makedirs(os.path.dirname(full_path), exist_ok=True)
        with open(full_path, "w") as out:
            out.write(text)
        return full_path

    def cleanup(self):
        """Removes the directory.
        디렉토리를 지운다.
        """
        self._dir.cleanup()


def run_to_exit(args, cwd):
    """Runs webserv expected to stop by itself; returns (code, stderr).
    스스로 끝날 webserv를 실행하고 (종료 코드, stderr)를 돌려준다.
    """
    finished = subprocess.run(WRAPPER + [WEBSERV] + args, cwd=cwd,
                              capture_output=True,
                              text=True, timeout=EXIT_TIMEOUT_SECONDS)
    return finished.returncode, finished.stderr


class RunningServer:
    """A webserv process that is accepting connections on its ports.
    자기 포트에서 연결을 받고 있는 webserv 프로세스.
    """

    def __init__(self, args, cwd, ports, fd_limit=None):
        """Starts the server and waits until every port accepts.
        fd_limit, if given, is the server's RLIMIT_NOFILE.
        서버를 띄우고 모든 포트가 연결을 받을 때까지 기다린다.
        fd_limit을 주면 서버의 RLIMIT_NOFILE이 된다.
        """
        self.ports = ports
        limit = None
        if fd_limit is not None:
            def limit():
                resource.setrlimit(resource.RLIMIT_NOFILE,
                                   (fd_limit, fd_limit))
        self.process = subprocess.Popen(WRAPPER + [WEBSERV] + args, cwd=cwd,
                                        preexec_fn=limit,
                                        stdout=subprocess.DEVNULL,
                                        stderr=subprocess.PIPE, text=True)
        self._wait_until_listening()

    def _wait_until_listening(self):
        """Raises RuntimeError if the server exits or does not listen in
        time.
        서버가 끝나거나 제시간에 listen하지 않으면 RuntimeError를 던진다.
        """
        deadline = time.monotonic() + STARTUP_TIMEOUT_SECONDS
        for port in self.ports:
            while not can_connect(port):
                if self.process.poll() is not None:
                    raise RuntimeError("webserv exited at startup: "
                                       + self.process.stderr.read())
                if time.monotonic() > deadline:
                    self.stop()
                    raise RuntimeError("webserv did not listen on %d" % port)
                time.sleep(RETRY_INTERVAL_SECONDS)

    def open_fd_count(self):
        """Open descriptors of the server, or None where /proc is missing.
        서버가 연 디스크립터 수. /proc이 없으면 None.
        """
        fd_dir = "/proc/%d/fd" % self.process.pid
        if not os.path.isdir(fd_dir):
            return None
        return len(os.listdir(fd_dir))

    def cpu_seconds(self):
        """User + system CPU time of the server, or None without /proc.
        서버의 user + system CPU 시간. /proc이 없으면 None.
        """
        stat_path = "/proc/%d/stat" % self.process.pid
        if not os.path.exists(stat_path):
            return None
        with open(stat_path) as stat:
            # Fields after the command name, which is in parentheses.
            # 괄호 안에 있는 명령 이름 뒤의 필드.
            fields = stat.read().rsplit(")", 1)[1].split()
        user_ticks, system_ticks = int(fields[11]), int(fields[12])
        return (user_ticks + system_ticks) / os.sysconf("SC_CLK_TCK")

    def _child_stats(self):
        """(pid, state) of the server's children; needs /proc.
        서버 자식 프로세스의 (pid, 상태). /proc이 필요하다.
        """
        found = []
        for entry in os.listdir("/proc"):
            if not entry.isdigit():
                continue
            try:
                with open("/proc/%s/stat" % entry) as stat:
                    fields = stat.read().rsplit(")", 1)[1].split()
            except OSError:  # the process ended meanwhile
                continue
            if int(fields[1]) == self.process.pid:
                found.append((int(entry), fields[0]))
        return found

    def children(self):
        """States ("R", "S", "Z", ...) of the server's child processes, or
        None where /proc is missing.
        서버 자식 프로세스의 상태("R", "S", "Z", ...). /proc이 없으면 None.
        """
        if not os.path.isdir("/proc/self"):
            return None
        return [state for _, state in self._child_stats()]

    def child_pids(self):
        """Pids of the server's child processes; needs /proc.
        서버 자식 프로세스의 pid. /proc이 필요하다.
        """
        return [pid for pid, _ in self._child_stats()]

    def peak_memory_kb(self):
        """Peak resident memory (VmHWM) of the server, or None without
        /proc.
        서버의 최대 상주 메모리(VmHWM). /proc이 없으면 None.
        """
        status_path = "/proc/%d/status" % self.process.pid
        if not os.path.exists(status_path):
            return None
        with open(status_path) as status:
            for line in status:
                if line.startswith("VmHWM:"):
                    return int(line.split()[1])
        return None

    def is_running(self):
        """True until the process exits.
        프로세스가 끝나기 전까지 True.
        """
        return self.process.poll() is None

    def send_signal(self, signal_number):
        """Sends a signal and returns the exit code.
        시그널을 보내고 종료 코드를 돌려준다.
        """
        self.process.send_signal(signal_number)
        return self.process.wait(timeout=EXIT_TIMEOUT_SECONDS)

    def stop(self):
        """Ends the server. Under WRAPPER it gets SIGINT and must exit with
        0, so that the wrapper's checks (leaks, errors) count.
        서버를 끝낸다. WRAPPER 아래에서는 SIGINT를 받고 0으로 끝나야 한다.
        그래야 wrapper의 검사(누수, 오류)가 반영된다.
        """
        if WRAPPER and self.is_running():
            self.process.send_signal(signal.SIGINT)
            code = self.process.wait(timeout=EXIT_TIMEOUT_SECONDS)
            self.process.stderr.close()
            if code != 0:
                raise AssertionError("wrapped webserv exited with %d" % code)
            return
        if self.is_running():
            self.process.kill()
            self.process.wait(timeout=EXIT_TIMEOUT_SECONDS)
        self.process.stderr.close()


def can_connect(port):
    """True if a connection to the port succeeds.
    포트에 연결할 수 있으면 True.
    """
    try:
        with socket.create_connection((HOST, port), timeout=1):
            return True
    except OSError:
        return False


def connect(port):
    """A connected socket with the test timeout.
    테스트 시간 제한을 둔 연결된 소켓.
    """
    return socket.create_connection((HOST, port),
                                    timeout=SOCKET_TIMEOUT_SECONDS)


def read_until_closed(sock):
    """Everything the server sends until it closes the connection.
    서버가 연결을 닫을 때까지 보낸 전부.
    """
    chunks = []
    while True:
        chunk = sock.recv(RECV_SIZE)
        if not chunk:
            return b"".join(chunks)
        chunks.append(chunk)


def read_response(stream, head_only=False):
    """One response from sock.makefile("rb"), cut by its Content-Length.
    A response to HEAD has no body (head_only). Returns what arrived if the
    connection closes first.
    sock.makefile("rb")에서 Content-Length만큼 자른 응답 하나. HEAD에 대한
    응답에는 본문이 없다(head_only). 연결이 먼저 닫히면 도착한 만큼
    돌려준다.
    """
    head = b""
    while not head.endswith(b"\r\n\r\n"):
        line = stream.readline()
        if not line:
            return head
        head += line
    if head_only:
        return head
    _, headers, _ = parse(head)
    return head + stream.read(int(headers.get("content-length", "0")))


def exchange(port, request=SIMPLE_GET):
    """Sends one request and returns its response. The connection may stay
    open (keep-alive), so the response is cut by its length.
    요청 하나를 보내고 응답을 돌려준다. 연결이 열린 채로 남을 수 있어서
    (keep-alive), 응답은 길이로 자른다.
    """
    with connect(port) as sock, sock.makefile("rb") as stream:
        sock.sendall(request)
        return read_response(stream, head_only=request.startswith(b"HEAD "))


def status_line(response):
    """The first line of a response, as text.
    응답의 첫 줄, 텍스트로.
    """
    return response.split(b"\r\n", 1)[0].decode("latin-1")


def request(head_lines, body=b""):
    """A raw request from its head lines and body.
    머리 줄과 본문으로 만든 날것의 요청.
    """
    return ("\r\n".join(head_lines) + "\r\n\r\n").encode("latin-1") + body


def get(path, method="GET"):
    """A body-less HTTP/1.1 request for path.
    path에 대한 본문 없는 HTTP/1.1 요청.
    """
    return request(["%s %s HTTP/1.1" % (method, path), "Host: x"])


def parse(response):
    """(status code, {lower-case name: value}, body).
    (상태 코드, {소문자 이름: 값}, 본문).
    """
    head, _, body = response.partition(b"\r\n\r\n")
    lines = head.decode("latin-1").split("\r\n")
    status = int(lines[0].split(" ")[1])
    headers = {}
    for line in lines[1:]:
        name, _, value = line.partition(":")
        headers[name.strip().lower()] = value.strip()
    return status, headers, body
