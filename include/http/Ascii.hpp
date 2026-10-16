#ifndef HTTP_ASCII_HPP
#define HTTP_ASCII_HPP

// ASCII character tests written out instead of <cctype> and <cstring>:
// those C functions are not on the subject's list of allowed functions
// (raw/subject.md:113), and HTTP is parsed as octets of a US-ASCII
// superset (RFC 9112 section 2.2), so no locale should matter.
namespace ascii {

const int kCaseOffset = 'a' - 'A';

}  // namespace ascii

bool isAsciiDigit(char c);
bool isAsciiAlpha(char c);
bool isAsciiAlnum(char c);
char toAsciiLower(char c);

// True if c is one of the characters of "set". NUL is never in a set
// (strchr() would have matched the terminator).
bool isOneOf(char c, const char* set);

#endif
