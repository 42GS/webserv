"""Constants of the integration tests, in one place (as the C++ headers).
통합 테스트의 상수를 한곳에 모은다 (C++ 헤더처럼).

Values that mirror the server (limits, timeouts) must follow
include/core/Limits.hpp.
서버 값을 따라 하는 값(상한, 시간 제한)은 include/core/Limits.hpp와 같아야
한다.
"""

import os
import re

# Where the server is
# 서버가 있는 곳
REPO_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), "..", ".."))
WEBSERV = os.path.join(REPO_ROOT, "webserv")
# Optional command the server runs under, such as
# "valgrind --error-exitcode=99 --leak-check=full ...". With it every
# server is stopped with SIGINT and must exit with 0 (test plan N-23).
# 서버를 감싸 실행할 선택 명령. 예: "valgrind --error-exitcode=99
# --leak-check=full ...". 이것을 쓰면 모든 서버를 SIGINT로 멈추고, 서버는
# 0으로 끝나야 한다 (테스트 계획 N-23).
WRAPPER = os.environ.get("WEBSERV_WRAPPER", "").split()
# How much slower a wrapped server is allowed to be in timing checks.
# 시간 검사에서 감싼 서버가 얼마나 느려도 되는지.
WRAPPER_SLOWDOWN = 10 if WRAPPER else 1
HOST = "127.0.0.1"

# Harness timing
# 하네스의 시간
STARTUP_TIMEOUT_SECONDS = 30.0 if WRAPPER else 5.0
EXIT_TIMEOUT_SECONDS = 30.0 if WRAPPER else 5.0
SOCKET_TIMEOUT_SECONDS = 5.0
RETRY_INTERVAL_SECONDS = 0.05
RECV_SIZE = 65536

# Requests and content
# 요청과 내용
SIMPLE_GET = b"GET / HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n"
DEFAULT_PAGE = "hello from www\n"  # www/index.html of every workspace
CUSTOM_404 = "<p>custom not found</p>\n"
BIG_FILE_SIZE = 10 * 1024 * 1024
SMALL_LIMIT = 10  # client_max_body_size of the /small/ location

# Server behaviour the tests expect (core/Limits.hpp)
# 테스트가 기대하는 서버 동작 (core/Limits.hpp)
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
# 연결 테스트의 부하와 시간
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
# 느린 테스트(헤더 시간 초과, 전체 빌드)는 요청할 때만 돈다
RUN_SLOW = os.environ.get("WEBSERV_SLOW_TESTS") == "1"

# Build rules
# 빌드 규칙
SOURCE_ENTRIES = ["Makefile", "include", "src"]
COMPILER = "c++ "
BUILD_TIMEOUT_SECONDS = 600

# Formats
# 형식
IMF_FIXDATE = re.compile(
    r"^(Mon|Tue|Wed|Thu|Fri|Sat|Sun), \d\d "
    r"(Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|Sep|Oct|Nov|Dec) \d{4} "
    r"\d\d:\d\d:\d\d GMT$")
README = os.path.join(REPO_ROOT, "README.md")
README_FIRST_LINE = re.compile(
    r"^\*This project has been created as part of the 42 curriculum by "
    r"[a-z0-9-]+(, [a-z0-9-]+)*\.\*$")
README_SECTIONS = ["## Description", "## Instructions", "## Resources"]
LARGE_RESPONSE_SIZE = 8 * 1024 * 1024  # many send() calls per response
RESET_DURING_SEND_CLIENTS = 5
RESET_SETTLE_SECONDS = 1  # lets the server's next send() meet the reset
SMALL_FD_LIMIT = 16  # RLIMIT_NOFILE of the server in N-26
