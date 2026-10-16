#ifndef CONFIG_CONFIGVALUES_HPP
#define CONFIG_CONFIGVALUES_HPP

#include <cstddef>
#include <string>

#include "config/ServerConfig.hpp"

namespace config_values {

const unsigned long kMaxPort = 65535;
const unsigned long kMaxOctet = 255;
const std::size_t kOctetCount = 4;  // a.b.c.d
const std::size_t kKilobyte = 1024;
const std::size_t kMegabyte = 1024 * 1024;
const char* const kAnyAddress = "0.0.0.0";  // "listen 8080;" listens here

}  // namespace config_values

// Parsers for single directive arguments. Each returns false when the text
// is not a valid value; the caller reports the error with its line number.

// NGINX size syntax: digits with an optional k/K or m/M suffix.
bool parseSize(const std::string& text, std::size_t& out);

// "port" or "a.b.c.d:port". A missing address means "0.0.0.0".
bool parseListenAddr(const std::string& text, ListenAddr& out);

#endif
