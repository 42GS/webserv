#include "http/Hex.hpp"

int hexDigitValue(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + hex::kLetterValue;
  if (c >= 'A' && c <= 'F') return c - 'A' + hex::kLetterValue;
  return hex::kNotHex;
}
