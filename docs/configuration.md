**English** | [한국어](configuration.ko.md) · [Back to README](../README.md)

# Configuration Reference

> NGINX-style configuration grammar, directive table, routing precedence, and demo presets for **webserv**.

---

## 1. Parsing Pipeline

Configuration loading (`src/config/`) runs strictly at startup before opening sockets, failing fast with line-numbered `ConfigError` diagnostics:

1. **`ConfigLexer`:** Tokenizes words, `{`, `}`, `;`, quoted strings, and `#` comments while tracking source line numbers.
2. **`ConfigSyntax`:** Validates brace balance, semicolons, and block nesting depth (`e58067d` prevents stack exhaustion from deeply nested braces).
3. **`DirectiveRules` (`src/config/DirectiveRules.cpp`):** Table-driven validation of context (`server` vs `location`), argument count bounds, block vs semicolon termination, and repeatability.
4. **`ServerBuilder` / `LocationBuilder`:** Resolves inheritance (`server` → `location`), validates IPv4/port pairs and body size limits, and sorts locations **longest-prefix first** for deterministic routing.

---

## 2. Directive Reference

All 12 supported directives defined in `src/config/DirectiveRules.cpp`:

| Directive | Context | Args | Repeatable | Description |
| :--- | :--- | :--- | :--- | :--- |
| `listen` | `server` | `1` (`[ip:]port`) | Yes | Explicit port required (`127.0.0.1:8080` or `8080`). Duplicate or conflicting binds across servers are rejected. |
| `error_page` | `server` | `2+` (`code... uri`) | Yes | Maps HTTP error codes to a custom URI served via internal routing (max page size `1 MiB`). |
| `location` | `server` | `1` (`prefix`) | Yes | Prefix route block (exact/regex modifiers like `=` are explicitly rejected with a diagnostic). |
| `root` | `server`, `location` | `1` (`path`) | No | Appends normalized URI path to directory (`root www` + `/a.txt` → `www/a.txt`). |
| `alias` | `location` | `1` (`path`) | No | Replaces the matched location prefix at segment boundary (`location /docs/ { alias www/documents/; }`). |
| `index` | `server`, `location` | `1+` (`file...`) | No | Index file candidates tried in order for directories (slashes in names are rejected). |
| `autoindex` | `server`, `location` | `1` (`on` \| `off`) | No | Enables/disables HTML directory listing when no index file exists. |
| `client_max_body_size` | `server`, `location` | `1` (`size[k\|m]`) | No | Request body limit (`1` byte to `100m`, default `1m`). Exceeding triggers `413`. |
| `return` | `location` | `2` (`code target`) | No | Immediate redirect/status response before filesystem lookup (e.g., `return 301 /about.html;`). |
| `limit_except` | `location` | `1+` (`method... { deny all; }`) | No | Whitelists HTTP methods. **Locations without `limit_except` allow `GET` and `HEAD` only.** |
| `upload_store` | `location` | `1` (`dir`) | No | Destination directory for `POST` `multipart/form-data` uploads (`201` / `409`) and `DELETE` (`204`). |
| `cgi` | `location` | `2` (`ext interpreter`) | Yes | Maps file extension (e.g., `.py`) to an absolute interpreter path (`/usr/bin/python3`). |

---

## 3. Routing Precedence

`Router::route()` (`include/route/Router.hpp`) evaluates each request in a deterministic order:

1. **Path Normalization (`PathResolver`):** Percent-decodes once, collapses `//`, resolves `.` and `..`. Escaping above `/` returns `400 Bad Request`.
2. **Location Selection (`matchLocation`):** Selects the longest matching prefix (`alias` requires a segment boundary match). If none matches, returns `404`.
3. **`return` Directive:** Immediately returns configured redirect/status if present.
4. **Method Check (`limit_except`):** Returns `405 Method Not Allowed` with `Allow` header if the method is disallowed (`501` for unimplemented methods).
5. **CGI / Upload / Static Dispatch:**
   * `POST` with `multipart/form-data` on an `upload_store` location → `UploadHandler` (`201` created, `409` if file already exists).
   * `DELETE` on an `upload_store` location → `DeleteHandler` (`204` no content).
   * File extension matching `cgi` directive → `CgiProcess` (extracts `PATH_INFO` and `PATH_TRANSLATED`).
   * Regular file → `StaticHandler` (in-memory if `<= 1 MiB`, streamed in `64 KiB` chunks otherwise).
   * Directory → redirects without trailing `/` (`301`), tries `index` files (up to `10` internal redirects), falls back to `autoindex` or `403`.

---

## 4. Included Configuration Files

| File | Purpose |
| :--- | :--- |
| [conf/default.conf](../conf/default.conf) | Full demo on `127.0.0.1:8080` (`www/`) and a second server on `127.0.0.1:8081` (`www2/`): static files, autoindex, alias, 301 redirect, custom 404, 64B body limit, uploads, Python/PHP CGI. |
| [conf/ports.conf](../conf/ports.conf) | Multi-interface & multi-port setup (`127.0.0.1:8080`, `8081`, `127.0.0.2:8080`, `0.0.0.0:8082`). Note: `127.0.0.2` requires Linux loopback or `lo0` alias on macOS. |
| [conf/same-port.conf](../conf/same-port.conf) | Negative test case with two `server` blocks sharing `127.0.0.1:8080`; rejected on startup because virtual hosts (`server_name`) are intentionally out of scope. |
