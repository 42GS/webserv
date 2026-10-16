/**
 * @file  Ascii.cpp
 * @brief Character and decimal tests.
 *        문자와 십진수 검사.
 */
#include "http/Ascii.hpp"

#include <locale>

bool isAsciiDigit(char c) { return c >= '0' && c <= '9'; }

char toAsciiLower(char c) { return std::tolower(c, std::locale::classic()); }

bool consistsOf(const std::string& text, const std::string& allowed) {
  return text.find_first_not_of(allowed) == std::string::npos;
}

bool parseDecimal(const std::string& text, unsigned long maxValue,
                  unsigned long& value) {
  if (text.empty() || !consistsOf(text, ascii::kDigits)) return false;
  unsigned long result = 0;
  for (std::string::size_type i = 0; i < text.size(); ++i) {
    const unsigned long digit = static_cast<unsigned long>(text[i] - '0');
    // digit > maxValue first, so that maxValue - digit cannot wrap around.
    // maxValue - digit이 음수로 돌지 않도록 digit > maxValue를 먼저 본다.
    if (digit > maxValue || result > (maxValue - digit) / 10) return false;
    result = result * 10 + digit;
  }
  value = result;
  return true;
}
