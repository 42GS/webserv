#ifndef HTTP_STATUSTEXT_HPP
#define HTTP_STATUSTEXT_HPP

#include <cstddef>
#include <string>

#include "http/StatusCodes.hpp"

// One row of the reason phrase table.
struct StatusName {
  int status;
  const char* reason;
};

namespace status_text {

// Names as in raw/iana-http-status-codes.txt of the wiki.
const StatusName kStatusNames[] = {
    {http_status::kOk, "OK"},
    {http_status::kCreated, "Created"},
    {http_status::kNoContent, "No Content"},
    {http_status::kMovedPermanently, "Moved Permanently"},
    {http_status::kFound, "Found"},
    {http_status::kSeeOther, "See Other"},
    {http_status::kTemporaryRedirect, "Temporary Redirect"},
    {http_status::kPermanentRedirect, "Permanent Redirect"},
    {http_status::kBadRequest, "Bad Request"},
    {http_status::kForbidden, "Forbidden"},
    {http_status::kNotFound, "Not Found"},
    {http_status::kMethodNotAllowed, "Method Not Allowed"},
    {http_status::kRequestTimeout, "Request Timeout"},
    {http_status::kContentTooLarge, "Content Too Large"},
    {http_status::kUriTooLong, "URI Too Long"},
    {http_status::kUnsupportedMediaType, "Unsupported Media Type"},
    {http_status::kRequestHeaderFieldsTooLarge,
     "Request Header Fields Too Large"},
    {http_status::kInternalServerError, "Internal Server Error"},
    {http_status::kNotImplemented, "Not Implemented"},
    {http_status::kBadGateway, "Bad Gateway"},
    {http_status::kGatewayTimeout, "Gateway Timeout"},
    {http_status::kHttpVersionNotSupported, "HTTP Version Not Supported"},
};

const std::size_t kStatusNameCount =
    sizeof(kStatusNames) / sizeof(kStatusNames[0]);

}  // namespace status_text

// Reason phrase from the IANA status code registry, or "" for a code not in
// the table (the status line still ends with SP, RFC 9112 section 4).
std::string reasonPhrase(int status);

#endif
