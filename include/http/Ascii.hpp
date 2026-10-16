#ifndef HTTP_ASCII_HPP
#define HTTP_ASCII_HPP

#include <string>

// Character and number tests for HTTP and configuration text, built on the
// C++ standard library (std::string, <locale>) instead
// of <cctype> and <cstring>: those C functions are not on the subject's
// list of allowed functions (raw/subject.md:113). HTTP is parsed as octets
// of a US-ASCII superset (RFC 9112 section 2.2), so the classic locale is
// used throughout.
namespace ascii {

const char* const kDigits = "0123456789";
const char* const kLetters =
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
const std::string kAlphanumerics = std::string(kDigits) + kLetters;
const unsigned long kDecimalBase = 10;

}  // namespace ascii

bool isAsciiDigit(char c);
char toAsciiLower(char c);

// True if every character of text is in "allowed" (an empty text passes).
bool consistsOf(const std::string& text, const std::string& allowed);

// Decimal digits only, value <= maxValue, no overflow. False otherwise.
bool parseDecimal(const std::string& text, unsigned long maxValue,
                  unsigned long& value);

#endif
