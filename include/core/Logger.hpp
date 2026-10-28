/**
 * @file  Logger.hpp
 * @brief Access and error log files.
 *        접근 로그와 오류 로그 파일.
 */
#ifndef CORE_LOGGER_HPP
#define CORE_LOGGER_HPP

#include <ctime>
#include <string>

/**
 * @brief Appends one dated line per call to the access or the error log.
 *        호출마다 접근 로그나 오류 로그에 날짜가 붙은 줄 하나를 덧붙인다.
 *
 * The logs are regular files, which need no poll() (raw/subject.md:134),
 * unlike stdout or stderr, which may be a pipe (design 11). Control
 * characters, which a client can put into a request, are written as "?"
 * so that one call stays one line.
 * 로그는 일반 파일이라 poll()이 필요 없다 (raw/subject.md:134). 표준
 * 출력·에러는 pipe일 수 있어 다르다 (설계 11절). 클라이언트가 요청에 넣을
 * 수 있는 제어 문자는 "?"로 써서, 호출 하나가 한 줄로 남게 한다.
 */
class Logger {
 public:
  /**
   * @brief A logger without files; it writes nothing until open().
   *        파일이 없는 logger. open() 전에는 아무것도 쓰지 않는다.
   */
  Logger();

  /**
   * @brief Closes the files.
   *        파일을 닫는다.
   */
  ~Logger();

  /**
   * @brief Opens both files for appending, creating them if needed.
   *        두 파일을 덧붙이기로 연다. 없으면 만든다.
   * @param[in] accessPath Access log. / 접근 로그.
   * @param[in] errorPath  Error log. / 오류 로그.
   * @return False if either cannot be opened; then nothing is logged.
   *         어느 하나라도 열 수 없으면 false. 그러면 아무것도 남기지 않는다.
   */
  bool open(const std::string& accessPath, const std::string& errorPath);

  /**
   * @brief Logs a response that is being sent.
   *        보내기 시작한 응답을 남긴다.
   * @param[in] now   Current time. / 현재 시각.
   * @param[in] entry Client, request line, status and size.
   *                  클라이언트, 요청 줄, 상태, 크기.
   */
  void access(std::time_t now, const std::string& entry) const;

  /**
   * @brief Logs an event that an operator may need to act on.
   *        운영자가 조치해야 할 수 있는 사건을 남긴다.
   * @param[in] now     Current time. / 현재 시각.
   * @param[in] message What happened. / 일어난 일.
   */
  void error(std::time_t now, const std::string& message) const;

 private:
  /// @cond
  // Not copyable: declared and never defined (C++98 has no "= delete").
  // 복사하지 않는다: 선언만 하고 정의하지 않는다 (C++98에는 "= delete"가 없다).
  Logger(const Logger&);
  Logger& operator=(const Logger&);
  /// @endcond

  /**
   * @brief Closes both files; the logger writes nothing afterwards.
   *        두 파일을 닫는다. 그 뒤로 logger는 아무것도 쓰지 않는다.
   */
  void close();

  /**
   * @brief Writes "[date] text" and a line end to a file.
   *        파일에 "[날짜] text"와 줄 끝을 쓴다.
   * @param[in] fd   Log file, or -1 to skip. / 로그 파일. -1이면 건너뛴다.
   * @param[in] now  Current time. / 현재 시각.
   * @param[in] text Line content. / 줄 내용.
   */
  static void writeLine(int fd, std::time_t now, const std::string& text);

  /** Access log, or -1. / 접근 로그, 또는 -1. */
  int accessFd_;
  /** Error log, or -1. / 오류 로그, 또는 -1. */
  int errorFd_;
};

#endif
