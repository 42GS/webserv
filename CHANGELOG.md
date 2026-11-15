**English** | [한국어](CHANGELOG.ko.md) · [Back to README](README.md)

# Changelog

All notable milestones of **webserv** reconstructed from the commit history, organized for Git tags and GitHub Releases.

---

## [v1.0.0] — 2026-10-29 (Commit `4db9510`)

**Production-Ready Release: Observability, CGI Security Hardening & Final Polish**

### Added
* **Access & Error Logging (`13982df`):** Per-response access log (`logs/access.log`) and lifecycle/timeout/CGI/accept failure error log (`logs/error.log`).
* **Benchmark Fixture (`5ca93a2`):** Added `www/empty.html` for `siege` stress testing.

### Security & Fixes
* **CGI Header Hardening (`cad952e`):** Mitigated **httpoxy (`CVE-2016-5385`)** by dropping `Proxy`, blocked underscore (`_`) header spoofing, and stripped hop-by-hop headers from CGI responses.
* **Local Redirect Preservation (`8f8dc8a`):** Kept CGI client-redirect response bodies intact.
* **Standard Portability (`664692b`):** Removed `std::auto_ptr` so the codebase builds cleanly both with `-std=c++98` and modern C++ compilers.

---

## [v0.6.0] — 2026-10-18 (Commit `6b4ffc2`)

**Stability, Memory Safety & Demo Site (Steps 8–9)**

### Stability & Performance
* **FD Exhaustion Backpressure (`6b4ffc2`):** Paused `Listener` polling when `accept()` fails on a full descriptor table and resumed on connection close or tick, eliminating 100% CPU busy-spin.
* **Zero-copy CGI Handoff (`6b4ffc2`):** Returned collected CGI output via `std::string::swap` to eliminate multi-megabyte buffer copies.
* **Memory & Bounds Audit (`d4ce794`, `83f413c`):** Verified zero leaks and clean signal shutdowns under **Valgrind** and **Electric Fence**; completed step 9 interactive demo site (`www/`).

---

## [v0.5.0] — 2026-10-18 (Commit `1655980`)

**Asynchronous CGI/1.1 Execution (Step 7)**

### Added
* **Non-blocking CGI Pipeline:** Spawned CGI scripts (`fork`/`execve`) in the script's own directory with RFC 3875 meta-variables (`CgiEnv`), polling stdin/stdout pipes (`CgiPipes`) inside the main `poll()` loop.
* **Timeout & Zombie Prevention:** Enforced `10s` CGI timeout (`504 Gateway Timeout`), `502 Bad Gateway` on malformed output, and non-blocking `waitpid(WNOHANG)` collection via `ChildReaper`.

---

## [v0.4.0] — 2026-10-17 (Commit `3385e7e`)

**File Uploads & Deletion (Step 6)**

### Added
* **`multipart/form-data` Upload (`bb13b56`, `ea1b330`):** Linear-time multipart boundary parser (RFC 7578 / 2046) writing uploaded files to `upload_store` (`201 Created`, `409 Conflict` naming the conflicting file `8664a05`).
* **`DELETE` Method (`bb13b56`):** Safe deletion of uploaded resources (`204 No Content`) restricted by `limit_except`.
* **Abuse Prevention (`ea1b330`):** Capped uploaded file count per request (`kMaxUploadFiles = 5`) to protect the single-threaded event loop.

---

## [v0.3.0] — 2026-10-17 (Commit `870f06a`)

**Persistent Connections, Pipelining & Timeouts (Step 5)**

### Added
* **HTTP/1.1 Connection Lifecycle:** Implemented `keep-alive`, HTTP pipelining over buffered `inBuffer_`, and `DRAIN` state (RFC 9112 §9.6) to avoid TCP `RST` erasing error responses.
* **Per-state Timeouts:** Enforced separate thresholds for idle keep-alive (`15s`), head completion (`30s`), body progress (`30s`), send stall (`30s`), and draining (`1s` idle / `5s` total).

---

## [v0.2.0] — 2026-10-16 (Commit `7404b94`)

**Incremental HTTP/1.1 Parser, Router & Static Serving (Steps 3–4)**

### Added
* **Incremental Parser (`98cb1e2`, `24fd810`):** Cursor-based HTTP/1.1 parser supporting arbitrary TCP chunk splits, `Content-Length`, `Transfer-Encoding: chunked`, and size limits (`413`, `414`, `431`).
* **Router & Path Security (`fd5c868`, `4922eb8`):** Percent-decoding, dot-segment resolution, traversal rejection (`400`), `root`/`alias`, `return` redirects, `limit_except`, and `HEAD` handling.
* **Static Streaming & Listings (`40cb5e8`, `7404b94`):** `ResponseStream` chunked file reads (`64 KiB`), MIME types, IMF-fixdate `Date` header, `autoindex` HTML listings, and custom `error_page`.

---

## [v0.1.0] — 2026-10-16 (Commit `6f48721`)

**Core `poll()` Event Loop & NGINX-style Config (Steps 1–2)**

### Added
* **Configuration Subsystem (`8f4ed3b`, `855f851`, `e58067d`):** Lexer, syntax parser with nesting depth limit, and table-driven `DirectiveRules` for `server` and `location` blocks.
* **Event Loop & Listener (`6f48721`, `9854f52`):** Single-process `poll()` multiplexer (`EventLoop`), non-blocking listening sockets, and `stat()`-based `Clock`.
* **Test Harnesses (`d1761bd`, `4b68ef5`):** C++98 unit-test runner and Python standard-library TCP integration harness.
