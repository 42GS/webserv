/**
 * @file  Limits.hpp
 * @brief Values to tune while testing (wiki analysis/design section 12).
 *        시험하며 조정하는 값 (위키 analysis/design 12절).
 *
 * Body size limits of the configuration are in config/ConfigLimits.hpp.
 * 설정의 본문 크기 상한은 config/ConfigLimits.hpp에 있다.
 */
#ifndef CORE_LIMITS_HPP
#define CORE_LIMITS_HPP

#include <cstddef>
#include <ctime>

/**
 * @brief Tunable limits, sizes and timeouts.
 *        조정할 수 있는 상한, 크기, 시간 제한.
 */
namespace limits {

/**
 * @brief Configuration file used without an argument.
 *        인자가 없을 때 쓰는 설정 파일.
 */
const char* const kDefaultConfigPath = "conf/default.conf";

/**
 * @brief Clock file, relative to the run directory.
 *        시계 파일, 실행 디렉토리 기준.
 */
const char* const kClockFile = ".webserv_clock";

/**
 * @brief Program name and an optional configuration file.
 *        프로그램 이름과 선택적 설정 파일.
 */
const int kMaxArgc = 2;

/**
 * @brief poll() timeout: how often timeouts are checked.
 *        poll() 대기 시간: 시간 초과를 검사하는 주기.
 */
const int kPollTimeoutMs = 1000;

/** Bytes read by one recv(). / recv() 한 번에 읽는 바이트. */
const std::size_t kRecvBufferSize = 65536;

/**
 * @brief Longer request line: 414 (RFC 9112 section 3 asks for at least 8000).
 *        더 긴 요청 줄은 414 (RFC 9112 3절은 8000 이상을 권한다).
 */
const std::size_t kMaxRequestLine = 8192;

/**
 * @brief Larger header section or trailer: 431.
 *        더 큰 헤더 영역이나 trailer는 431.
 */
const std::size_t kMaxHeaderBytes = 16384;
/** Longer chunk-size line: 400. / 더 긴 chunk-size 줄은 400. */
const std::size_t kMaxChunkLine = 4096;
/** Time allowed for the whole head. / 머리 전체에 주는 시간. */
const std::time_t kHeaderTimeoutSeconds = 30;
/** Body time allowed without input. / 본문 수신 중 입력 없이 기다리는 시간. */
const std::time_t kBodyTimeoutSeconds = 30;

/**
 * @brief Send time allowed without progress.
 *        응답 전송 중 진행 없이 기다리는 시간.
 */
const std::time_t kSendTimeoutSeconds = 30;

/**
 * @brief Wait for the first byte of a request, on a new or kept-alive
 *        connection; then it closes without a response.
 *        새 연결이나 유지한 연결에서 요청의 첫 바이트를 기다리는 시간. 넘으면
 *        응답 없이 닫는다.
 */
const std::time_t kKeepAliveTimeoutSeconds = 15;

/**
 * @brief Draining an unread request before closing: time allowed without
 *        input (concepts/connection-management).
 *        읽지 않은 요청을 닫기 전에 버릴 때, 입력 없이 기다리는 시간
 *        (concepts/connection-management).
 */
const std::time_t kDrainIdleSeconds = 1;
/** Total time of draining. / 버리기의 총 시간. */
const std::time_t kDrainTotalSeconds = 5;

/**
 * @brief Internal redirects per request, as NGINX (design decision 6).
 *        요청당 내부 리다이렉트 횟수, NGINX와 같다 (설계 결정 6).
 */
const int kMaxInternalRedirects = 10;

/**
 * @brief Files up to this size are read into memory, larger ones streamed.
 *        이 크기까지는 메모리로 읽고, 더 크면 나눠 보낸다.
 */
const std::size_t kInMemoryFileLimit = 1024 * 1024;
/** One read() of a streamed file. / 나눠 보내는 파일의 read() 한 번. */
const std::size_t kFileChunkSize = 65536;
/** One read() of a whole-file read. / 파일 전체를 읽을 때의 read() 한 번. */
const std::size_t kFileReadSize = 65536;

/**
 * @brief Largest configured error page that is used.
 *        사용할 수 있는 오류 페이지 파일의 최대 크기.
 */
const std::size_t kMaxErrorPageSize = 1024 * 1024;

}  // namespace limits

#endif
