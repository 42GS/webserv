[한국어](architecture.md) | **English** · [Back to README](../README.en.md)

# Architecture & Design Notes

Notes on how `webserv` works internally and why certain design choices were made.

---

## 1. Module Layout

The codebase is split into 8 directories under `include/` and `src/`. Dependencies point downward toward `core`, `fs`, and `http`.

```mermaid
flowchart TB
    Main["main.cpp (Startup & Signals)"] --> Config["config (Lexer, Parser & DirectiveRules)"]
    Main --> Server["core::Server (Sockets & Connections)"]
    Server --> EventLoop["core::EventLoop (Single poll() Loop)"]
    EventLoop -->|IoHandler| Listener["core::Listener (accept)"]
    EventLoop -->|IoHandler| Conn["http::Connection (State Machine)"]
    EventLoop -->|IoHandler| CgiPipes["cgi::CgiStdin / CgiStdout (Pipe I/O)"]

    Conn --> Parser["http::RequestParser (Incremental HTTP Parser)"]
    Conn --> Processor["app::RequestProcessor (Request Dispatch)"]
    Conn --> Stream["http::ResponseStream (File Streaming)"]

    Processor --> Router["route::Router & PathResolver (Normalization & Routing)"]
    Processor --> Handlers["handler (Static / Autoindex / Upload / Delete / StatusPage)"]
    Processor --> CgiProc["cgi::CgiProcess (fork/execve, CgiEnv, CgiOutput)"]

    Handlers --> FS["fs (FileInfo, FilePath, FileReader, FileWriter)"]
    CgiProc --> Reaper["core::ChildReaper (waitpid WNOHANG)"]
```

| Directory | Responsibility |
| :--- | :--- |
| `core/` | Single `poll()` loop, listening sockets, `.webserv_clock`, signal flags, child reaping, logging |
| `config/` | Config tokenizer, syntax parser, table-driven `DirectiveRules`, server and location builders |
| `http/` | Connection state machine, incremental RFC 9112 parser, response streaming, multipart parser, MIME types |
| `route/` | Path normalization, traversal rejection (`..`), longest-prefix location matching |
| `handler/` | Static files, directory listings (`autoindex`), uploads, `DELETE`, status pages |
| `cgi/` | CGI meta-variables, header filtering, non-blocking pipe I/O, CGI output parsing |
| `fs/` | File info checks, path joining, bounded file reads and writes |
| `app/` | Connects parsed requests to routing, handlers, CGI launch, and common headers (`Date`, `Connection`) |

---

## 2. Single `poll()` Loop & Deferred Removal

`EventLoop` (`include/core/EventLoop.hpp`, `src/core/EventLoop.cpp`) watches all file descriptors—listening sockets, client sockets, and CGI pipes—in one thread with a single `poll()` call (`1000 ms` timeout).

### Why `unwatch()` marks handlers instead of erasing right away

During `dispatch()`, an `IoHandler::onEvent()` callback can close a connection or finish a CGI pipe and call `unwatch()`. Erasing directly from `handlers_` mid-loop would invalidate iteration or touch a freed handler later in the same pass.

To avoid that, removal happens in two steps:

1. `dispatch()` copies `handlers_` into `polled` and sets `dispatching_ = true`.
2. Calling `unwatch(handler)` adds the pointer to `unwatched_` and skips remaining events for that handler in the current round.
3. After `dispatch()` returns, `dropUnwatched()` removes marked handlers from `handlers_`. Closed `Connection` objects are deleted outside dispatch in `Server::afterIteration()`.

### Fault Isolation & FD Exhaustion

If `std::exception` (including `std::bad_alloc`) escapes a handler or timeout check, `EventLoop::dispatch()` and `Server::afterIteration()` call `onFailure()` to close only that connection instead of bringing down the server.

When `accept()` fails because the descriptor table is full, `Listener` pauses `POLLIN` polling (`Server::acceptFailed`) rather than spinning at 100% CPU failing `accept()` on every pass. `Server::afterIteration()` calls `resume()` once a connection closes or one second passes.

---

## 3. Connection State Machine & Timeouts

Each client socket belongs to one `http::Connection`. Each readiness event triggers at most one `recv()` or `send()`.

```mermaid
stateDiagram-v2
    [*] --> READ_HEAD: accept()
    READ_HEAD --> READ_BODY: HEAD_DONE (apply location body limit)
    READ_HEAD --> WRITE: Parse error (400/413/414/431/501/505) or Timeout (408)
    READ_BODY --> WAIT_CGI: DONE -> CgiProcess started
    READ_BODY --> WRITE: DONE -> Static/Upload/Delete/Error response
    READ_BODY --> WRITE: Error or Timeout (408)
    WAIT_CGI --> WRITE: CGI finished (response) or Timeout (504)
    WAIT_CGI --> WAIT_CGI: CGI local redirect (up to 10x)
    WRITE --> READ_HEAD: Sent & keepAlive == true (pipeline parseInput)
    WRITE --> DRAIN: Sent & !keepAlive & !requestConsumed
    WRITE --> CLOSED: Sent & !keepAlive & requestConsumed
    DRAIN --> CLOSED: Peer closed or drain timeout
    READ_HEAD --> CLOSED: Idle keep-alive timeout or EOF
    WRITE --> CLOSED: Send stalled timeout or error
    CLOSED --> [*]
```

### Why `DRAIN` is needed (RFC 9112 §9.6)

If the server sends an early error like `413` and closes the socket while unread request bytes are still sitting in the kernel receive buffer, the OS sends a TCP `RST`. That reset can wipe out the error response before the client reads it. Moving to `DRAIN` lets the server read and discard remaining input briefly (`1s` idle, `5s` total) before closing cleanly.

### Limits and Timeouts (`include/core/Limits.hpp`, `include/config/ConfigLimits.hpp`)

| Constant | Value | Purpose |
| :--- | :--- | :--- |
| `kPollTimeoutMs` | `1000 ms` | `poll()` interval so timeout checks run periodically |
| `kKeepAliveTimeoutSeconds` | `15 s` | Wait for the first byte on a new or kept-alive connection (silent close) |
| `kHeaderTimeoutSeconds` | `30 s` | Total time allowed for the request head after its first byte |
| `kBodyTimeoutSeconds` | `30 s` | Idle time allowed while receiving the request body |
| `kSendTimeoutSeconds` | `30 s` | Idle time allowed without `send()` progress |
| `kDrainIdleSeconds` / `Total` | `1 s` / `5 s` | Idle and total limits while draining unread input before close |
| `kCgiTimeoutSeconds` | `10 s` | Time allowed for a CGI script to finish (`504` if exceeded) |
| `kMaxRequestLine` / `kMaxHeaderBytes` | `8 KiB` / `16 KiB` | Max request line (`414`) and max header + trailer bytes (`431`) |
| `kInMemoryFileLimit` / `kFileChunkSize` | `1 MiB` / `64 KiB` | Cutoff for in-memory static files and piece size for streamed files |
| `kMaxCgiOutputBytes` | `16 MiB` | Max buffered CGI output (`502` if exceeded) |
| `kMaxUploadFiles` | `5` | Max files stored by one upload request (`413` if exceeded) |
| `kDefaultMaxBodySize` / `Ceiling` | `1 MiB` / `100 MiB` | Default and maximum configurable `client_max_body_size` |

---

## 4. Incremental HTTP/1.1 Parser

`RequestParser` (`include/http/RequestParser.hpp`) handles incoming bytes regardless of how TCP splits packets.

Instead of erasing from the front of `inBuffer_` on every read, it advances a cursor (`consumed_`) and erases once at the end of `feed()`. Any pipelined bytes for the next request stay in the buffer intact.

The body limit is applied in two phases. `maxBodySize_` starts at `0` until the head finishes (`HEAD_DONE`), at which point `Connection` asks `RequestProcessor::bodyLimitFor()` for the matched location's `client_max_body_size` before parsing the body. Both `Content-Length` (with overflow checks) and `Transfer-Encoding: chunked` (with trailers counted against `kMaxHeaderBytes`) are supported.

---

## 5. Non-blocking CGI/1.1 & Security

`CgiProcess` (`include/cgi/CgiProcess.hpp`, `src/cgi/CgiEnv.cpp`) runs CGI scripts over non-blocking pipes so the `poll()` loop never stalls.

`CgiStdin` and `CgiStdout` implement `IoHandler` and register with `EventLoop`. Inside the child process, `runChild()` hooks up stdin/stdout with `dup2`, closes inherited descriptors, `chdir()`s into the script's directory, and calls `execve()`. Collected output (up to `16 MiB`) is returned in `takeOutput()` using `std::string::swap`, avoiding multi-megabyte copies in C++98. Finished or killed (`SIGKILL`) CGI PIDs go to `ChildReaper`, which reaps them without blocking via `waitpid(pid, ..., WNOHANG)` on every iteration.

Three security checks guard CGI execution and path resolution:

* `CgiEnv` drops `Proxy` so `HTTP_PROXY` cannot be injected (`httpoxy`, `CVE-2016-5385`).
* Header names containing `_` are ignored (matching NGINX defaults) so `X_FOO` cannot collide with `HTTP_X_FOO` from `X-Foo`.
* `PathResolver` percent-decodes once, resolves `.` and `..`, rejects paths going above root with `400 Bad Request`, and requires `alias` matches at segment boundaries.

---

## 6. Wall-Clock Time Without Time Functions (`.webserv_clock`)

The 42 `webserv` subject forbids standard time calls (`time()`, `gettimeofday()`, `clock_gettime()`).

Once per `EventLoop` iteration, `Clock::refresh()` rewrites `.webserv_clock` and reads its modification timestamp (`st_mtime`) using `stat()`, which is allowed. All connection timeouts, CGI timeouts, and RFC 9110 `Date` headers read from this single `Clock` class, keeping time handling isolated and easy to unit test (`clock_test.cpp`).
