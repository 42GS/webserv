#ifndef HTTP_STATUSTEXT_HPP
#define HTTP_STATUSTEXT_HPP

#include <string>

// Reason phrase from the IANA status code registry, or "" for a code not in
// the table (the status line still ends with SP, RFC 9112 section 4).
std::string reasonPhrase(int status);

#endif
