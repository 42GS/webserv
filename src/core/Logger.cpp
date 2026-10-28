/**
 * @file  Logger.cpp
 * @brief Access and error log files.
 *        접근 로그와 오류 로그 파일.
 */
#include "core/Logger.hpp"

#include <fcntl.h>
#include <unistd.h>

#include "core/HttpDate.hpp"

namespace {

/** Written in place of a control character. / 제어 문자 대신 쓰는 글자. */
const char kReplacement = '?';
/** DEL, the control character after "~". / "~" 다음의 제어 문자 DEL. */
const unsigned char kDeleteChar = 0x7f;

/**
 * @brief The text with each control character replaced by kReplacement.
 *        제어 문자를 kReplacement로 바꾼 텍스트.
 * @param[in] text Text to log. / 남길 텍스트.
 * @return One line of printable text. / 출력 가능한 글자로 된 한 줄.
 */
std::string printable(const std::string& text) {
  std::string line(text);
  for (std::string::size_type i = 0; i < line.size(); ++i) {
    const unsigned char c = static_cast<unsigned char>(line[i]);
    if (c < ' ' || c == kDeleteChar) line[i] = kReplacement;
  }
  return line;
}

/**
 * @brief Opens a file for appending, creating it if needed.
 *        파일을 덧붙이기로 연다. 없으면 만든다.
 * @param[in] path File. / 파일.
 * @return The descriptor, or -1. / 디스크립터, 또는 -1.
 */
int openForAppend(const std::string& path) {
  return ::open(path.c_str(), O_WRONLY | O_CREAT | O_APPEND,
                0644);  // rw-r--r--
}

}  // namespace

Logger::Logger() : accessFd_(-1), errorFd_(-1) {}

Logger::~Logger() { close(); }

bool Logger::open(const std::string& accessPath, const std::string& errorPath) {
  close();
  accessFd_ = openForAppend(accessPath);
  errorFd_ = openForAppend(errorPath);
  if (accessFd_ >= 0 && errorFd_ >= 0) return true;
  close();
  return false;
}

void Logger::access(std::time_t now, const std::string& entry) const {
  writeLine(accessFd_, now, entry);
}

void Logger::error(std::time_t now, const std::string& message) const {
  writeLine(errorFd_, now, message);
}

// A descriptor that was never opened is -1; close() errors leave nothing
// to do.
// 열지 않은 디스크립터는 -1이다. close()가 실패해도 할 일이 없다.
void Logger::close() {
  if (accessFd_ >= 0) ::close(accessFd_);
  if (errorFd_ >= 0) ::close(errorFd_);
  accessFd_ = -1;
  errorFd_ = -1;
}

// One write() per line, so with O_APPEND a line is not split by another
// writer of the file. A failed or short write is not retried: the server
// has nowhere else to report it, and serving goes on without the line.
// 줄마다 write() 한 번이라, O_APPEND와 함께 다른 쓰기에 줄이 나뉘지 않는다.
// 실패하거나 덜 쓴 write()는 다시 하지 않는다. 서버가 그것을 알릴 다른
// 곳이 없고, 그 줄 없이 서비스를 계속한다.
void Logger::writeLine(int fd, std::time_t now, const std::string& text) {
  if (fd < 0) return;
  const std::string line =
      "[" + formatHttpDate(now) + "] " + printable(text) + "\n";
  static_cast<void>(write(fd, line.data(), line.size()));
}
