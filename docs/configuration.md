**한국어** | [English](configuration.en.md) · [README로 돌아가기](../README.md)

# 설정 파일 레퍼런스

`webserv`가 NGINX 스타일 설정 파일을 어떻게 읽고 검증하는지, 그리고 어떤 순서로 요청을 라우팅하는지 정리했다.

---

## 1. 설정 읽는 순서

설정 로딩(`src/config/`)은 소켓을 열기 전에 끝낸다. 문법이나 값에 문제가 있으면 줄 번호가 들어간 `ConfigError`를 내고 바로 종료한다.

1. `ConfigLexer.cpp`의 `tokenizeConfig()`가 단어, `{`, `}`, `;`, `#` 주석을 토큰으로 나누고 줄 번호를 기록한다. 따옴표(`"` 또는 `'`)는 지원하지 않으며 만나면 바로 `ConfigError`를 낸다.
2. `ConfigSyntax.cpp`의 `parseDirectives()`(`SyntaxParser`)가 중괄호 짝, 세미콜론 종결, 블록 중첩 깊이(`kMaxBlockDepth = 3`, `e58067d`)를 확인한다. 중첩이 너무 깊으면 재귀 파서가 스택을 다 쓰기 전에 막는다.
3. `DirectiveRules.cpp`의 `BlockChecker`(`kRules`)가 지시어 규칙 표를 보고 `server`/`location` 위치가 맞는지, 인자 수가 맞는지, 중복 선언이 가능한지 확인한다.
4. `ServerBuilder.cpp`와 `LocationBuilder.cpp`가 `server` 기본값을 `location`으로 물려주고(`root`가 있고 `location /`가 없으면 암시적 `location /`도 생성), IPv4·포트·본문 상한을 검증한다. 마지막으로 라우팅이 일관되게 되도록 `location` 목록을 접두사가 긴 순서로 정렬한다.

---

## 2. 지원하는 지시어

`src/config/DirectiveRules.cpp`에 정의된 지시어 12개다.

| 지시어 | 쓸 수 있는 블록 | 인자 수 | 중복 선언 | 설명 |
| :--- | :--- | :--- | :--- | :--- |
| `listen` | `server` | `1` (`[ip:]port`) | 가능 | 포트는 꼭 적어야 한다 (`127.0.0.1:8080` 또는 `8080`). 서버끼리 주소:포트가 겹치면 거부한다. |
| `error_page` | `server` | `2+` (`code... uri`) | 가능 | `300..599` 상태 코드별 커스텀 오류 페이지 URI(`/`로 시작)를 지정한다 (최대 `1 MiB`). |
| `location` | `server` | `1` (`prefix`) | 가능 | 접두사 기반 라우트 블록이다. `=` 같은 modifier를 쓰면 오류 메시지로 알려 주고 거부한다. |
| `root` | `server`, `location` | `1` (`path`) | 불가 | 정규화된 URI 경로를 이어 붙일 디렉터리다 (`root www` + `/a.txt` → `www/a.txt`). |
| `alias` | `location` | `1` (`path`) | 불가 | 매칭된 `location` 접두사 부분을 다른 경로로 바꾼다 (`location /docs/ { alias www/documents/; }`). |
| `index` | `server`, `location` | `1+` (`file...`) | 불가 | 디렉터리 요청 시 순서대로 찾아볼 파일 이름이다 (이름 안에 `/`가 들어가면 안 된다). |
| `autoindex` | `server`, `location` | `1` (`on` \| `off`) | 불가 | 인덱스 파일이 없을 때 디렉터리 목록 HTML을 만들어 줄지 정한다. |
| `client_max_body_size` | `server`, `location` | `1` (`size[k\|m]`) | 불가 | 요청 본문 크기 제한이다 (`1`바이트 ~ `100m`, 기본값 `1m`). 넘으면 `413`을 보낸다. |
| `return` | `location` | `2` (`code target`) | 불가 | 파일을 찾아보기 전에 바로 리다이렉트 응답을 보낸다 (`301, 302, 303, 307, 308` 코드만 허용). |
| `limit_except` | `location` | `1+` (`method... { deny all; }`) | 불가 | 허용할 HTTP 메서드를 고른다 (`GET`을 허용하면 `HEAD`도 함께 열린다). **`limit_except`를 안 적으면 `GET`과 `HEAD`만 받는다.** |
| `upload_store` | `location` | `1` (`dir`) | 불가 | `POST` 업로드 파일을 저장할 디렉터리다 (`DELETE`는 `upload_store`가 아니라 해당 location의 `root`/`alias` 경로를 쓴다). |
| `cgi` | `location` | `2` (`ext interpreter`) | 가능 | 파일 확장자(예: `.py`)와 실행할 프로그램 절대 경로(`/usr/bin/python3`)를 연결한다. |

---

## 3. 라우팅 판단 순서

요청이 들어오면 `Router::route()`와 `Router::routePath()`(`src/route/Router.cpp`)가 아래 순서로 처리 방법을 고른다.

1. `PathResolver.cpp`의 `normalizePath()`가 퍼센트 인코딩을 한 번 풀고, `//`를 합치고, `.`과 `..`을 정리한다. `/` 밖으로 나가려고 하면 `400 Bad Request`다.
2. `matchLocation()`이 가장 긴 접두사가 맞는 `location`을 고른다 (`alias`는 세그먼트 경계가 딱 맞을 때만 고른다). 맞는 게 없으면 `404`를 반환한다.
3. `return` 지시어가 있으면 파일 탐색 없이 바로 해당 리다이렉트 응답을 돌려준다.
4. `findScript()`로 경로 앞부분에 `cgi` 확장자를 가진 일반 파일이 있는지 먼저 찾는다. 스크립트가 있으면 `limit_except`(`allows()`) 허용 여부만 검사해 통과 시 `PATH_INFO`·`PATH_TRANSLATED`를 채워 `CgiProcess`로 넘기고, 불허 시 `Allow` 헤더와 함께 `405`를 보낸다. (스크립트는 `upload_store`가 없어도 허용된 `POST` 등을 직접 받는다.)
5. CGI가 아닌 경로는 `answers()`로 `limit_except` 허용 여부와 함께 `POST`일 때 `upload_store`가 있는지도 확인한다 (미충족 시 `405`, 미구현 메서드는 요청 파서에서 `501`).
6. 위 검사를 통과하면 메서드와 파일 종류에 따라 분기한다.
   * `POST` 요청은 `UploadHandler`(`storeUpload()`)로 간다. `Content-Type`이 `multipart/form-data`가 아니면 `415`, 저장 성공 시 `201`, 같은 파일명이 있으면 `409`를 반환한다.
   * `DELETE` 요청은 location의 `root`/`alias`로 매핑한 경로가 일반 파일이고 상위 디렉터리를 수정할 수 있으면 `DeleteHandler`(`std::remove`)로 지우고 `204`를 보낸다 (없으면 `404`, 디렉터리나 권한 부족이면 `403`).
   * `GET`/`HEAD` 요청에서 대상이 디렉터리면 끝에 `/`가 없을 때 `301`로 붙여서 보내고, 있으면 `index` 파일을 순서대로 찾아 다시 `routePath()`를 태운다 (내부 리다이렉트 최대 `10`번, 초과 시 `500`). 인덱스 파일이 없으면 `autoindex` 목록을 만들거나 `403`을 보낸다.
   * 읽을 수 있는 일반 파일은 `StaticHandler`가 처리한다 (`1 MiB`까지는 메모리로 읽고, 넘으면 `64 KiB`씩 나눠 보낸다). 장치 파일·FIFO나 읽기 권한이 없는 파일은 `403`이다.

---

## 4. 포함된 설정 파일 예제

| 파일 | 설명 |
| :--- | :--- |
| [conf/default.conf](../conf/default.conf) | `127.0.0.1:8080`(`www/`) 메인 데모와 `127.0.0.1:8081`(`www2/`) 보조 사이트. 정적 파일, 목록, alias, 301, 커스텀 404, 64B 본문 제한, 업로드, Python/PHP CGI가 다 들어 있다. |
| [conf/ports.conf](../conf/ports.conf) | 여러 IP와 포트에 바인딩하는 예제 (`127.0.0.2`는 리눅스나 macOS `lo0` 별칭 설정이 필요하다). |
| [conf/same-port.conf](../conf/same-port.conf) | 같은 `ip:port`에 서버 블록 두 개를 넣은 실패 테스트용 설정이다. 가상 호스트(`server_name`)는 지원하지 않으므로 시작할 때 거부된다. |
