# Changelog / 릴리스 노트

All notable milestones of **webserv** reconstructed from the commit history, organized for Git tags and GitHub Releases.
**webserv**의 커밋 히스토리를 기반으로 재구성한 주요 마일스톤 및 Git Tag / GitHub Release 기준 변경 이력입니다.

---

## [v1.0.0] — 2026-10-29 (Commit `4db9510`)

**Production-Ready Release: Observability, CGI Security Hardening & Demo Polish / 운영 관측성, CGI 보안 강화 및 최종 릴리스**

### Added / 추가
* **Access & Error Logging (`13982df`):** Per-response access log (`logs/access.log`) and lifecycle/timeout/CGI/accept failure error log (`logs/error.log`). / 요청별 접근 로그 및 서버·CGI·타임아웃·소켓 오류 로그 추가.
* **Benchmark Fixture (`5ca93a2`):** Added `www/empty.html` for `siege` stress testing. / `siege` 부하 테스트용 빈 페이지 추가.

### Security & Fixes / 보안 및 수정
* **CGI Header Hardening (`cad952e`):** Mitigated **httpoxy (`CVE-2016-5385`)** by dropping `Proxy`, blocked underscore (`_`) header spoofing, and stripped hop-by-hop headers from CGI responses. / `Proxy` 헤더 차단(`httpoxy` 방어), `_` 포함 헤더 스푸핑 방지, hop-by-hop 헤더 필터링.
* **Local Redirect Preservation (`8f8dc8a`):** Kept CGI client-redirect response bodies intact. / CGI 로컬·클라이언트 리다이렉트 시 문서 처리 보완.
* **Standard Portability (`664692b`):** Removed `std::auto_ptr` so the codebase builds cleanly both with `-std=c++98` and modern C++ compilers. / `std::auto_ptr` 제거로 `-std=c++98` 및 최신 C++ 표준 모두에서 빌드 호환성 확보.

---

## [v0.6.0] — 2026-10-18 (Commit `6b4ffc2`)

**Stability, Memory Safety & Demo Site (Steps 8–9) / 안정성·메모리 검증 및 데모 사이트**

### Stability & Performance / 안정성 및 성능
* **FD Exhaustion Backpressure (`6b4ffc2`):** Paused `Listener` polling when `accept()` fails on a full descriptor table and resumed on connection close or tick, eliminating 100% CPU busy-spin. / FD 고갈 시 `Listener` 폴링을 일시 정지해 CPU 스핀 방지.
* **Zero-copy CGI Handoff (`6b4ffc2`):** Returned collected CGI output via `std::string::swap` to eliminate multi-megabyte buffer copies. / 대용량 CGI 출력 전달 시 불필요한 메모리 복사 제거.
* **Memory & Bounds Audit (`d4ce794`):** Verified zero leaks and clean signal shutdowns under **Valgrind** and **Electric Fence**; completed step 9 interactive demo site (`83f413c`). / Valgrind·Electric Fence 검증 및 데모 페이지(`www/`) 구축.

---

## [v0.5.0] — 2026-10-18 (Commit `1655980`)

**Asynchronous CGI/1.1 Execution (Step 7) / 비동기 CGI/1.1 실행 엔진**

### Added / 추가
* **Non-blocking CGI Pipeline:** Spawned CGI scripts (`fork`/`execve`) in the script's own directory with RFC 3875 meta-variables (`CgiEnv`), polling stdin/stdout pipes (`CgiPipes`) inside the main `poll()` loop. / 메인 `poll()` 루프에서 논블로킹 파이프로 CGI 입출력 다중화.
* **Timeout & Zombie Prevention:** Enforced `10s` CGI timeout (`504 Gateway Timeout`), `502 Bad Gateway` on malformed output, and non-blocking `waitpid(WNOHANG)` collection via `ChildReaper`. / CGI 타임아웃(`504`), 비정상 종료(`502`), `ChildReaper` 기반 좀비 프로세스 회수.

---

## [v0.4.0] — 2026-10-17 (Commit `3385e7e`)

**File Uploads & Deletion (Step 6) / 멀티파트 파일 업로드 및 삭제**

### Added / 추가
* **`multipart/form-data` Upload (`bb13b56`, `ea1b330`):** Linear-time multipart boundary parser (RFC 7578 / 2046) writing uploaded files to `upload_store` (`201 Created`, `409 Conflict` naming the conflicting file `8664a05`). / 선형 시간 멀티파트 파서 및 업로드(`201`/`409`) 구현.
* **`DELETE` Method (`bb13b56`):** Safe deletion of uploaded resources (`204 No Content`) restricted by `limit_except`. / `limit_except` 기반 파일 삭제(`204`) 지원.
* **Abuse Prevention (`ea1b330`):** Capped uploaded file count per request (`kMaxUploadFiles = 5`) to protect the single-threaded event loop. / 요청당 업로드 파일 수 제한으로 이벤트 루프 블로킹 방지.

---

## [v0.3.0] — 2026-10-17 (Commit `870f06a`)

**Persistent Connections, Pipelining & Timeouts (Step 5) / 연결 유지, 파이프라이닝 및 타임아웃**

### Added / 추가
* **HTTP/1.1 Connection Lifecycle:** Implemented `keep-alive`, HTTP pipelining over buffered `inBuffer_`, and `DRAIN` state (RFC 9112 §9.6) to avoid TCP `RST` erasing error responses. / Keep-Alive, 파이프라이닝, TCP RST 방지용 `DRAIN` 상태 구현.
* **Per-state Timeouts:** Enforced separate thresholds for idle keep-alive (`15s`), head completion (`30s`), body progress (`30s`), send stall (`30s`), and draining (`1s` idle / `5s` total). / 상태별 세분화 타임아웃 적용.

---

## [v0.2.0] — 2026-10-16 (Commit `7404b94`)

**Incremental HTTP/1.1 Parser, Router & Static Serving (Steps 3–4) / 증분 HTTP/1.1 파서, 라우팅 및 정적 파일 서빙**

### Added / 추가
* **Incremental Parser (`98cb1e2`, `24fd810`):** Cursor-based HTTP/1.1 parser supporting arbitrary TCP chunk splits, `Content-Length`, `Transfer-Encoding: chunked`, and size limits (`413`, `414`, `431`). / TCP 분할 수신 및 청크 인코딩을 지원하는 증분 파서.
* **Router & Path Security (`fd5c868`, `4922eb8`):** Percent-decoding, dot-segment resolution, traversal rejection (`400`), `root`/`alias`, `return` redirects, `limit_except`, and `HEAD` handling. / 경로 정규화, 탈출 차단(`400`), 최장 접두사 라우팅.
* **Static Streaming & Listings (`40cb5e8`, `7404b94`):** `ResponseStream` chunked file reads (`64 KiB`), MIME types, IMF-fixdate `Date` header, `autoindex` HTML listings, and custom `error_page`. / 대용량 파일 스트리밍, 디렉토리 목록, 커스텀 에러 페이지.

---

## [v0.1.0] — 2026-10-16 (Commit `6f48721`)

**Core `poll()` Event Loop & NGINX-style Config (Steps 1–2) / 코어 이벤트 루프 및 설정 파서**

### Added / 추가
* **Configuration Subsystem (`8f4ed3b`, `855f851`, `e58067d`):** Lexer, syntax parser with nesting depth limit, and table-driven `DirectiveRules` for `server` and `location` blocks. / NGINX 스타일 설정 렉서·파서 및 테이블 기반 지시어 검증.
* **Event Loop & Listener (`6f48721`, `9854f52`):** Single-process `poll()` multiplexer (`EventLoop`), non-blocking listening sockets, and `stat()`-based `Clock`. / 단일 `poll()` 이벤트 루프, 논블로킹 리스너, 파일 `mtime` 기반 시계.
* **Test Harnesses (`d1761bd`, `4b68ef5`):** C++98 unit-test runner and Python standard-library TCP integration harness. / C++98 단위 테스트 및 Python TCP 통합 테스트 프레임워크 구축.
