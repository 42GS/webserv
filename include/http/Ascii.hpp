/**
 * @file  Ascii.hpp
 * @brief Character and number tests for HTTP and configuration text.
 *        HTTP와 설정 텍스트용 문자·숫자 검사.
 *
 * Built on the C++ standard library (std::string, `<locale>`) instead of
 * `<cctype>` and `<cstring>`: their character and string functions are not
 * on the subject's list of allowed functions, which has only strerror()
 * from them (raw/subject.md:113). HTTP is parsed as octets of a US-ASCII
 * superset (RFC 9112 section 2.2), so the classic locale is used
 * throughout.
 * `<cctype>`와 `<cstring>` 대신 C++ 표준 라이브러리(std::string, `<locale>`)로
 * 만들었다. 그 헤더의 문자·문자열 함수는 과제의 허용 함수 목록에 없고,
 * 목록에는 그중 strerror()만 있다 (raw/subject.md:113). HTTP는 US-ASCII
 * 상위 집합의 옥텟으로 해석하므로 (RFC 9112 2.2절) 항상 classic locale을
 * 쓴다.
 */
#ifndef HTTP_ASCII_HPP
#define HTTP_ASCII_HPP

#include <string>

/**
 * @brief Character sets used to build allowed-character strings.
 *        허용 문자열을 만들 때 쓰는 문자 집합.
 */
namespace ascii {

/** "0".."9". */
const char* const kDigits = "0123456789";
/** "a".."z" and "A".."Z". / "a".."z"와 "A".."Z". */
const char* const kLetters =
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
/** Digits and letters. / 숫자와 글자. */
const std::string kAlphanumerics = std::string(kDigits) + kLetters;

}  // namespace ascii

/**
 * @brief True for "0".."9".
 *        "0".."9"이면 true.
 * @param[in] c Character. / 문자.
 * @return Whether c is a decimal digit. / c가 십진 숫자인지.
 */
bool isAsciiDigit(char c);

/**
 * @brief Lowercases an ASCII letter; other characters are unchanged.
 *        ASCII 글자를 소문자로 바꾼다. 다른 문자는 그대로 둔다.
 * @param[in] c Character. / 문자.
 * @return The lowercase character. / 소문자.
 */
char toAsciiLower(char c);

/**
 * @brief Lowercases the ASCII letters of a text, for case-insensitive
 *        comparisons.
 *        대소문자 구분 없이 비교하려고 텍스트의 ASCII 글자를 소문자로 바꾼다.
 * @param[in] text Text. / 텍스트.
 * @return The lowercase text. / 소문자 텍스트.
 */
std::string toAsciiLower(const std::string& text);

/**
 * @brief True if every character of @p text is in @p allowed.
 *        @p text의 모든 문자가 @p allowed에 있으면 true.
 * @param[in] text    Text to check; an empty text passes.
 *                    검사할 텍스트. 비어 있으면 통과한다.
 * @param[in] allowed Allowed characters. / 허용 문자.
 * @return Whether text consists of allowed characters only.
 *         허용 문자로만 이루어졌는지.
 */
bool consistsOf(const std::string& text, const std::string& allowed);

/**
 * @brief Parses a decimal number with an upper bound.
 *        상한이 있는 십진수를 해석한다.
 * @param[in]  text     Digits only; no sign or space.
 *                      숫자만. 부호나 공백은 안 된다.
 * @param[in]  maxValue Largest accepted value. / 받을 수 있는 최댓값.
 * @param[out] value    The number; set only on success.
 *                      해석한 수. 성공했을 때만 쓴다.
 * @return False for an empty text, a non-digit, or a value above
 *         @p maxValue (overflow included).
 *         텍스트가 비었거나, 숫자가 아닌 문자가 있거나, 값이
 *         @p maxValue를 넘으면(overflow 포함) false.
 */
bool parseDecimal(const std::string& text, unsigned long maxValue,
                  unsigned long& value);

#endif
