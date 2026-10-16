/**
 * @file  ConfigError.cpp
 * @brief Message formatting of ConfigError.
 *        ConfigError의 메시지 형식.
 */
#include "config/ConfigError.hpp"

#include <sstream>

namespace {

/**
 * @brief Builds "source:line: message", or "source: message" for line 0.
 *        "source:line: message"를 만든다. line이 0이면 "source: message".
 *
 * A function, because the base class must be initialized with the
 * finished text.
 * 기반 클래스를 완성된 문자열로 초기화해야 해서 함수로 뺐다.
 *
 * @param[in] source  Name of the text. / 텍스트 이름.
 * @param[in] line    Line number, 0 for none. / 줄 번호, 없으면 0.
 * @param[in] message Description. / 설명.
 * @return The message. / 완성된 메시지.
 */
std::string formatMessage(const std::string& source, int line,
                          const std::string& message) {
  std::ostringstream out;
  out << source;
  if (line > 0) out << ":" << line;
  out << ": " << message;
  return out.str();
}

}  // namespace

ConfigError::ConfigError(const std::string& source, int line,
                         const std::string& message)
    : std::runtime_error(formatMessage(source, line, message)) {}
