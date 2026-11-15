[English](configuration.md) | **한국어** · [README로 돌아가기](../README.ko.md)

# 설정 파일 레퍼런스

> **webserv**의 NGINX 스타일 설정 문법, 지시어 레퍼런스, 라우팅 우선순위 및 데모 설정 안내입니다.

---

## 1. 설정 파싱 파이프라인

설정 로딩(`src/config/`)은 소켓을 열기 전 시작 단계에서 엄격하게 수행되며, 오류 발생 시 줄 번호가 포함된 `ConfigError` 진단 메시지와 함께 즉시 종료됩니다.

1. **`ConfigLexer`:** 줄 번호를 추적하며 토큰, `{`, `}`, `;`, 따옴표 문자열, `#` 주석을 분리합니다.
2. **`ConfigSyntax`:** 중괄호 짝, 세미콜론 종결, 블록 중첩 깊이 상한(`e58067d` — 깊은 중첩으로 인한 스택 고갈 방지)을 검증합니다.
3. **`DirectiveRules` (`src/config/DirectiveRules.cpp`):** 테이블 기반 검증으로 허용 블록(`server` / `location`), 인자 개수 범위, 블록/세미콜론 종결, 중복 선언 가능 여부를 확인합니다.
4. **`ServerBuilder` / `LocationBuilder`:** `server` → `location` 설정 상속, IPv4/포트 쌍 및 본문 크기 제한 검증을 수행하고, 결정적 라우팅을 위해 `location` 목록을 **최장 접두사(Longest-prefix) 순**으로 정렬합니다.

---

## 2. 지시어 레퍼런스

`src/config/DirectiveRules.cpp`에 정의된 전체 12개 지원 지시어입니다.

| 지시어 | 허용 블록 | 인자 수 | 중복 가능 | 설명 |
| :--- | :--- | :--- | :--- | :--- |
| `listen` | `server` | `1` (`[ip:]port`) | 예 | 포트 명시 필수 (`127.0.0.1:8080` 또는 `8080`). 서버 간 중복/충돌 바인드는 거부됩니다. |
| `error_page` | `server` | `2+` (`code... uri`) | 예 | 오류 코드별 커스텀 에러 페이지 URI를 지정합니다 (내부 라우팅 처리, 최대 페이지 크기 `1 MiB`). |
| `location` | `server` | `1` (`prefix`) | 예 | 접두사 기반 라우트 블록입니다 (`=` 같은 modifier는 오류 메시지로 거부). |
| `root` | `server`, `location` | `1` (`path`) | 아니오 | 정규화된 URI 경로를 이어 붙일 루트 디렉터리 (`root www` + `/a.txt` → `www/a.txt`). |
| `alias` | `location` | `1` (`path`) | 아니오 | 매칭된 `location` 접두사를 세그먼트 경계에서 대체합니다 (`location /docs/ { alias www/documents/; }`). |
| `index` | `server`, `location` | `1+` (`file...`) | 아니오 | 디렉터리 요청 시 순서대로 탐색할 인덱스 파일 목록 (파일명 내 `/` 금지). |
| `autoindex` | `server`, `location` | `1` (`on` \| `off`) | 아니오 | 인덱스 파일이 없을 때 HTML 디렉터리 목록 생성 여부를 설정합니다. |
| `client_max_body_size` | `server`, `location` | `1` (`size[k\|m]`) | 아니오 | 요청 본문 크기 상한 (`1`바이트 ~ `100m`, 기본 `1m`). 초과 시 `413` 반환. |
| `return` | `location` | `2` (`code target`) | 아니오 | 파일 시스템 탐색 전 즉시 리다이렉트 또는 상태 응답을 반환합니다 (`return 301 /about.html;`). |
| `limit_except` | `location` | `1+` (`method... { deny all; }`) | 아니오 | 허용 HTTP 메서드를 지정합니다. **`limit_except`가 없는 location은 `GET`, `HEAD`만 허용합니다.** |
| `upload_store` | `location` | `1` (`dir`) | 아니오 | `POST` `multipart/form-data` 업로드(`201` / `409`) 및 `DELETE`(`204`) 대상 디렉터리. |
| `cgi` | `location` | `2` (`ext interpreter`) | 예 | 파일 확장자(예: `.py`)와 CGI 인터프리터 절대 경로(`/usr/bin/python3`)를 매핑합니다. |

---

## 3. 라우팅 처리 순서

`Router::route()`(`include/route/Router.hpp`)는 모든 요청을 아래 순서대로 평가합니다.

1. **경로 정규화 (`PathResolver`):** 퍼센트 인코딩을 한 번 디코딩하고, `//` 병합 및 `.` / `..`을 해석합니다. `/` 상위로 탈출하는 경로는 `400 Bad Request`를 반환합니다.
2. **Location 선택 (`matchLocation`):** 가장 길게 일치하는 접두사 location을 선택합니다 (`alias`는 세그먼트 경계 일치 필수). 매칭 실패 시 `404`를 반환합니다.
3. **`return` 지시어:** 설정된 경우 즉시 리다이렉트/상태 코드로 응답합니다.
4. **메서드 검사 (`limit_except`):** 허용되지 않은 메서드는 `Allow` 헤더와 함께 `405 Method Not Allowed`를 반환합니다 (미구현 메서드는 `501`).
5. **CGI / 업로드 / 정적 파일 분기:**
   * `upload_store`가 설정된 location의 `multipart/form-data` `POST` → `UploadHandler` (`201 Created`, 중복 시 `409 Conflict`).
   * `upload_store`가 설정된 location의 `DELETE` → `DeleteHandler` (`204 No Content`).
   * `cgi` 지시어와 일치하는 확장자 → `CgiProcess` (`PATH_INFO`, `PATH_TRANSLATED` 추출).
   * 일반 파일 → `StaticHandler` (`1 MiB` 이하 메모리 응답, 초과 시 `64 KiB` 청크 스트리밍).
   * 디렉터리 → 끝 `/` 누락 시 `301` 리다이렉트, `index` 파일 탐색(내부 리다이렉트 최대 `10`회), 없으면 `autoindex` 또는 `403`.

---

## 4. 포함된 설정 파일

| 파일 | 설명 |
| :--- | :--- |
| [conf/default.conf](../conf/default.conf) | `127.0.0.1:8080`(`www/`) 메인 데모 및 `127.0.0.1:8081`(`www2/`) 보조 사이트. 정적 파일, 오토인덱스, alias, 301 리다이렉트, 커스텀 404, 64B 본문 제한, 업로드, Python/PHP CGI 포함. |
| [conf/ports.conf](../conf/ports.conf) | 다중 인터페이스·포트 바인딩 예제 (`127.0.0.1:8080`, `8081`, `127.0.0.2:8080`, `0.0.0.0:8082`). 참고: `127.0.0.2`는 Linux 루프백 또는 macOS `lo0` 별칭 필요. |
| [conf/same-port.conf](../conf/same-port.conf) | 동일 `ip:port`를 공유하는 두 `server` 블록 테스트용. 가상 호스트(`server_name`) 미지원으로 시작 시 거부됨을 확인합니다. |
