#ifndef HTTP_RESPONSE_HPP
#define HTTP_RESPONSE_HPP

#include <cstddef>
#include <string>

#include "http/HeaderMap.hpp"

// A response before serialization (design 8). The body is either in memory
// or, for a large static file, a path read in pieces by ResponseStream.
struct Response {
  Response() : status(0), bodyFileSize(0), omitBody(false) {}

  int status;
  HeaderMap headers;  // without Content-Length, which ResponseStream adds
  std::string body;
  std::string bodyFile;
  std::size_t bodyFileSize;
  bool omitBody;  // HEAD: same head as GET, no content (RFC 9110 9.3.2)
};

#endif
