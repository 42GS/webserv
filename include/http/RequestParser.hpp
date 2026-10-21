/**
 * @file  RequestParser.hpp
 * @brief Incremental HTTP/1.x request parser.
 *        조금씩 들어오는 HTTP/1.x 요청을 파싱한다.
 */
#ifndef HTTP_REQUESTPARSER_HPP
#define HTTP_REQUESTPARSER_HPP

#include <cstddef>
#include <string>

#include "http/Request.hpp"

/**
 * @brief The values of Request::version.
 *        Request::version이 가질 수 있는 값.
 */
namespace request_parser {

/** HTTP/1.0 */
const char* const kHttp10 = "HTTP/1.0";
/** HTTP/1.1, also for 1.2 and up. / 1.2 이상도 이것. */
const char* const kHttp11 = "HTTP/1.1";

}  // namespace request_parser

/**
 * @brief Incremental HTTP/1.x request parser (concepts/request-parsing).
 *        조금씩 들어오는 HTTP/1.x 요청의 파서 (concepts/request-parsing).
 *
 * feed() consumes bytes from the front of the buffer and leaves anything
 * it has not used, so data may arrive in any number of pieces and a
 * following request stays in the buffer. Once the head is complete it
 * returns HEAD_DONE; the caller then sets the body limit of the chosen
 * location and calls feed() again.
 * feed()는 버퍼 앞에서 바이트를 소비하고 쓰지 않은 것은 남겨 둔다.
 * 그래서 데이터가 몇 조각으로 오든 되고, 다음 요청은 버퍼에 남는다.
 * 머리가 끝나면 HEAD_DONE을 돌려준다. 호출자는 고른 location의 본문
 * 상한을 정한 뒤 feed()를 다시 부른다.
 *
 * Section numbers in the comments refer to RFC 9112 unless noted
 * otherwise.
 * 주석의 절 번호는 따로 적지 않으면 RFC 9112의 것이다.
 */
class RequestParser {
 public:
  /**
   * @brief What feed() found.
   *        feed()의 결과.
   */
  enum Result {
    /** Waiting for more bytes. / 바이트를 더 기다린다. */
    NEED_MORE,
    /**
     * @brief Head parsed; set the body limit, then feed again.
     *        머리를 다 읽음. 본문 상한을 정하고 다시 feed.
     */
    HEAD_DONE,
    /** The request is complete. / 요청이 끝남. */
    DONE,
    /** Invalid; see errorStatus(). / 잘못된 요청. errorStatus() 참고. */
    ERROR
  };

  /**
   * @brief A parser waiting for a request line, with the size limits of
   *        core/Limits.hpp.
   *        요청 줄을 기다리는 파서. 크기 상한은 core/Limits.hpp의 값이다.
   */
  RequestParser();

  /**
   * @brief Parses as much of @p buffer as possible.
   *        @p buffer를 할 수 있는 만큼 파싱한다.
   * @param[in,out] buffer Received bytes; the used ones are erased from
   *                       the front.
   *                       받은 바이트. 쓴 것은 앞에서 지운다.
   * @return Where parsing stopped. / 파싱이 멈춘 이유.
   */
  Result feed(std::string& buffer);

  /**
   * @brief Sets the body limit of the chosen location.
   *        고른 location의 본문 상한을 정한다.
   *
   * Until it is called the limit is 0, so any body is 413.
   * 부르기 전에는 상한이 0이라 본문이 있으면 모두 413이다.
   *
   * @param[in] maxBodySize Limit in bytes. / 바이트 단위 상한.
   */
  void setMaxBodySize(std::size_t maxBodySize);

  /**
   * @brief Status code to answer with after ERROR.
   *        ERROR 뒤에 답할 상태 코드.
   *
   * One of 400, 413, 414, 431, 501, 505. The connection is closed after
   * any error response.
   * 400, 413, 414, 431, 501, 505 중 하나. 오류 응답 뒤에는 연결을 닫는다.
   *
   * @return The status code. / 상태 코드.
   */
  int errorStatus() const;

  /**
   * @brief True once any byte of a request has been seen.
   *        요청의 바이트를 하나라도 받았으면 true.
   *
   * Decides between 408 and a silent close on timeout.
   * 시간 초과 때 408을 보낼지 그냥 닫을지 정한다.
   *
   * @return Whether a request has started. / 요청이 시작됐는지.
   */
  bool hasStarted() const;

  /** @brief The request parsed so far. / 지금까지 파싱한 요청. */
  const Request& request() const;

  /**
   * @brief Gets ready for the next request.
   *        다음 요청을 받을 준비를 한다.
   */
  void reset();

 private:
  /**
   * @brief Position in the request grammar.
   *        요청 문법상의 위치.
   */
  enum State {
    /** Waiting for the request line. / 요청 줄을 기다림. */
    REQUEST_LINE,
    /** Reading header lines. / 헤더 줄을 읽음. */
    HEADERS,
    /** Head done; waiting for the body limit. / 머리 끝, 본문 상한을 기다림. */
    BODY_START,
    /** Reading a Content-Length body. / Content-Length 본문을 읽음. */
    BODY_LENGTH,
    /** Reading a chunk-size line. / chunk-size 줄을 읽음. */
    CHUNK_SIZE,
    /** Reading chunk data. / chunk 데이터를 읽음. */
    CHUNK_DATA,
    /** Reading the CRLF after chunk data. / chunk 데이터 뒤 CRLF를 읽음. */
    CHUNK_DATA_END,
    /** Reading trailer lines. / trailer 줄을 읽음. */
    TRAILERS,
    /** Request complete. / 요청 끝. */
    COMPLETE,
    /** Error recorded. / 오류를 기록함. */
    FAILED
  };

  /**
   * @brief Result of taking one line from the buffer.
   *        버퍼에서 줄 하나를 꺼낸 결과.
   */
  enum LineStatus {
    /** A full line was taken. / 줄 하나를 꺼냄. */
    LINE_READY,
    /** No LF yet. / 아직 LF가 없음. */
    LINE_INCOMPLETE,
    /** A CR inside the line. / 줄 안에 CR이 있음. */
    LINE_INVALID
  };

  // Every function below that returns bool returns true to keep parsing
  // and false to stop: either more input is needed or fail() has recorded
  // an error. feed() then reports the state it stopped in.
  // 아래에서 bool을 돌려주는 함수는 모두, 계속 파싱하면 true, 멈추면
  // false다. 입력이 더 필요하거나 fail()이 오류를 기록한 경우다. 그러면
  // feed()가 멈춘 상태를 보고 결과를 알린다.

  /**
   * @brief Runs the reader of the current state once.
   *        현재 상태의 읽기 함수를 한 번 실행한다.
   * @param[in,out] buffer Received bytes. / 받은 바이트.
   * @return True to keep parsing. / 계속하면 true.
   */
  bool step(std::string& buffer);

  /**
   * @brief The Result for the state parsing stopped in.
   *        파싱이 멈춘 상태에 맞는 Result.
   */
  Result resultOfState() const;

  /**
   * @brief Records an error.
   *        오류를 기록한다.
   * @param[in] status Status code to answer with. / 답할 상태 코드.
   * @return Always false, so that callers can "return fail(...)".
   *         항상 false. 호출자가 "return fail(...)"로 쓸 수 있게 한다.
   */
  bool fail(int status);

  /**
   * @brief Takes one line ending in LF.
   *        LF로 끝나는 줄 하나를 꺼낸다.
   *
   * A lone LF is accepted, a preceding CR is dropped, and any other CR
   * makes the line invalid (section 2.2).
   * LF만 있어도 받고, 그 앞의 CR은 버리며, 다른 위치의 CR은 줄을
   * 잘못된 것으로 만든다 (2.2절).
   *
   * @param[in]  buffer Received bytes. / 받은 바이트.
   * @param[out] line   The line without its ending. / 끝 문자를 뺀 줄.
   * @return Whether a line was taken. / 줄을 꺼냈는지.
   */
  LineStatus takeLine(const std::string& buffer, std::string& line);

  /**
   * @brief Takes a line of the header section or of the trailer.
   *        헤더 영역이나 trailer의 줄 하나를 꺼낸다.
   *
   * Both count against the same limit, so a long trailer cannot bypass
   * it.
   * 둘 다 같은 상한으로 세므로, 긴 trailer로 상한을 피할 수 없다.
   *
   * @param[in]  buffer Received bytes. / 받은 바이트.
   * @param[out] line   The line. / 꺼낸 줄.
   * @return True if a line was taken. / 줄을 꺼냈으면 true.
   */
  bool takeHeaderLine(const std::string& buffer, std::string& line);

  /**
   * @brief Bytes of @p buffer not yet used in this feed().
   *        이번 feed()에서 아직 쓰지 않은 @p buffer의 바이트 수.
   * @param[in] buffer Received bytes. / 받은 바이트.
   * @return The count. / 바이트 수.
   */
  std::size_t unread(const std::string& buffer) const;

  /**
   * @brief REQUEST_LINE: skips empty lines, then parses the request line.
   *        REQUEST_LINE: 빈 줄을 건너뛰고 요청 줄을 파싱한다.
   * @param[in,out] buffer Received bytes. / 받은 바이트.
   * @return True to keep parsing. / 계속하면 true.
   */
  bool readRequestLine(std::string& buffer);

  /**
   * @brief HEADERS: stores one field, or finishes the head on an empty
   *        line.
   *        HEADERS: 필드 하나를 저장하거나, 빈 줄이면 머리를 끝낸다.
   * @param[in,out] buffer Received bytes. / 받은 바이트.
   * @return True to keep parsing. / 계속하면 true.
   */
  bool readHeaderLine(std::string& buffer);

  /**
   * @brief BODY_START: checks Content-Length against the limit and picks
   *        the body state.
   *        BODY_START: Content-Length를 상한과 비교하고 본문 상태를 고른다.
   * @return True to keep parsing. / 계속하면 true.
   */
  bool startBody();

  /**
   * @brief Copies body or chunk bytes until remaining_ is 0.
   *        remaining_이 0이 될 때까지 본문이나 chunk 바이트를 복사한다.
   * @param[in,out] buffer   Received bytes. / 받은 바이트.
   * @param[in]     whenDone State after the last byte.
   *                         마지막 바이트 뒤의 상태.
   * @return True to keep parsing. / 계속하면 true.
   */
  bool readBodyBytes(std::string& buffer, State whenDone);

  /**
   * @brief CHUNK_SIZE: takes a chunk-size line.
   *        CHUNK_SIZE: chunk-size 줄을 꺼낸다.
   * @param[in,out] buffer Received bytes. / 받은 바이트.
   * @return True to keep parsing. / 계속하면 true.
   */
  bool readChunkLine(std::string& buffer);

  /**
   * @brief CHUNK_DATA_END: takes the CRLF after chunk data.
   *        CHUNK_DATA_END: chunk 데이터 뒤의 CRLF를 꺼낸다.
   * @param[in,out] buffer Received bytes. / 받은 바이트.
   * @return True to keep parsing. / 계속하면 true.
   */
  bool readChunkEnd(std::string& buffer);

  /**
   * @brief TRAILERS: checks and discards one trailer field (section
   *        7.1.2), or completes the request on an empty line.
   *        TRAILERS: trailer 필드 하나를 검사하고 버리거나 (7.1.2절), 빈
   *        줄이면 요청을 끝낸다.
   * @param[in,out] buffer Received bytes. / 받은 바이트.
   * @return True to keep parsing. / 계속하면 true.
   */
  bool readTrailerLine(std::string& buffer);

  /**
   * @brief Parses "method SP request-target SP HTTP-version" (section 3).
   *        "method SP request-target SP HTTP-version"을 파싱한다 (3절).
   * @param[in] line The request line. / 요청 줄.
   * @return True to keep parsing. / 계속하면 true.
   */
  bool parseRequestLine(const std::string& line);

  /**
   * @brief Splits the origin-form or absolute-form target into path and
   *        query.
   *        origin-form이나 absolute-form target을 경로와 query로 나눈다.
   * @param[in] target request-target. / request-target.
   * @return False for any other form. / 다른 형태면 false.
   */
  bool parseTarget(const std::string& target);

  /**
   * @brief Parses "name: value" (section 5).
   *        "name: value"를 파싱한다 (5절).
   * @param[in]  line  The field line. / 필드 줄.
   * @param[out] name  Field name. / 필드 이름.
   * @param[out] value Value without surrounding whitespace.
   *                   앞뒤 공백을 뺀 값.
   * @return True to keep parsing. / 계속하면 true.
   */
  bool parseFieldLine(const std::string& line, std::string& name,
                      std::string& value);

  /**
   * @brief Checks Host and the body framing, then stops with HEAD_DONE.
   *        Host와 본문 길이 표시를 검사하고 HEAD_DONE으로 멈춘다.
   * @return Always false: the caller sets the body limit next.
   *         항상 false. 다음으로 호출자가 본문 상한을 정한다.
   */
  bool finishHead();

  /**
   * @brief Chooses chunked, Content-Length or no body (section 6.3).
   *        chunked, Content-Length, 본문 없음 중 하나를 고른다 (6.3절).
   * @return True if the framing is valid. / 길이 표시가 올바르면 true.
   */
  bool decideBodyLength();

  /**
   * @brief Parses "chunk-size [ chunk-ext ]" (section 7.1).
   *        "chunk-size [ chunk-ext ]"를 파싱한다 (7.1절).
   * @param[in] line The chunk-size line. / chunk-size 줄.
   * @return True to keep parsing. / 계속하면 true.
   */
  bool readChunkSize(const std::string& line);

  /**
   * @brief Bytes of the buffer used during the current feed().
   *        이번 feed()에서 쓴 버퍼 바이트 수.
   *
   * They are erased once at the end, so each byte is moved at most once
   * per call.
   * 끝에서 한 번만 지우므로, 한 번 호출에 바이트마다 많아야 한 번 옮긴다.
   */
  std::string::size_type consumed_;
  /**
   * @brief Body limit, 0 until setMaxBodySize().
   *        본문 상한, setMaxBodySize() 전에는 0.
   */
  std::size_t maxBodySize_;
  /** Current state. / 현재 상태. */
  State state_;
  /** Request being built. / 만들고 있는 요청. */
  Request request_;
  /** Status for ERROR. / ERROR일 때의 상태 코드. */
  int errorStatus_;
  /** A byte of this request arrived. / 이 요청의 바이트가 도착함. */
  bool started_;
  /** Empty lines before the request line. / 요청 줄 앞의 빈 줄. */
  std::size_t preambleBytes_;
  /**
   * @brief Header section and trailer bytes so far.
   *        지금까지의 헤더 영역과 trailer 바이트.
   */
  std::size_t headerBytes_;
  /** Transfer-Encoding: chunked. */
  bool chunked_;
  /** Content-Length value. / Content-Length 값. */
  std::size_t contentLength_;
  /**
   * @brief Body or chunk bytes still expected.
   *        아직 기다리는 본문이나 chunk 바이트.
   */
  std::size_t remaining_;
};

#endif
