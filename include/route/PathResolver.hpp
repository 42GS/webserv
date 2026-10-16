#ifndef ROUTE_PATHRESOLVER_HPP
#define ROUTE_PATHRESOLVER_HPP

#include <string>

namespace path_resolver {

const std::string::size_type kEscapeDigits = 2;  // %XX
// Left as they are by encodePath besides letters and digits: unreserved
// and sub-delims, ":", "@" (pchar) and "/" (RFC 3986 section 3.3).
const char* const kPathSymbols = "-._~!$&'()*+,;=:@/";

}  // namespace path_resolver

// Turns the percent-encoded path of a request into a normalized one
// (concepts/security "경로 정규화 절차"): split into segments, decode each
// once, merge repeated "/", resolve "." and "..". Returns false, meaning
// 400, for a bad or NUL percent-encoding, an encoded "/" (design decision
// 3), or ".." above the root (design decision 2).
//
// A trailing "/" is kept, and a path ending in "." or ".." is treated as a
// directory, as in RFC 3986 remove_dot_segments.
bool normalizePath(const std::string& encodedPath, std::string& normalized);

// Percent-encodes a decoded path for use in a URI (Location, links):
// unreserved characters, sub-delims, ":", "@" and "/" stay as they are.
std::string encodePath(const std::string& decodedPath);

#endif
