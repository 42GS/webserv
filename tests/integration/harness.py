"""Start webserv with a generated configuration and talk to it over TCP.

Only the Python standard library is used, so the tests run on the
evaluation machine (Ubuntu 22.04, Python 3.10) without installing anything.
"""

import os
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
)


def free_port():
    """A port that was free a moment ago (the kernel picks it)."""
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as probe:
        probe.bind((HOST, 0))
        return probe.getsockname()[1]


def server_block(*ports):
    listens = "".join("    listen %s:%d;\n" % (HOST, port) for port in ports)
    return "server {\n%s    root www;\n}\n" % listens


class Workspace:
    """A temporary directory used as the server's working directory, with
    www/index.html so that "root www" serves "/"."""

    def __init__(self):
        self._dir = tempfile.TemporaryDirectory(prefix="webserv-test-")
        self.path = self._dir.name
        self.write("www/index.html", DEFAULT_PAGE)

    def write(self, relative_path, text):
        full_path = os.path.join(self.path, relative_path)
        os.makedirs(os.path.dirname(full_path), exist_ok=True)
        with open(full_path, "w") as out:
            out.write(text)
        return full_path

    def cleanup(self):
        self._dir.cleanup()


def run_to_exit(args, cwd):
    """Runs webserv expected to stop by itself; returns (code, stderr)."""
    finished = subprocess.run([WEBSERV] + args, cwd=cwd, capture_output=True,
                              text=True, timeout=EXIT_TIMEOUT_SECONDS)
    return finished.returncode, finished.stderr


class RunningServer:
    """A webserv process that is accepting connections on its ports."""

    def __init__(self, args, cwd, ports):
        self.ports = ports
        self.process = subprocess.Popen([WEBSERV] + args, cwd=cwd,
                                        stdout=subprocess.DEVNULL,
                                        stderr=subprocess.PIPE, text=True)
        self._wait_until_listening()

    def _wait_until_listening(self):
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
        """Open descriptors of the server, or None where /proc is missing."""
        fd_dir = "/proc/%d/fd" % self.process.pid
        if not os.path.isdir(fd_dir):
            return None
        return len(os.listdir(fd_dir))

    def cpu_seconds(self):
        """User + system CPU time of the server, or None without /proc."""
        stat_path = "/proc/%d/stat" % self.process.pid
        if not os.path.exists(stat_path):
            return None
        with open(stat_path) as stat:
            # Fields after the command name, which is in parentheses.
            fields = stat.read().rsplit(")", 1)[1].split()
        user_ticks, system_ticks = int(fields[11]), int(fields[12])
        return (user_ticks + system_ticks) / os.sysconf("SC_CLK_TCK")

    def _child_stats(self):
        """(pid, state) of the server's children; needs /proc."""
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
        None where /proc is missing."""
        if not os.path.isdir("/proc/self"):
            return None
        return [state for _, state in self._child_stats()]

    def child_pids(self):
        """Pids of the server's child processes; needs /proc."""
        return [pid for pid, _ in self._child_stats()]

    def is_running(self):
        return self.process.poll() is None

    def send_signal(self, signal_number):
        """Sends a signal and returns the exit code."""
        self.process.send_signal(signal_number)
        return self.process.wait(timeout=EXIT_TIMEOUT_SECONDS)

    def stop(self):
        if self.is_running():
            self.process.kill()
            self.process.wait(timeout=EXIT_TIMEOUT_SECONDS)
        self.process.stderr.close()


def can_connect(port):
    try:
        with socket.create_connection((HOST, port), timeout=1):
            return True
    except OSError:
        return False


def connect(port):
    return socket.create_connection((HOST, port),
                                    timeout=SOCKET_TIMEOUT_SECONDS)


def read_until_closed(sock):
    """Everything the server sends until it closes the connection."""
    chunks = []
    while True:
        chunk = sock.recv(RECV_SIZE)
        if not chunk:
            return b"".join(chunks)
        chunks.append(chunk)


def read_response(stream, head_only=False):
    """One response from sock.makefile("rb"), cut by its Content-Length.
    A response to HEAD has no body (head_only). Returns what arrived if the
    connection closes first."""
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
    open (keep-alive), so the response is cut by its length."""
    with connect(port) as sock, sock.makefile("rb") as stream:
        sock.sendall(request)
        return read_response(stream, head_only=request.startswith(b"HEAD "))


def status_line(response):
    return response.split(b"\r\n", 1)[0].decode("latin-1")


def request(head_lines, body=b""):
    """A raw request from its head lines and body."""
    return ("\r\n".join(head_lines) + "\r\n\r\n").encode("latin-1") + body


def get(path, method="GET"):
    """A body-less HTTP/1.1 request for path."""
    return request(["%s %s HTTP/1.1" % (method, path), "Host: x"])


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
