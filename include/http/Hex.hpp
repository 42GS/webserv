/**
 * @file  Hex.hpp
 * @brief Hexadecimal digits.
 *        16진 숫자.
 *
 * Used for chunk sizes (RFC 9112 section 7.1) and percent-encoding
 * (RFC 3986 section 2.1, where upper and lower case are equivalent). Both
 * cases are accepted.
 * chunk 크기(RFC 9112 7.1절)와 퍼센트 인코딩(RFC 3986 2.1절, 대문자와
 * 소문자가 같다)에 쓴다. 대문자와 소문자를 모두 받는다.
 */
#ifndef HTTP_HEX_HPP
#define HTTP_HEX_HPP

/**
 * @brief Hexadecimal constants.
 *        16진 상수.
 */
namespace hex {

/** Radix. / 기수. */
const int kBase = 16;
/**
 * @brief hexDigitValue() of a non-digit.
 *        16진 숫자가 아닐 때의 hexDigitValue().
 */
const int kNotHex = -1;
/**
 * @brief Digits used when encoding (RFC 3986 section 2.1 recommends
 *        uppercase).
 *        인코딩에 쓰는 숫자 (RFC 3986 2.1절은 대문자를 권한다).
 */
const char* const kUpperDigits = "0123456789ABCDEF";

}  // namespace hex

/**
 * @brief Value of a hexadecimal digit.
 *        16진 숫자 하나의 값.
 * @param[in] c Character, either case. / 문자, 대소문자 무관.
 * @return 0..15, or hex::kNotHex. / 0..15, 또는 hex::kNotHex.
 */
int hexDigitValue(char c);

#endif
