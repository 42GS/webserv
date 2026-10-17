/**
 * @file  StatusText.cpp
 * @brief The reason phrase table.
 *        이유 문구 표.
 */
#include "http/StatusText.hpp"

#include <cstddef>

#include "http/StatusCodes.hpp"

namespace {

/**
 * @brief One row of the reason phrase table.
 *        이유 문구 표의 한 행.
 */
struct StatusName {
  /** Status code. / 상태 코드. */
  int status;
  /** Reason phrase. / 이유 문구. */
  const char* reason;
};

/**
 * @brief Reason phrases of the codes the server sends.
 *        서버가 보내는 코드의 이유 문구.
 *
 * Names as in raw/iana-http-status-codes.txt of the wiki.
 * 이름은 위키의 raw/iana-http-status-codes.txt와 같다.
 */
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
    {http_status::kConflict, "Conflict"},
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

/** Number of rows in kStatusNames. / kStatusNames의 행 수. */
const std::size_t kStatusNameCount =
    sizeof(kStatusNames) / sizeof(kStatusNames[0]);

}  // namespace

std::string reasonPhrase(int status) {
  for (std::size_t i = 0; i < kStatusNameCount; ++i) {
    if (kStatusNames[i].status == status) return kStatusNames[i].reason;
  }
  return "";
}
