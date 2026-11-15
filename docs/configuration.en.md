[한국어](configuration.md) | **English** · [Back to README](../README.en.md)

# Configuration Reference

NGINX-style configuration parsing, supported directives, and routing order in `webserv`.

---

## 1. Parsing Order

Configuration loading (`src/config/`) runs at startup before any socket is opened. Any problem fails fast with a `ConfigError` showing the line number.

1. **`ConfigLexer`:** Splits words, `{`, `}`, `;`, quoted strings, and `#` comments while tracking line numbers.
2. **`ConfigSyntax`:** Checks matching braces, semicolons, and block nesting depth (`e58067d`) so deeply nested braces do not overflow the stack.
3. **`DirectiveRules` (`src/config/DirectiveRules.cpp`):** Uses a directive table to check valid context (`server` vs `location`), argument counts, block/semicolon form, and repeatability.
4. **`ServerBuilder` / `LocationBuilder`:** Inherits `server` defaults into each `location`, validates IPv4/port pairs and body limits, and sorts locations **longest-prefix first** so routing stays deterministic.

---

## 2. Supported Directives

All 12 directives defined in `src/config/DirectiveRules.cpp`:

| Directive | Context | Args | Repeatable | Description |
| :--- | :--- | :--- | :--- | :--- |
| `listen` | `server` | `1` (`[ip:]port`) | Yes | Port is required (`127.0.0.1:8080` or `8080`). Duplicate or conflicting binds across servers are rejected. |
| `error_page` | `server` | `2+` (`code... uri`) | Yes | Maps status codes to a custom error URI served internally (max file size `1 MiB`). |
| `location` | `server` | `1` (`prefix`) | Yes | Prefix route block. Modifiers such as `=` are rejected with an explicit error message. |
| `root` | `server`, `location` | `1` (`path`) | No | Directory prepended to the normalized URI (`root www` + `/a.txt` → `www/a.txt`). |
| `alias` | `location` | `1` (`path`) | No | Replaces the matched location prefix at a segment boundary (`location /docs/ { alias www/documents/; }`). |
| `index` | `server`, `location` | `1+` (`file...`) | No | Index files tried in order for directories (slashes in names are rejected). |
| `autoindex` | `server`, `location` | `1` (`on` \| `off`) | No | Turns HTML directory listing on or off when no index file exists. |
| `client_max_body_size` | `server`, `location` | `1` (`size[k\|m]`) | No | Request body limit (`1` byte to `100m`, default `1m`). Larger bodies get `413`. |
| `return` | `location` | `2` (`code target`) | No | Replies immediately with a redirect or status before touching the filesystem (e.g., `return 301 /about.html;`). |
| `limit_except` | `location` | `1+` (`method... { deny all; }`) | No | Lists accepted methods. **Locations without `limit_except` accept `GET` and `HEAD` only.** |
| `upload_store` | `location` | `1` (`dir`) | No | Target directory for `POST` `multipart/form-data` uploads (`201`/`409`) and `DELETE` (`204`). |
| `cgi` | `location` | `2` (`ext interpreter`) | Yes | Maps a file extension (such as `.py`) to an absolute interpreter path (`/usr/bin/python3`). |

---

## 3. Routing Order

`Router::route()` (`include/route/Router.hpp`) checks each request in this order:

1. **Path Normalization (`PathResolver`):** Percent-decodes once, merges `//`, and resolves `.` and `..`. Trying to go above `/` returns `400 Bad Request`.
2. **Location Match (`matchLocation`):** Picks the longest matching prefix (`alias` only matches at a segment boundary). If nothing matches, returns `404`.
3. **`return` Directive:** Sends the configured redirect or status right away if present.
4. **Method Check (`limit_except`):** Disallowed methods return `405 Method Not Allowed` with an `Allow` header (unimplemented methods return `501`).
5. **Dispatch:**
   * `POST` with `multipart/form-data` on an `upload_store` location → `UploadHandler` (`201 Created`, or `409 Conflict` if the file exists).
   * `DELETE` on an `upload_store` location → `DeleteHandler` (`204 No Content`).
   * File extension matching `cgi` → `CgiProcess` (also resolves `PATH_INFO` and `PATH_TRANSLATED`).
   * Regular file → `StaticHandler` (read into memory up to `1 MiB`, streamed in `64 KiB` pieces above that).
   * Directory → redirects without trailing `/` (`301`), tries `index` files (up to `10` internal redirects), then falls back to `autoindex` or `403`.

---

## 4. Included Configuration Files

| File | What it is for |
| :--- | :--- |
| [conf/default.conf](../conf/default.conf) | Full demo on `127.0.0.1:8080` (`www/`) and a second site on `127.0.0.1:8081` (`www2/`): static files, autoindex, alias, 301 redirect, custom 404, 64B body limit, uploads, Python/PHP CGI. |
| [conf/ports.conf](../conf/ports.conf) | Multiple interfaces and ports (`127.0.0.1:8080`, `8081`, `127.0.0.2:8080`, `0.0.0.0:8082`). `127.0.0.2` needs Linux loopback or a macOS `lo0` alias. |
| [conf/same-port.conf](../conf/same-port.conf) | Negative test with two `server` blocks on `127.0.0.1:8080`. Rejected on startup because virtual hosts (`server_name`) are not implemented. |
