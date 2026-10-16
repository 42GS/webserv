"""Start webserv with a generated configuration and talk to it over TCP.

Only the Python standard library is used, so the tests run on the
evaluation machine (Ubuntu 22.04, Python 3.10) without installing anything.
"""

import os
import socket
import subprocess
import tempfile
import time

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
WEBSERV = os.path.join(REPO_ROOT, "webserv")
HOST = "127.0.0.1"

STARTUP_TIMEOUT_SECONDS = 5.0
EXIT_TIMEOUT_SECONDS = 5.0
SOCKET_TIMEOUT_SECONDS = 5.0
RETRY_INTERVAL_SECONDS = 0.05
RECV_SIZE = 65536

SIMPLE_GET = b"GET / HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n"


def free_port():
    """A port that was free a moment ago (the kernel picks it)."""
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as probe:
        probe.bind((HOST, 0))
        return probe.getsockname()[1]


def server_block(*ports):
    listens = "".join("    listen %s:%d;\n" % (HOST, port) for port in ports)
    return "server {\n%s    root www;\n}\n" % listens


class Workspace:
    """A temporary directory used as the server's working directory."""

    def __init__(self):
        self._dir = tempfile.TemporaryDirectory(prefix="webserv-test-")
        self.path = self._dir.name

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


def exchange(port, request=SIMPLE_GET):
    """Sends one request and returns the whole response."""
    with connect(port) as sock:
        sock.sendall(request)
        return read_until_closed(sock)


def status_line(response):
    return response.split(b"\r\n", 1)[0].decode("latin-1")
