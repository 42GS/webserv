/**
 * @file  CgiPipes.hpp
 * @brief The two pipes to a CGI script, driven by poll().
 *        CGI 스크립트와 잇는 두 pipe. poll()로 움직인다.
 */
#ifndef CGI_CGIPIPES_HPP
#define CGI_CGIPIPES_HPP

#include <cstddef>
#include <string>

#include "core/IoHandler.hpp"

/**
 * @brief The script's standard input: writes the request body.
 *        스크립트의 표준 입력: 요청 본문을 쓴다.
 *
 * One write() per readiness. It stops at the end of the body, or when
 * the script no longer reads: POLLERR and POLLHUP are checked first,
 * because Ubuntu also sets POLLOUT then (wiki concepts/cgi section 1).
 * 준비될 때마다 write() 한 번. 본문 끝에서, 또는 스크립트가 더 읽지 않을
 * 때 멈춘다. Ubuntu는 그때 POLLOUT도 켜므로 POLLERR·POLLHUP을 먼저 본다
 * (위키 concepts/cgi 1절).
 */
class CgiStdin : public IoHandler {
 public:
  /**
   * @brief Writes @p body to the write end @p fd.
   *        쓰는 쪽 @p fd에 @p body를 쓴다.
   * @param[in] fd       Non-blocking write end, now owned.
   *                     non-blocking 쓰는 쪽. 이제 소유한다.
   * @param[in] body     Kept by reference; must outlive this object.
   *                     참조로 보관한다. 이 객체보다 오래 살아야 한다.
   * @param[in] registry Event loop; kept by reference.
   *                     이벤트 루프. 참조로 보관한다.
   */
  CgiStdin(int fd, const std::string& body, IoRegistry& registry);
  /** @brief Stops writing. / 쓰기를 멈춘다. */
  ~CgiStdin();

  /** @brief The write end, -1 once closed. / 쓰는 쪽. 닫히면 -1. */
  int fd() const;
  /** @brief POLLOUT until closed. / 닫힐 때까지 POLLOUT. */
  short wantedEvents() const;
  /**
   * @brief One write(), or closes on an error, hang-up or the end.
   *        write() 한 번. 오류·끊김이거나 끝이면 닫는다.
   * @param[in] revents Events from poll(). / poll()이 돌려준 이벤트.
   */
  void onEvent(short revents);
  /** @brief Closes. / 닫는다. */
  void onFailure();
  /**
   * @brief Unwatches and closes the write end; the script sees EOF.
   *        감시를 풀고 쓰는 쪽을 닫는다. 스크립트는 EOF를 본다.
   */
  void close();

 private:
  /// @cond
  // Not copyable: declared and never defined (C++98 has no "= delete").
  // 복사하지 않는다: 선언만 하고 정의하지 않는다 (C++98에는 "= delete"가 없다).
  CgiStdin(const CgiStdin&);
  CgiStdin& operator=(const CgiStdin&);
  /// @endcond

  /** Write end, -1 once closed. / 쓰는 쪽. 닫히면 -1. */
  int fd_;
  /** Request body. / 요청 본문. */
  const std::string& body_;
  /** Bytes written so far. / 지금까지 쓴 바이트. */
  std::size_t written_;
  /** Event loop. / 이벤트 루프. */
  IoRegistry& registry_;
};

/**
 * @brief The script's standard output: reads it to EOF (RFC 3875 section
 *        6.4).
 *        스크립트의 표준 출력: EOF까지 읽는다 (RFC 3875 6.4절).
 *
 * POLLHUP alone also leads to a read(): Ubuntu reports it without POLLIN
 * after the last data, so only a read() of 0 marks the end (wiki
 * concepts/cgi section 1).
 * POLLHUP만 와도 read()한다. Ubuntu는 마지막 데이터 뒤에 POLLIN 없이 그것만
 * 보고하므로, read()가 0을 돌려줄 때만 끝으로 본다 (위키 concepts/cgi 1절).
 */
class CgiStdout : public IoHandler {
 public:
  /**
   * @brief Reads from the read end @p fd.
   *        읽는 쪽 @p fd에서 읽는다.
   * @param[in] fd       Non-blocking read end, now owned.
   *                     non-blocking 읽는 쪽. 이제 소유한다.
   * @param[in] registry Event loop; kept by reference.
   *                     이벤트 루프. 참조로 보관한다.
   */
  CgiStdout(int fd, IoRegistry& registry);
  /** @brief Stops reading. / 읽기를 멈춘다. */
  ~CgiStdout();

  /** @brief The read end, -1 once closed. / 읽는 쪽. 닫히면 -1. */
  int fd() const;
  /** @brief POLLIN until closed. / 닫힐 때까지 POLLIN. */
  short wantedEvents() const;
  /**
   * @brief One read(); EOF, an error or too much output closes.
   *        read() 한 번. EOF, 오류, 너무 많은 출력이면 닫는다.
   * @param[in] revents Events from poll(). / poll()이 돌려준 이벤트.
   */
  void onEvent(short revents);
  /** @brief Closes as failed. / 실패로 닫는다. */
  void onFailure();

  /** @brief True once closed. / 닫혔으면 true. */
  bool isClosed() const;
  /**
   * @brief True if it closed on an error or on limits::kMaxCgiOutputBytes.
   *        오류나 limits::kMaxCgiOutputBytes 때문에 닫혔으면 true.
   */
  bool failed() const;
  /**
   * @brief Swaps everything read with @p other.
   *        읽은 전부를 @p other와 맞바꾼다.
   * @param[in,out] other Text to swap with. / 맞바꿀 텍스트.
   */
  void swapData(std::string& other);

 private:
  /// @cond
  // Not copyable: declared and never defined (C++98 has no "= delete").
  // 복사하지 않는다: 선언만 하고 정의하지 않는다 (C++98에는 "= delete"가 없다).
  CgiStdout(const CgiStdout&);
  CgiStdout& operator=(const CgiStdout&);
  /// @endcond

  /**
   * @brief Unwatches and closes the read end.
   *        감시를 풀고 읽는 쪽을 닫는다.
   * @param[in] failed Whether the output is unusable. / 출력을 쓸 수 없는지.
   */
  void close(bool failed);

  /** Read end, -1 once closed. / 읽는 쪽. 닫히면 -1. */
  int fd_;
  /** Output so far. / 지금까지의 출력. */
  std::string data_;
  /** Closed on an error or the size limit. / 오류나 크기 상한으로 닫힘. */
  bool failed_;
  /** Event loop. / 이벤트 루프. */
  IoRegistry& registry_;
};

#endif
