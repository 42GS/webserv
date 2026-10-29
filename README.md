*This project has been created as part of the 42 curriculum by yoshin, jyoo.*

# webserv

## Description

webserv is an HTTP/1.1 server written in C++98. One process serves every
listening socket, client connection and CGI pipe from a single `poll()`
loop, with non-blocking I/O and one read or write per readiness event.

Features:

- NGINX-style configuration file: several `server` blocks on different
  `interface:port` pairs, `location` routes with `root` or `alias`,
  `index`, `autoindex`, `return` redirects, `error_page`,
  `client_max_body_size`, and `limit_except` for the accepted methods.
- GET, HEAD, POST and DELETE. A location without `limit_except` only
  answers GET and HEAD, so writing methods must be listed.
- Static files (large ones are streamed), directory listings, custom and
  built-in error pages, a `Date` header on every response.
- Request parsing per RFC 9112: chunked bodies, size limits (414, 431,
  413), keep-alive and pipelining, timeouts for every connection state.
- File uploads with `multipart/form-data` (`upload_store`), and DELETE.
- CGI by file extension (`cgi .py /usr/bin/python3;`): body on standard
  input, RFC 3875 meta-variables, script run in its own directory,
  output collected to EOF, 502 for a broken script, 504 after 10 seconds.
- Clean shutdown on SIGINT/SIGTERM: connections closed, CGI children
  killed and collected, all memory freed.
- Access and error logs in `logs/` (see Run below).

## Instructions

### Build

```sh
make            # ./webserv
make test       # unit tests
make integration  # integration tests (Python 3, standard library only)
```

The build uses `c++ -Wall -Wextra -Werror -std=c++98`. `make clean`,
`make fclean` and `make re` are available.

### Run

```sh
./webserv [configuration file]   # default: conf/default.conf
```

Run it from the repository root: paths in the demo configurations are
relative to it. Stop it with Ctrl-C.

The demo CGI scripts need `/usr/bin/python3`; the PHP example needs
`/usr/bin/php-cgi` (`sudo apt-get install php-cgi`).

The server appends one line per response to `logs/access.log` (client,
request line, status, body bytes) and writes start, stop, CGI failures,
timeouts and failed `accept()` calls to `logs/error.log`. The `logs`
directory must exist in the directory the server runs from; watch the
logs with `tail -f logs/*.log`.

The server also creates a hidden file, `.webserv_clock`, in the directory
it runs from. The allowed function list has no time function, so the
server rewrites this file and reads its modification time with `stat()`
to know the current time (for timeouts and the `Date` header).

### Demo configurations

| File | What it shows |
|---|---|
| `conf/default.conf` | The full demo on `127.0.0.1:8080` (site in `www/`) and a second site on `127.0.0.1:8081` (`www2/`) |
| `conf/ports.conf` | Different sites on different interfaces and ports (`127.0.0.2` needs Linux) |
| `conf/same-port.conf` | Two sites on the same `interface:port`: refused, since virtual hosts are not implemented |

With `./webserv conf/default.conf`, open <http://127.0.0.1:8080/> for the
browser demo, or use curl:

```sh
curl -i http://127.0.0.1:8080/                      # static site
curl -i http://127.0.0.1:8080/files/                # directory listing
curl -i http://127.0.0.1:8080/docs/                 # alias route with its index
curl -i http://127.0.0.1:8080/old/                  # 301 redirect
curl -i http://127.0.0.1:8080/nothing               # custom 404 page
curl -i -X DELETE http://127.0.0.1:8080/index.html  # 405: not allowed here
curl -i -X UNKNOWN http://127.0.0.1:8080/           # 501, no crash

# body size limit (64 bytes on /limit/)
curl -i -X POST -H "Content-Type: plain/text" --data "BODY IS HERE" \
     http://127.0.0.1:8080/limit/echo.py
curl -i -X POST -H "Content-Type: plain/text" \
     --data "BODY IS HERE write something longer than the body limit of 64 bytes" \
     http://127.0.0.1:8080/limit/echo.py            # 413

# upload, download, delete
curl -i -F "file=@README.md" http://127.0.0.1:8080/upload/   # 201
curl -i http://127.0.0.1:8080/upload/README.md
curl -i -X DELETE http://127.0.0.1:8080/upload/README.md     # 204
# Uploading a name that already exists gives 409; delete it first to
# upload it again.

# CGI
curl -i "http://127.0.0.1:8080/cgi-bin/hello.py?name=you"
curl -i -X POST --data "name=a&message=b" http://127.0.0.1:8080/cgi-bin/form.py
curl -i -H "Transfer-Encoding: chunked" --data "chunked body" \
     http://127.0.0.1:8080/cgi-bin/form.py
curl -i http://127.0.0.1:8080/cgi-bin/relative.py   # reads a file by relative path
curl -i http://127.0.0.1:8080/cgi-bin/error.py      # 502
curl -i http://127.0.0.1:8080/cgi-bin/loop.py       # 504 after 10 s
curl -i http://127.0.0.1:8080/cgi-bin/hello.php     # bonus: second CGI

# bonus: cookie session (repeat with the cookie to count visits)
curl -i -c jar -b jar http://127.0.0.1:8080/cgi-bin/session.py

# stress test on an empty page, as the evaluation sheet asks
siege -b -c 50 -t 1M http://127.0.0.1:8080/empty.html
```

### Configuration file

```nginx
server {
    listen 127.0.0.1:8080;          # or "listen 8080;" for all interfaces
    root www;                       # the URI is appended to it
    index index.html;
    error_page 404 /errors/404.html;
    client_max_body_size 1m;        # 1 byte to 100m

    location /upload/ {
        limit_except GET POST DELETE { deny all; }
        upload_store www/upload;
    }
    location /cgi-bin/ {
        limit_except GET POST { deny all; }
        cgi .py /usr/bin/python3;   # extension and absolute program path
    }
    location /kapouet/ { alias /tmp/www/; }
    location /old/ { return 301 /new/; }
}
```

### Tests

- `make test`: unit tests (C++98).
- `make integration`: starts the server and talks to it over TCP.
  `WEBSERV_SLOW_TESTS=1` adds the tests that wait for timeouts.
  `WEBSERV_WRAPPER="valgrind --error-exitcode=99 --leak-check=full ..."`
  runs every test server under that command and requires a clean exit.

## Resources

- RFC 9110 (HTTP semantics), RFC 9112 (HTTP/1.1), RFC 3986 (URI),
  RFC 3875 (CGI 1.1), RFC 7578 and RFC 2046 (multipart/form-data),
  RFC 6265 (cookies), RFC 1945 (HTTP/1.0).
- NGINX documentation: core module, request processing, index,
  autoindex and rewrite modules (behaviour and configuration syntax).
- POSIX.1-2024: `poll`, `recv`, `send`, `bind`, `fcntl`, `waitpid`,
  `_Exit`, `remove`, `rmdir`, `<signal.h>`.
- PHP manual (`cgi.force_redirect`, php.ini) and the php-cgi source, for
  the variables php-cgi needs.
- siege manual; C++ Core Guidelines and Google C++ Style Guide (code
  organisation).

### Use of AI

We used Claude Code (Anthropic) throughout the project, and reviewed and
tested everything it produced:

- **Research:** summarising the sources above into a personal wiki, with
  each claim tied to a line of the source, and running small experiments
  (macOS and Ubuntu 22.04) where the documents did not decide a question,
  such as `poll()` bits on closed pipes or closing after a 413.
- **Design:** proposing the class structure and listing the open
  decisions; the decisions themselves (for example 400 for `..` above the
  root, uploads with multipart only, GET/HEAD by default) were ours.
- **Code:** writing the C++ code step by step, each step reviewed and
  tested by us.
- **Refactor:** restructuring and cleaning up the code after reviews
  (naming, splitting modules, removing copies and out-parameters).
- **Tests:** writing the unit tests and the Python integration tests.
- **Review:** code reviews for correctness, security and object-oriented
  design, and stability checks with valgrind, Electric Fence and siege.
