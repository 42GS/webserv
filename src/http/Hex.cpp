/**
 * @file  Hex.cpp
 * @brief Hexadecimal digit values.
 *        16진 숫자의 값.
 */
#include "http/Hex.hpp"

int hexDigitValue(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return hex::kNotHex;
}
