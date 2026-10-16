#ifndef HTTP_HEX_HPP
#define HTTP_HEX_HPP

// Hexadecimal digits of chunk sizes (RFC 9112 7.1) and percent-encoding
// (RFC 3986 2.1, where upper and lower case are equivalent). Both cases
// are accepted.
namespace hex {

const int kBase = 16;
const int kLetterValue = 10;  // value of 'a' / 'A'
const int kNotHex = -1;
const char* const kUpperDigits = "0123456789ABCDEF";

}  // namespace hex

// Value of a hexadecimal digit, or hex::kNotHex.
int hexDigitValue(char c);

#endif
