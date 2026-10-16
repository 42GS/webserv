#ifndef HTTP_METHODS_HPP
#define HTTP_METHODS_HPP

#include <string>

namespace methods {

const char* const kGet = "GET";
const char* const kHead = "HEAD";
const char* const kPost = "POST";
const char* const kDelete = "DELETE";

}  // namespace methods

// The request methods this server implements. The request parser answers
// 501 to any other method, and "limit_except" accepts only these, so both
// use this one list.
bool isImplementedMethod(const std::string& method);

// Length of the longest implemented method name; a longer method is 501
// even before the request line ends (RFC 9112 section 3).
std::string::size_type longestImplementedMethod();

#endif
