**한국어** | [English](configuration.en.md) · [README로 돌아가기](../README.md)

# 설정 파일 레퍼런스

`webserv`에서 쓰는 NGINX 스타일 설정 파일의 파싱 순서, 지시어 목록, 라우팅 규칙을 정리했다.

---

## 1. 설정 읽는 순서

설정 로딩(`src/config/`)은 소켓을 열기 전에 끝낸다. 문제가 있으면 줄 번호가 들어간 `ConfigError`를 내고 바로 종료한다.

1. **`ConfigLexer`:** 단어, `{`, `}`, `;`, 따옴표 문자열, `#` 주석을 토큰으로 나누고 줄 번호를 기록한다.
2. **`ConfigSyntax`:** 중괄호 짝, 세미콜론, 블록 중첩 깊이(`e58067d`)를 확인한다. 중첩이 너무 깊으면 파서가 터지기 전에 막는다.
3. **`DirectiveRules` (`src/config/DirectiveRules.cpp`):** 지시어 규칙 표를 보고 `server`/`location` 위치가 맞는지, 인자 수가 맞는지, 여러 번 써도 되는지 확인한다.
4. **`ServerBuilder` / `LocationBuilder`:** `server` 설정을 `location`으로 물려주고 IPv4·포트·본문 상한을 검증한다. 마지막으로 라우팅이 일관되게 되도록 `location`을 **접두사가 긴 순서**대로 정렬한다.

---

## 2. 지원하는 지시어

`src/config/DirectiveRules.cpp`에 들어 있는 지시어 12개다.

| 지시어 | 쓸 수 있는 블록 | 인자 수 | 중복 선언 | 설명 |
| :--- | :--- | :--- | :--- | :--- |
| `listen` | `server` | `1` (`[ip:]port`) | 가능 | 포트는 꼭 적어야 한다 (`127.0.0.1:8080` 또는 `8080`). 서버끼리 주소:포트가 겹치면 거부한다. |
| `error_page` | `server` | `2+` (`code... uri`) | 가능 | 상태 코드별 커스텀 오류 페이지 URI를 지정한다 (최대 `1 MiB`). |
| `location` | `server` | `1` (`prefix`) | 가능 | 접두사 기반 라우트 블록이다. `=` 같은 modifier를 쓰면 오류 메시지로 알려 주고 거부한다. |
| `root` | `server`, `location` | `1` (`path`) | 불가 | 정규화된 URI 경로를 이어 붙일 디렉터리다 (`root www` + `/a.txt` → `www/a.txt`). |
| `alias` | `location` | `1` (`path`) | 불가 | 매칭된 `location` 접두사 부분을 다른 경로로 바꾼다 (`location /docs/ { alias www/documents/; }`). |
| `index` | `server`, `location` | `1+` (`file...`) | 불가 | 디렉터리 요청 시 순서대로 찾아볼 파일 이름이다 (이름 안에 `/`가 들어가면 안 된다). |
| `autoindex` | `server`, `location` | `1` (`on` \| `off`) | 불가 | 인덱스 파일이 없을 때 디렉터리 목록 HTML을 만들어 줄지 정한다. |
| `client_max_body_size` | `server`, `location` | `1` (`size[k\|m]`) | 불가 | 요청 본문 크기 제한이다 (`1`바이트 ~ `100m`, 기본값 `1m`). 넘으면 `413`을 보낸다. |
| `return` | `location` | `2` (`code target`) | 불가 | 파일을 찾아보기 전에 바로 리다이렉트나 상태 코드로 답한다 (`return 301 /about.html;`). |
| `limit_except` | `location` | `1+` (`method... { deny all; }`) | 불가 | 허용할 HTTP 메서드를 고른다. **`limit_except`를 안 적으면 `GET`과 `HEAD`만 받는다.** |
| `upload_store` | `location` | `1` (`dir`) | 불가 | `POST` `multipart/form-data` 업로드(`201`/`409`)와 `DELETE`(`204`)를 처리할 디렉터리다. |
| `cgi` | `location` | `2` (`ext interpreter`) | 가능 | 파일 확장자(예: `.py`)와 실행할 프로그램 절대 경로(`/usr/bin/python3`)를 연결한다. |

---

## 3. 라우팅 판단 순서

`Router::route()`(`include/route/Router.hpp`)는 요청이 들어오면 아래 순서로 처리한다.

1. **경로 정규화 (`PathResolver`):** 퍼센트 인코딩을 한 번 풀고, `//`를 합치고, `.`과 `..`을 계산한다. `/` 밖으로 나가려고 하면 `400 Bad Request`다.
2. **Location 고르기 (`matchLocation`):** 가장 긴 접두사가 맞는 `location`을 고른다 (`alias`는 세그먼트 경계가 딱 맞을 때만 고른다). 맞는 게 없으면 `404`다.
3. **`return` 확인:** `return` 지시어가 있으면 바로 그 응답을 돌려준다.
4. **메서드 확인 (`limit_except`):** 허용 목록에 없으면 `Allow` 헤더를 붙여서 `405 Method Not Allowed`를 보낸다 (구현하지 않은 메서드는 `501`).
5. **실제 처리 고르기:**
   * `upload_store`가 있는 곳으로 온 `multipart/form-data` `POST` → `UploadHandler` (`201 Created`, 이미 같은 파일이 있으면 `409 Conflict`)
   * `upload_store`가 있는 곳으로 온 `DELETE` → `DeleteHandler` (`204 No Content`)
   * `cgi` 확장자와 맞는 파일 → `CgiProcess` (`PATH_INFO`와 `PATH_TRANSLATED`도 같이 계산한다)
   * 일반 파일 → `StaticHandler` (`1 MiB`까지는 메모리로 읽고, 넘으면 `64 KiB`씩 나눠 보낸다)
   * 디렉터리 → 끝에 `/`가 없으면 `301`로 붙여서 보내고, `index` 파일을 찾는다 (내부 리다이렉트는 최대 `10`번). 없으면 `autoindex` 목록을 만들거나 `403`을 보낸다.

---

## 4. 포함된 설정 파일 예제

| 파일 | 설명 |
| :--- | :--- |
| [conf/default.conf](../conf/default.conf) | `127.0.0.1:8080`(`www/`) 메인 데모와 `127.0.0.1:8081`(`www2/`) 두 번째 사이트. 정적 파일, 목록, alias, 301, 커스텀 404, 64B 본문 제한, 업로드, Python/PHP CGI가 다 들어 있다. |
| [conf/ports.conf](../conf/ports.conf) | 여러 IP와 포트에 바인딩하는 예제 (`127.0.0.2`는 리눅스나 macOS `lo0` 별칭 설정이 필요하다). |
| [conf/same-port.conf](../conf/same-port.conf) | 같은 `ip:port`에 서버 블록 두 개를 넣은 실패 테스트용 설정이다. 가상 호스트(`server_name`)는 지원하지 않으므로 시작할 때 거부된다. |
