[한국어](CHANGELOG.md) | **English** · [Back to README](README.en.md)

# Changelog

Milestones of `webserv` grouped from the commit history into Git tags and releases.

---

## [v1.0.0] (2026-10-29, `4db9510`)

Access/Error Logs, CGI Header Hardening & Release

### Added
* Added `logs/access.log` (one line per response) and `logs/error.log` (startup, shutdown, timeouts, CGI failures, and failed `accept()` calls) (`13982df`).
* Added `www/empty.html` for `siege` stress testing (`5ca93a2`).

### Fixes & Security
* Dropped `Proxy` in `CgiEnv` to block `httpoxy` (`CVE-2016-5385`), ignored header names containing `_`, and stripped hop-by-hop headers from CGI responses (`cad952e`).
* Kept the response body intact on CGI client redirects (`8f8dc8a`).
* Removed `std::auto_ptr` so the code builds both with `-std=c++98` and without `-std=` flags (`664692b`).

---

## [v0.6.0] (2026-10-18, `6b4ffc2`)

Stability, Memory Checks & Demo Site (Steps 8–9)

* Paused `Listener` polling when `accept()` fails on a full FD table, resuming when a connection closes or after one second (`6b4ffc2`).
* Returned CGI output via `std::string::swap` instead of copying multi-megabyte buffers (`6b4ffc2`).
* Checked leaks and bounds with Valgrind and Electric Fence, and added the browser demo under `www/` (`d4ce794`, `83f413c`).

---

## [v0.5.0] (2026-10-18, `1655980`)

Non-blocking CGI/1.1 Execution (Step 7)

* Spawned scripts with `fork`/`execve` inside the script's directory, passing RFC 3875 meta-variables (`CgiEnv`) and polling stdin/stdout pipes (`CgiPipes`) in the main `poll()` loop.
* Returned `504` after `10s`, `502` on broken output, and collected finished or killed children via `ChildReaper` using `waitpid(WNOHANG)`.

---

## [v0.4.0] (2026-10-17, `3385e7e`)

File Uploads & Deletion (Step 6)

* Added a linear-time `multipart/form-data` parser using slice offsets to save files to `upload_store` (`201`, or `409` naming the existing file) (`bb13b56`, `8664a05`, `ea1b330`).
* Supported `DELETE` on `upload_store` locations (`204`) when allowed by `limit_except` (`bb13b56`).
* Capped saved files per request at `5` (`kMaxUploadFiles`) so one upload cannot stall the event loop (`ea1b330`).

---

## [v0.3.0] (2026-10-17, `870f06a`)

Keep-Alive, Pipelining & Timeouts (Step 5)

* Added `keep-alive`, pipelining over buffered input in `inBuffer_`, and `DRAIN` (RFC 9112 §9.6) to avoid TCP `RST` wiping out error responses.
* Enforced separate timeouts for idle keep-alive (`15s`), head (`30s`), body (`30s`), send (`30s`), and draining (`1s` idle / `5s` total).

---

## [v0.2.0] (2026-10-16, `7404b94`)

Incremental HTTP/1.1 Parser, Router & Static Serving (Steps 3–4)

* Built `RequestParser` for split TCP reads, `Content-Length`, `Transfer-Encoding: chunked`, and size limits (`413`, `414`, `431`) (`98cb1e2`, `24fd810`).
* Added percent-decoding, dot-segment normalization, traversal rejection (`400`), `root`/`alias`, `return`, `limit_except`, and `HEAD` (`fd5c868`, `4922eb8`).
* Added `ResponseStream` (`64 KiB` pieces), MIME types, `Date` formatting, `autoindex`, and custom `error_page` (`40cb5e8`, `7404b94`).

---

## [v0.1.0] (2026-10-16, `6f48721`)

`poll()` Event Loop & Config Parser (Steps 1–2)

* Built the lexer, syntax parser with a nesting depth cap, and table-driven `DirectiveRules` (`8f4ed3b`, `855f851`, `e58067d`).
* Built the single `poll()` loop (`EventLoop`), non-blocking listeners, and `.webserv_clock`-based `Clock` (`6f48721`, `9854f52`).
* Set up the C++98 unit test runner and Python TCP integration harness (`d1761bd`, `4b68ef5`).
