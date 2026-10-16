#ifndef HTTP_REQUESTPARSER_HPP
#define HTTP_REQUESTPARSER_HPP

#include <cstddef>
#include <limits>
#include <string>

#include "http/Ascii.hpp"
#include "http/Request.hpp"

namespace request_parser {

const char* const kHttp10 = "HTTP/1.0";
const char* const kHttp11 = "HTTP/1.1";
const char* const kVersionPrefix = "HTTP/";
const std::string::size_type kVersionLength = 8;  // "HTTP/x.y"
const std::string::size_type kMajorIndex = 5;
const std::string::size_type kDotIndex = 6;
const std::string::size_type kMinorIndex = 7;
const char* const kAbsoluteFormPrefix = "http://";
const char* const kChunked = "chunked";

const unsigned char kDeleteChar = 0x7f;
// tchar (RFC 9110 section 5.6.2).
const std::string kTokenChars = ascii::kAlphanumerics + "!#$%&'*+-.^_`|~";
// Host characters: unreserved, pct-encoded, sub-delims, ":" and IP-literal
// brackets (RFC 3986 section 3.2.2).
const std::string kHostChars = ascii::kAlphanumerics + "-._~%!$&'()*+,;=:[]";
const std::size_t kMaxSize = std::numeric_limits<std::size_t>::max();
// The largest Content-Length that fits both parseDecimal() and size_t.
const unsigned long kMaxContentLength =
    std::numeric_limits<unsigned long>::max() < kMaxSize
        ? std::numeric_limits<unsigned long>::max()
        : static_cast<unsigned long>(kMaxSize);

}  // namespace request_parser

// Size limits that apply before a location is known (design 12).
struct ParserLimits {
  std::size_t maxRequestLine;  // longer request line: 414 (or 501, see .cpp)
  std::size_t maxHeaderBytes;  // larger header section or trailer: 431
  std::size_t maxChunkLine;    // longer chunk-size line: 400
};

ParserLimits defaultParserLimits();

// Incremental HTTP/1.x request parser (concepts/request-parsing).
//
// feed() consumes bytes from the front of the buffer and leaves anything it
// has not used, so data may arrive in any number of pieces and a following
// request stays in the buffer. Once the head is complete it returns
// HEAD_DONE; the caller then sets the body limit of the chosen location and
// calls feed() again.
class RequestParser {
 public:
  enum Result { NEED_MORE, HEAD_DONE, DONE, ERROR };

  explicit RequestParser(const ParserLimits& limits);

  Result feed(std::string& buffer);
  void setMaxBodySize(std::size_t maxBodySize);

  // Status code to answer with after ERROR (400, 413, 414, 431, 501, 505).
  // The connection is closed after any error response.
  int errorStatus() const;
  // True once any byte of a request has been seen (for 408 versus silent
  // close on timeout).
  bool hasStarted() const;
  const Request& request() const;
  void reset();

 private:
  enum State {
    REQUEST_LINE,
    HEADERS,
    BODY_START,
    BODY_LENGTH,
    CHUNK_SIZE,
    CHUNK_DATA,
    CHUNK_DATA_END,
    TRAILERS,
    COMPLETE,
    FAILED
  };
  enum LineStatus { LINE_READY, LINE_INCOMPLETE, LINE_INVALID };

  // Every step below returns true to keep parsing and false to stop:
  // either more input is needed or fail() has recorded an error. feed()
  // then reports the state it stopped in.
  bool step(std::string& buffer);
  Result resultOfState() const;
  bool fail(int status);

  LineStatus takeLine(const std::string& buffer, std::string& line);
  bool takeHeaderLine(const std::string& buffer, std::string& line);
  std::size_t unread(const std::string& buffer) const;

  bool readRequestLine(std::string& buffer);
  bool readHeaderLine(std::string& buffer);
  bool startBody();
  bool readBodyBytes(std::string& buffer, State whenDone);
  bool readChunkLine(std::string& buffer);
  bool readChunkEnd(std::string& buffer);
  bool readTrailerLine(std::string& buffer);

  bool parseRequestLine(const std::string& line);
  bool parseTarget(const std::string& target);
  bool parseFieldLine(const std::string& line, std::string& name,
                      std::string& value);
  bool finishHead();
  bool decideBodyLength();
  bool readChunkSize(const std::string& line);

  ParserLimits limits_;
  // Bytes of the buffer used during the current feed(). They are erased
  // once at the end, so each byte is moved at most once per call.
  std::string::size_type consumed_;
  std::size_t maxBodySize_;  // 0 until setMaxBodySize(): any body is 413
  State state_;
  Request request_;
  int errorStatus_;
  bool started_;
  std::size_t preambleBytes_;  // empty lines before the request line
  std::size_t headerBytes_;
  bool chunked_;
  std::size_t contentLength_;
  std::size_t remaining_;  // body or chunk bytes still expected
};

#endif
