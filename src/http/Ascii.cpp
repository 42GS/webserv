#include "http/Ascii.hpp"

#include <string>

bool isAsciiDigit(char c) { return c >= '0' && c <= '9'; }

bool isAsciiAlpha(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

bool isAsciiAlnum(char c) { return isAsciiDigit(c) || isAsciiAlpha(c); }

char toAsciiLower(char c) {
  return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + ascii::kCaseOffset) : c;
}

bool isOneOf(char c, const char* set) {
  return std::string(set).find(c) != std::string::npos;
}
