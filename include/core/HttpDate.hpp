#ifndef CORE_HTTPDATE_HPP
#define CORE_HTTPDATE_HPP

#include <ctime>
#include <string>

// Seconds since the epoch (UTC) as an IMF-fixdate, for example
// "Sun, 06 Nov 1994 08:49:37 GMT" (RFC 9110 section 5.6.7). Computed by
// hand because the allowed function list has no time functions (design
// decision 10).
std::string formatHttpDate(std::time_t secondsSinceEpoch);

#endif
