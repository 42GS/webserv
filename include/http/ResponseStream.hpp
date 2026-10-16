/**
 * @file  ResponseStream.hpp
 * @brief Serialized response sent a piece at a time.
 *        조각씩 보내는 직렬화된 응답.
 */
#ifndef HTTP_RESPONSESTREAM_HPP
#define HTTP_RESPONSESTREAM_HPP

#include <cstddef>
#include <string>

#include "http/Response.hpp"

/**
 * @brief Serialized response handed to send() a piece at a time.
 *        send()에 조각씩 넘기는 직렬화된 응답.
 *
 * A file body is read in limits::kFileChunkSize pieces as the previous
 * piece is sent, so a large file never stalls the loop (raw/subject.md:134
 * exempts regular files from poll(), design 8). Owns the file descriptor
 * while reading.
 * 파일 본문은 앞 조각을 보낼 때마다 limits::kFileChunkSize씩 읽으므로,
 * 큰 파일이 루프를 막지 않는다 (raw/subject.md:134는 일반 파일을
 * poll()에서 제외한다, 설계 8절). 읽는 동안 파일 디스크립터를 소유한다.
 */
class ResponseStream {
 public:
  /** @brief An empty stream. / 빈 스트림. */
  ResponseStream();

  /**
   * @brief Closes the body file if it is open.
   *        본문 파일이 열려 있으면 닫는다.
   */
  ~ResponseStream();

  /**
   * @brief Starts a response: serializes the head and opens the body file.
   *        응답을 시작한다: 머리를 직렬화하고 본문 파일을 연다.
   * @param[in] response The response. / 응답.
   * @return False if the body file cannot be opened.
   *         본문 파일을 열 수 없으면 false.
   */
  bool reset(const Response& response);

  /**
   * @brief True while bytes are left to send.
   *        보낼 바이트가 남아 있으면 true.
   */
  bool hasPending() const;

  /** @brief Start of the bytes to send next. / 다음에 보낼 바이트의 시작. */
  const char* data() const;

  /** @brief Number of bytes available at data(). / data()에 있는 바이트 수. */
  std::size_t size() const;

  /**
   * @brief Marks @p n bytes as sent and reads the next file piece when
   *        needed.
   *        @p n 바이트를 보낸 것으로 표시하고, 필요하면 다음 파일 조각을
   *        읽는다.
   * @param[in] n Bytes accepted by send(). / send()가 받아 간 바이트 수.
   * @return False if the file could not be read: the response cannot be
   *         finished and the connection must close.
   *         파일을 읽지 못하면 false. 응답을 끝낼 수 없으니 연결을 닫아야
   *         한다.
   */
  bool consume(std::size_t n);

 private:
  /// @cond
  // Not copyable: declared and never defined (C++98 has no "= delete").
  // 복사하지 않는다: 선언만 하고 정의하지 않는다 (C++98에는 "= delete"가 없다).
  ResponseStream(const ResponseStream&);
  ResponseStream& operator=(const ResponseStream&);
  /// @endcond

  /**
   * @brief Replaces out_ with the next piece of the body file.
   *        out_을 본문 파일의 다음 조각으로 바꾼다.
   * @return False if read() fails or the file ends early.
   *         read()가 실패하거나 파일이 일찍 끝나면 false.
   */
  bool readFilePiece();

  /** @brief Closes the body file if open. / 본문 파일이 열려 있으면 닫는다. */
  void closeFile();

  /**
   * @brief Bytes in hand: the head, an in-memory body, or a file piece.
   *        지금 가진 바이트: 머리, 메모리 본문, 또는 파일 조각.
   */
  std::string out_;
  /** Bytes of out_ already sent. / out_ 중 이미 보낸 바이트. */
  std::size_t offset_;
  /** Body file, -1 when no file is open. / 본문 파일, 열린 파일이 없으면 -1. */
  int fileFd_;
  /** File bytes not yet read. / 아직 읽지 않은 파일 바이트. */
  std::size_t fileRemaining_;
};

#endif
