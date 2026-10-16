#ifndef HTTP_HEADERNAMES_HPP
#define HTTP_HEADERNAMES_HPP

// Field names the server reads or writes. HeaderMap compares names
// case-insensitively, so these spellings serve for both.
namespace header_name {

const char* const kAllow = "Allow";
const char* const kConnection = "Connection";
const char* const kContentLength = "Content-Length";
const char* const kContentType = "Content-Type";
const char* const kDate = "Date";
const char* const kHost = "Host";
const char* const kLocation = "Location";
const char* const kTransferEncoding = "Transfer-Encoding";

}  // namespace header_name

namespace header_value {

const char* const kClose = "close";  // Connection: close

}  // namespace header_value

#endif
