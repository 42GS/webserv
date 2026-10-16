"""Constants of the integration tests, in one place (as the C++ headers).

Values that mirror the server (limits, timeouts) must follow
include/core/Limits.hpp.
"""

import os
import re

# Where the server is
REPO_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), "..", ".."))
WEBSERV = os.path.join(REPO_ROOT, "webserv")
HOST = "127.0.0.1"

# Harness timing
STARTUP_TIMEOUT_SECONDS = 5.0
EXIT_TIMEOUT_SECONDS = 5.0
SOCKET_TIMEOUT_SECONDS = 5.0
RETRY_INTERVAL_SECONDS = 0.05
RECV_SIZE = 65536

# Requests and content
SIMPLE_GET = b"GET / HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n"
DEFAULT_PAGE = "hello from www\n"  # www/index.html of every workspace
CUSTOM_404 = "<p>custom not found</p>\n"
BIG_FILE_SIZE = 10 * 1024 * 1024
SMALL_LIMIT = 10  # client_max_body_size of the /small/ location

# Server behaviour the tests expect (core/Limits.hpp)
HEAD_LIMIT_BYTES = 16384  # limits::kMaxHeaderBytes
HEADER_TIMEOUT_SECONDS = 30  # limits::kHeaderTimeoutSeconds
KEEP_ALIVE_TIMEOUT_SECONDS = 15  # limits::kKeepAliveTimeoutSeconds
SEND_TIMEOUT_SECONDS = 30  # limits::kSendTimeoutSeconds
DRAIN_TOTAL_SECONDS = 5  # limits::kDrainTotalSeconds
TIMEOUT_MARGIN_SECONDS = 5
EXIT_SUCCESS = 0
EXIT_FAILURE = 1
DEEP_NESTING = 100000  # crashed the recursive parser before the depth limit

# Load and timing of connection tests
CONCURRENT_CLIENTS = 50
ABORTED_CLIENTS = 50
FD_SETTLE_SECONDS = 3
TRICKLE_INTERVAL_SECONDS = 5
IDLE_CLIENTS = 10
IDLE_SECONDS = 2
KEEP_ALIVE_PAUSE_SECONDS = 5  # wait between a response and the next head
DRAIN_POLL_SECONDS = 0.3  # how often the drain tests send and look
UNREAD_FILE_SIZE = 64 * 1024 * 1024  # more than the socket buffers hold
MAX_IDLE_CPU_SHARE = 0.1

# Slow tests (header timeouts, full builds) run only when asked
RUN_SLOW = os.environ.get("WEBSERV_SLOW_TESTS") == "1"

# Build rules
SOURCE_ENTRIES = ["Makefile", "include", "src"]
COMPILER = "c++ "
BUILD_TIMEOUT_SECONDS = 600

# Formats
IMF_FIXDATE = re.compile(
    r"^(Mon|Tue|Wed|Thu|Fri|Sat|Sun), \d\d "
    r"(Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|Sep|Oct|Nov|Dec) \d{4} "
    r"\d\d:\d\d:\d\d GMT$")
README = os.path.join(REPO_ROOT, "README.md")
README_FIRST_LINE = re.compile(
    r"^\*This project has been created as part of the 42 curriculum by "
    r"[a-z0-9-]+(, [a-z0-9-]+)*\.\*$")
README_SECTIONS = ["## Description", "## Instructions", "## Resources"]
