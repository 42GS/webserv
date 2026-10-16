#ifndef HTTP_REQUESTPARSER_HPP
#define HTTP_REQUESTPARSER_HPP

#include <cstddef>
#include <string>

#include "http/Request.hpp"

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

  LineStatus takeLine(std::string& buffer, std::string& line);
  bool fail(int status, Result& result);
  bool step(std::string& buffer, Result& result);

  bool parseRequestLine(const std::string& line, Result& result);
  bool parseTarget(const std::string& target);
  bool parseHeaderLine(const std::string& line, bool store, Result& result);
  bool finishHead(Result& result);
  bool decideBodyLength(Result& result);
  bool startBody(Result& result);
  bool readChunkSize(const std::string& line, Result& result);
  bool readRequestLine(std::string& buffer, Result& result);
  bool readHeaderLine(std::string& buffer, Result& result);
  bool readChunkLine(std::string& buffer, Result& result);
  bool readChunkEnd(std::string& buffer, Result& result);
  bool readTrailerLine(std::string& buffer, Result& result);
  bool readBodyBytes(std::string& buffer, State whenDone, Result& result);

  ParserLimits limits_;
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
