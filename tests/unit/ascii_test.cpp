#include "http/Ascii.hpp"

#include "unit/TestFramework.hpp"

TEST_CASE(b08_asciiClassesWithoutLocale) {
  CHECK(isAsciiDigit('0') && isAsciiDigit('9'));
  CHECK(!isAsciiDigit('a') && !isAsciiDigit('/') && !isAsciiDigit(':'));
  CHECK(isAsciiAlpha('a') && isAsciiAlpha('Z'));
  CHECK(!isAsciiAlpha('@') && !isAsciiAlpha('[') && !isAsciiAlpha('`'));
  CHECK(!isAsciiAlpha('{') && !isAsciiAlpha('\xe9'));
  CHECK(isAsciiAlnum('7') && isAsciiAlnum('q') && !isAsciiAlnum('-'));
  CHECK_EQ('a', toAsciiLower('A'));
  CHECK_EQ('z', toAsciiLower('Z'));
  CHECK_EQ('m', toAsciiLower('m'));
  CHECK_EQ('[', toAsciiLower('['));
  CHECK_EQ('\xc9', toAsciiLower('\xc9'));
}

TEST_CASE(b08_isOneOfNeverMatchesNul) {
  CHECK(isOneOf('-', "-._~"));
  CHECK(!isOneOf('a', "-._~"));
  CHECK(!isOneOf('\0', "-._~"));
}
