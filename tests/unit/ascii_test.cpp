#include "http/Ascii.hpp"

#include "unit/TestFramework.hpp"

TEST_CASE(b08_asciiClassesWithoutLocale) {
  CHECK(isAsciiDigit('0') && isAsciiDigit('9'));
  CHECK(!isAsciiDigit('a') && !isAsciiDigit('/') && !isAsciiDigit(':'));
  CHECK(!isAsciiDigit('\0') && !isAsciiDigit('\xb9'));
  CHECK_EQ('a', toAsciiLower('A'));
  CHECK_EQ('z', toAsciiLower('Z'));
  CHECK_EQ('m', toAsciiLower('m'));
  CHECK_EQ('[', toAsciiLower('['));
  CHECK_EQ('\xc9', toAsciiLower('\xc9'));
}

TEST_CASE(b08_consistsOf) {
  CHECK(consistsOf("abc", "abc"));
  CHECK(consistsOf("", "abc"));
  CHECK(!consistsOf("abd", "abc"));
  CHECK(!consistsOf(std::string("a\0", 2), "abc"));
}

TEST_CASE(parseDecimalChecksDigitsAndRange) {
  unsigned long value = 0;
  CHECK(parseDecimal("255", 255, value));
  CHECK_EQ(255ul, value);
  CHECK(!parseDecimal("256", 255, value));
  CHECK(!parseDecimal("", 255, value));
  CHECK(!parseDecimal("1a", 255, value));
  CHECK(!parseDecimal("-1", 255, value));
  CHECK(!parseDecimal("9", 5, value));  // digit above a small maximum
  CHECK(parseDecimal("5", 5, value));
  CHECK(!parseDecimal("99999999999999999999999", 4294967295ul, value));
}
