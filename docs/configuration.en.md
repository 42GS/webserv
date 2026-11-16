[한국어](configuration.md) | **English** · [Back to README](../README.en.md)

# Configuration Reference

How `webserv` reads NGINX-style config files, which directives it accepts, and how requests are routed.

---

## 1. Parsing Order

Configuration loading (`src/config/`) runs at startup before opening any socket. Any invalid token or rule fails fast with a line-numbered `ConfigError`.

1. `ConfigLexer` splits words, `{`, `}`, `;`, quoted strings, and `#` comments while tracking source line numbers.
2. `ConfigSyntax` checks matching braces, semicolons, and block nesting depth (`e58067d`) so deeply nested braces do not blow the stack.
3. `DirectiveRules` (`src/config/DirectiveRules.cpp`) checks valid blocks (`server` vs `location`), argument counts, block/semicolon form, and repeatability using a lookup table.
4. `ServerBuilder` and `LocationBuilder` pass `server` defaults down to each `location`, validate IPv4/port pairs and body limits, and sort locations longest-prefix first so routing stays deterministic.

---

## 2. Supported Directives

All 12 directives defined in `src/config/DirectiveRules.cpp`:

| Directive | Context | Args | Repeatable | Description |
| :--- | :--- | :--- | :--- | :--- |
| `listen` | `server` | `1` (`[ip:]port`) | Yes | Port is required (`127.0.0.1:8080` or `8080`). Duplicate or conflicting binds across servers are rejected. |
| `error_page` | `server` | `2+` (`code... uri`) | Yes | Maps status codes to a custom error URI served internally (max file size `1 MiB`). |
| `location` | `server` | `1` (`prefix`) | Yes | Prefix route block. Modifiers such as `=` are rejected with a specific error message. |
| `root` | `server`, `location` | `1` (`path`) | No | Directory prepended to the normalized URI (`root www` + `/a.txt` → `www/a.txt`). |
| `alias` | `location` | `1` (`path`) | No | Replaces the matched location prefix at a segment boundary (`location /docs/ { alias www/documents/; }`). |
| `index` | `server`, `location` | `1+` (`file...`) | No | Index files tried in order for directories (slashes in file names are rejected). |
| `autoindex` | `server`, `location` | `1` (`on` \| `off`) | No | Turns HTML directory listing on or off when no index file exists. |
| `client_max_body_size` | `server`, `location` | `1` (`size[k\|m]`) | No | Request body limit (`1` byte to `100m`, default `1m`). Larger bodies get `413`. |
| `return` | `location` | `2` (`code target`) | No | Replies right away with a redirect or status before touching the filesystem (e.g., `return 301 /about.html;`). |
| `limit_except` | `location` | `1+` (`method... { deny all; }`) | No | Lists accepted methods. **Locations without `limit_except` accept `GET` and `HEAD` only.** |
| `upload_store` | `location` | `1` (`dir`) | No | Target directory for `POST` `multipart/form-data` uploads (`201`/`409`) and `DELETE` (`204`). |
| `cgi` | `location` | `2` (`ext interpreter`) | Yes | Maps a file extension (such as `.py`) to an absolute interpreter path (`/usr/bin/python3`). |

---

## 3. Routing Order

`Router::route()` (`include/route/Router.hpp`) evaluates each request in this order:

1. `PathResolver` percent-decodes once, merges `//`, and resolves `.` and `..`. Trying to go above `/` returns `400 Bad Request`.
2. `matchLocation()` picks the longest matching prefix (`alias` only matches at a segment boundary). If nothing matches, it returns `404`.
3. If `return` is set on that location, the redirect or status response is returned right away.
4. `limit_except` rules are checked next. Disallowed methods return `405 Method Not Allowed` with an `Allow` header (unimplemented methods return `501`).
5. Once those checks pass, `Router` picks the target handler:
   * A `multipart/form-data` `POST` on an `upload_store` location goes to `UploadHandler` (`201 Created`, or `409 Conflict` if the file exists).
   * A `DELETE` on an `upload_store` location goes to `DeleteHandler` (`204 No Content`).
   * A file extension matching `cgi` runs through `CgiProcess` (also resolving `PATH_INFO` and `PATH_TRANSLATED`).
   * A regular file goes to `StaticHandler` (read into memory up to `1 MiB`, streamed in `64 KiB` pieces above that).
   * A directory without a trailing `/` redirects with `301`; otherwise `Router` tries `index` files (up to `10` internal redirects), then falls back to `autoindex` or `403`.

---

## 4. Included Configuration Files

| File | Purpose |
| :--- | :--- |
| [conf/default.conf](../conf/default.conf) | Full demo on `127.0.0.1:8080` (`www/`) and a second site on `127.0.0.1:8081` (`www2/`): static files, autoindex, alias, 301 redirect, custom 404, 64B body limit, uploads, Python/PHP CGI. |
| [conf/ports.conf](../conf/ports.conf) | Multiple interfaces and ports (`127.0.0.1:8080`, `8081`, `127.0.0.2:8080`, `0.0.0.0:8082`). `127.0.0.2` needs Linux loopback or a macOS `lo0` alias. |
| [conf/same-port.conf](../conf/same-port.conf) | Negative test with two `server` blocks on `127.0.0.1:8080`. Rejected on startup because virtual hosts (`server_name`) are not implemented. |
