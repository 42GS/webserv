# Configuration Reference / 설정 파일 레퍼런스

> NGINX-style configuration grammar, directive table, routing precedence, and demo presets for **webserv**.
>
> **webserv**의 NGINX 스타일 설정 문법, 지시어 레퍼런스, 라우팅 우선순위 및 데모 설정 안내입니다.

---

## 1. Parsing Pipeline / 설정 파싱 파이프라인

Configuration loading (`src/config/`) runs strictly at startup before opening sockets, failing fast with line-numbered `ConfigError` diagnostics:

1. **`ConfigLexer`:** Tokenizes words, `{`, `}`, `;`, quoted strings, and `#` comments while tracking source line numbers.
2. **`ConfigSyntax`:** Validates brace balance, semicolons, and block nesting depth (`e58067d` prevents stack exhaustion from deeply nested braces).
3. **`DirectiveRules` (`src/config/DirectiveRules.cpp`):** Table-driven validation of context (`server` vs `location`), argument count bounds, block vs semicolon termination, and repeatability.
4. **`ServerBuilder` / `LocationBuilder`:** Resolves inheritance (`server` → `location`), validates IPv4/port pairs and body size limits, and sorts locations **longest-prefix first** for deterministic routing.

---

## 2. Directive Reference / 지시어 레퍼런스

All 12 supported directives defined in `src/config/DirectiveRules.cpp`:

| Directive | Context | Args | Repeatable | Description (EN / KR) |
| :--- | :--- | :--- | :--- | :--- |
| `listen` | `server` | `1` (`[ip:]port`) | Yes | Explicit port required (`127.0.0.1:8080` or `8080`). Duplicate/conflicting binds across servers are rejected. / 포트 명시 필수. 서버 간 중복 바인드 거부. |
| `error_page` | `server` | `2+` (`code... uri`) | Yes | Maps HTTP error codes to a custom URI served via internal routing (max page size `1 MiB`). / 오류 코드별 커스텀 에러 페이지 URI 지정. |
| `location` | `server` | `1` (`prefix`) | Yes | Prefix route block (exact/regex modifiers like `=` are explicitly rejected with a diagnostic). / 접두사 기반 라우트 블록. |
| `root` | `server`, `location` | `1` (`path`) | No | Appends normalized URI path to directory (`root www` + `/a.txt` → `www/a.txt`). / URI 경로를 이어 붙일 루트 디렉토리. |
| `alias` | `location` | `1` (`path`) | No | Replaces the matched location prefix at segment boundary (`location /docs/ { alias www/documents/; }`). / 매칭된 prefix를 대체하는 별칭 경로 (세그먼트 경계 검사). |
| `index` | `server`, `location` | `1+` (`file...`) | No | Index file candidates tried in order for directories (slash in name rejected). / 디렉토리 요청 시 순서대로 탐색할 인덱스 파일 목록. |
| `autoindex` | `server`, `location` | `1` (`on` \| `off`) | No | Enables/disables HTML directory listing when no index file exists. / 인덱스 파일이 없을 때 디렉토리 목록 생성 여부. |
| `client_max_body_size` | `server`, `location` | `1` (`size[k\|m]`) | No | Request body limit (`1` byte to `100m`, default `1m`). Exceeding triggers `413`. / 요청 본문 크기 상한 (기본 `1m`, 최대 `100m`). |
| `return` | `location` | `2` (`code target`) | No | Immediate redirect/status response before filesystem lookup (e.g., `return 301 /about.html;`). / 파일 탐색 전 즉시 리다이렉트·상태 응답. |
| `limit_except` | `location` | `1+` (`method... { deny all; }`) | No | Whitelists HTTP methods. **Locations without `limit_except` allow `GET` and `HEAD` only.** / 허용 메서드 지정 (미지정 시 `GET`, `HEAD`만 허용). |
| `upload_store` | `location` | `1` (`dir`) | No | Destination directory for `POST` `multipart/form-data` uploads (`201` / `409`) and `DELETE` (`204`). / 멀티파트 업로드 저장 및 삭제 대상 디렉토리. |
| `cgi` | `location` | `2` (`ext interpreter`) | Yes | Maps file extension (e.g., `.py`) to an absolute interpreter path (`/usr/bin/python3`). / 확장자별 CGI 인터프리터 절대 경로 매핑. |

---

## 3. Routing Precedence / 라우팅 처리 순서

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

## 4. Included Configuration Files / 포함된 설정 파일

| File | Purpose (EN) | 설명 (KR) |
| :--- | :--- | :--- |
| [conf/default.conf](file:///Users/yong/Projects/gdtknight/webserv/conf/default.conf) | Full demo on `127.0.0.1:8080` (`www/`) and a second server on `127.0.0.1:8081` (`www2/`): static files, autoindex, alias, 301 redirect, custom 404, 64B body limit, uploads, Python/PHP CGI. | 전체 기능 데모 (`8080` 메인 데모 + `8081` 보조 사이트). 정적/리다이렉트/업로드/CGI/본문 제한 포함. |
| [conf/ports.conf](file:///Users/yong/Projects/gdtknight/webserv/conf/ports.conf) | Multi-interface & multi-port setup (`127.0.0.1:8080`, `8081`, `127.0.0.2:8080`, `0.0.0.0:8082`). Note: `127.0.0.2` requires Linux loopback or `lo0` alias on macOS. | 다중 인터페이스/포트 바인딩 예제 (`127.0.0.2`는 Linux 루프백 또는 macOS `lo0` 별칭 필요). |
| [conf/same-port.conf](file:///Users/yong/Projects/gdtknight/webserv/conf/same-port.conf) | Negative test case with two `server` blocks sharing `127.0.0.1:8080`; rejected on startup because virtual hosts (`server_name`) are intentionally out of scope. | 동일 `ip:port` 중복 바인딩 검증용 (가상 호스트 미지원으로 시작 시 거부됨을 확인). |
