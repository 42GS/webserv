#ifndef HTTP_STATUSCODES_HPP
#define HTTP_STATUSCODES_HPP

// Status codes the server sends (concepts/status-codes in the wiki).
namespace http_status {

const int kOk = 200;
const int kCreated = 201;
const int kNoContent = 204;
const int kMovedPermanently = 301;
const int kFound = 302;
const int kSeeOther = 303;
const int kTemporaryRedirect = 307;
const int kPermanentRedirect = 308;
const int kBadRequest = 400;
const int kForbidden = 403;
const int kNotFound = 404;
const int kMethodNotAllowed = 405;
const int kRequestTimeout = 408;
const int kContentTooLarge = 413;
const int kUriTooLong = 414;
const int kUnsupportedMediaType = 415;
const int kRequestHeaderFieldsTooLarge = 431;
const int kInternalServerError = 500;
const int kNotImplemented = 501;
const int kBadGateway = 502;
const int kGatewayTimeout = 504;
const int kHttpVersionNotSupported = 505;

// A status code has three digits (RFC 9110 section 15).
const int kMaxCode = 999;

}  // namespace http_status

#endif
