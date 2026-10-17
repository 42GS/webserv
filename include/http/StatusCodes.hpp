/**
 * @file  StatusCodes.hpp
 * @brief Status codes the server sends (concepts/status-codes in the wiki).
 *        서버가 보내는 상태 코드 (위키 concepts/status-codes).
 */
#ifndef HTTP_STATUSCODES_HPP
#define HTTP_STATUSCODES_HPP

/**
 * @brief Status codes, named after their reason phrases.
 *        이유 문구를 이름으로 쓴 상태 코드.
 */
namespace http_status {

// clang-format off
const int kOk                          = 200;  /**< OK */
const int kCreated                     = 201;  /**< Created */
const int kNoContent                   = 204;  /**< No Content */
const int kMovedPermanently            = 301;  /**< Moved Permanently */
const int kFound                       = 302;  /**< Found */
const int kSeeOther                    = 303;  /**< See Other */
const int kTemporaryRedirect           = 307;  /**< Temporary Redirect */
const int kNotModified                 = 304;  /**< Not Modified */
const int kPermanentRedirect           = 308;  /**< Permanent Redirect */
const int kBadRequest                  = 400;  /**< Bad Request */
const int kForbidden                   = 403;  /**< Forbidden */
const int kNotFound                    = 404;  /**< Not Found */
const int kMethodNotAllowed            = 405;  /**< Method Not Allowed */
const int kRequestTimeout              = 408;  /**< Request Timeout */
const int kConflict                    = 409;  /**< Conflict */
const int kContentTooLarge             = 413;  /**< Content Too Large */
const int kUriTooLong                  = 414;  /**< URI Too Long */
const int kUnsupportedMediaType        = 415;  /**< Unsupported Media Type */
const int kRequestHeaderFieldsTooLarge = 431;  /**< Request Header Fields Too Large */
const int kInternalServerError         = 500;  /**< Internal Server Error */
const int kNotImplemented              = 501;  /**< Not Implemented */
const int kBadGateway                  = 502;  /**< Bad Gateway */
const int kGatewayTimeout              = 504;  /**< Gateway Timeout */
const int kHttpVersionNotSupported     = 505;  /**< HTTP Version Not Supported */
// clang-format on

/**
 * @brief Largest three-digit code (RFC 9110 section 15).
 *        세 자리 상태 코드의 최댓값 (RFC 9110 15절).
 */
const int kMaxCode = 999;

}  // namespace http_status

#endif
