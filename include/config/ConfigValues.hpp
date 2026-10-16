#ifndef CONFIG_CONFIGVALUES_HPP
#define CONFIG_CONFIGVALUES_HPP

#include <cstddef>
#include <string>

#include "config/ServerConfig.hpp"

namespace config_values {

const unsigned long kMaxOctet = 255;
const char* const kAnyAddress = "0.0.0.0";  // "listen 8080;" listens here

}  // namespace config_values

// Parsers for single directive arguments. Each returns false when the text
// is not a valid value; the caller reports the error with its line number.

// NGINX size syntax: digits with an optional k/K or m/M suffix.
bool parseSize(const std::string& text, std::size_t& out);

// "port" or "a.b.c.d:port". A missing address means "0.0.0.0".
bool parseListenAddr(const std::string& text, ListenAddr& out);

// "a.b.c.d:port", for error messages.
std::string formatListenAddr(const ListenAddr& addr);

#endif
