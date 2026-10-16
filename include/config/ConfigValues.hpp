#ifndef CONFIG_CONFIGVALUES_HPP
#define CONFIG_CONFIGVALUES_HPP

#include <cstddef>
#include <string>

#include "config/ServerConfig.hpp"

// Parsers for single directive arguments. Each returns false when the text
// is not a valid value; the caller reports the error with its line number.

// Decimal digits only, value <= maxValue.
bool parseUnsigned(const std::string& text, unsigned long maxValue,
                   unsigned long& out);

// NGINX size syntax: digits with an optional k/K or m/M suffix.
bool parseSize(const std::string& text, std::size_t& out);

// "port" or "a.b.c.d:port". A missing address means "0.0.0.0".
bool parseListenAddr(const std::string& text, ListenAddr& out);

#endif
