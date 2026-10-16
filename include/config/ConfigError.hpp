/**
 * @file  ConfigError.hpp
 * @brief The exception for every configuration problem.
 *        모든 설정 오류에 쓰는 예외.
 */
#ifndef CONFIG_CONFIGERROR_HPP
#define CONFIG_CONFIGERROR_HPP

#include <stdexcept>
#include <string>

/**
 * @brief A configuration problem, reported as "source:line: message".
 *        "source:line: message" 형식으로 알리는 설정 오류.
 *
 * A separate type lets main() catch configuration errors alone and exit
 * with a message, while other exceptions take their own path.
 * 별도 타입이라 main()이 설정 오류만 잡아 메시지를 내고 끝낼 수 있고,
 * 다른 예외는 다른 경로로 간다.
 */
class ConfigError : public std::runtime_error {
 public:
  /**
   * @brief Builds the message from its parts.
   *        부분들로 메시지를 만든다.
   * @param[in] source  File name (or another name for the text).
   *                    파일 이름 (또는 텍스트를 가리키는 이름).
   * @param[in] line    Line number; 0 gives "source: message", for
   *                    problems such as a file that cannot be read.
   *                    줄 번호. 0이면 "source: message"가 된다 (파일을
   *                    읽을 수 없는 경우 등).
   * @param[in] message What is wrong. / 무엇이 잘못됐는지.
   */
  ConfigError(const std::string& source, int line, const std::string& message);
};

#endif
