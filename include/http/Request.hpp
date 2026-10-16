#ifndef HTTP_REQUEST_HPP
#define HTTP_REQUEST_HPP

#include <string>

#include "http/HeaderMap.hpp"

// A parsed request. "path" is still percent-encoded; decoding and dot
// segments are the router's job (concepts/security, step 4).
struct Request {
  std::string method;
  std::string target;   // as received
  std::string path;     // before the first "?"
  std::string query;    // after the first "?", not decoded
  std::string version;  // "HTTP/1.0" or "HTTP/1.1"
  HeaderMap headers;
  std::string body;  // chunked coding already removed
};

#endif
