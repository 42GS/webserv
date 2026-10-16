/**
 * @file  HeaderNames.hpp
 * @brief Field names and values the server reads or writes.
 *        서버가 읽거나 쓰는 필드 이름과 값.
 *
 * HeaderMap compares names case-insensitively, so these spellings serve
 * for both reading and writing.
 * HeaderMap은 이름을 대소문자 구분 없이 비교하므로, 이 표기를 읽기와
 * 쓰기에 함께 쓴다.
 */
#ifndef HTTP_HEADERNAMES_HPP
#define HTTP_HEADERNAMES_HPP

/**
 * @brief Field names.
 *        필드 이름.
 */
namespace header_name {

/** Methods a 405 resource supports. / 405일 때 지원하는 메서드. */
const char* const kAllow = "Allow";
/** "close" ends the connection. / "close"면 연결을 끝낸다. */
const char* const kConnection = "Connection";
/** Body length. / 본문 길이. */
const char* const kContentLength = "Content-Length";
/** Media type of the body. / 본문의 미디어 타입. */
const char* const kContentType = "Content-Type";
/** Sent with every response. / 모든 응답에 보낸다. */
const char* const kDate = "Date";
/** Required in HTTP/1.1 requests. / HTTP/1.1 요청에 필수. */
const char* const kHost = "Host";
/** Target of a redirect. / 리다이렉트 대상. */
const char* const kLocation = "Location";
/** "chunked" bodies. / "chunked" 본문. */
const char* const kTransferEncoding = "Transfer-Encoding";

}  // namespace header_name

/**
 * @brief Field values.
 *        필드 값.
 */
namespace header_value {

/** Connection: close */
const char* const kClose = "close";

}  // namespace header_value

#endif
