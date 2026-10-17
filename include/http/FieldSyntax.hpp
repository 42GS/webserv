/**
 * @file  FieldSyntax.hpp
 * @brief Common field value syntax (RFC 9110 section 5.6).
 *        공통 필드 값 문법 (RFC 9110 5.6절).
 *
 * Used by the request parser and by the multipart/form-data parser, whose
 * part headers carry parameters in the same form.
 * 요청 파서와, 같은 형태의 parameter를 part 헤더에 담는 multipart/form-data
 * 파서가 함께 쓴다.
 */
#ifndef HTTP_FIELDSYNTAX_HPP
#define HTTP_FIELDSYNTAX_HPP

#include <map>
#include <string>
#include <vector>

/**
 * @brief True for a non-empty token (RFC 9110 section 5.6.2).
 *        비어 있지 않은 token이면 true (RFC 9110 5.6.2절).
 * @param[in] text Text. / 텍스트.
 * @return Whether text is a token. / token인지.
 */
bool isToken(const std::string& text);

/**
 * @brief True for SP and HTAB, the whitespace of HTTP (OWS).
 *        HTTP의 공백(OWS)인 SP와 HTAB이면 true.
 * @param[in] c Character. / 문자.
 * @return Whether c is SP or HTAB. / SP나 HTAB인지.
 */
bool isWhitespace(char c);

/**
 * @brief Removes SP and HTAB at both ends.
 *        양 끝의 SP와 HTAB을 지운다.
 * @param[in] text Text. / 텍스트.
 * @return The trimmed text. / 다듬은 텍스트.
 */
std::string trimWhitespace(const std::string& text);

/**
 * @brief Splits comma-separated lists (RFC 9110 section 5.6.1).
 *        쉼표로 구분한 목록을 나눈다 (RFC 9110 5.6.1절).
 *
 * Whitespace around elements is trimmed and empty elements are dropped.
 * 원소 앞뒤 공백은 지우고 빈 원소는 버린다.
 *
 * @param[in] lines Values of every field line with the same name.
 *                  같은 이름을 가진 모든 필드 줄의 값.
 * @return The elements of all lines, in order. / 모든 줄의 원소, 순서대로.
 */
std::vector<std::string> splitList(const std::vector<std::string>& lines);

/**
 * @brief The value before the parameters, trimmed and lowercased.
 *        parameter 앞의 값. 공백을 지우고 소문자로 바꾼다.
 *
 * Such as the media type of "Text/HTML; charset=utf-8". Media types and
 * disposition types are case-insensitive.
 * "Text/HTML; charset=utf-8"의 미디어 타입 같은 것이다. 미디어 타입과
 * disposition 타입은 대소문자를 구분하지 않는다.
 *
 * @param[in] fieldValue Field value. / 필드 값.
 * @return Text before the first ";". / 첫 ";" 앞의 텍스트.
 */
std::string valueBeforeParameters(const std::string& fieldValue);

/**
 * @brief Parses the parameters after the first ";" (RFC 9110 section
 *        5.6.6).
 *        첫 ";" 뒤의 parameter를 해석한다 (RFC 9110 5.6.6절).
 *
 * Names are lowercased; values are tokens or quoted strings, with each
 * quoted-pair replaced by its octet (section 5.6.4).
 * 이름은 소문자로 바꾼다. 값은 token이나 quoted-string이고, quoted-pair는
 * 뒤의 옥텟으로 바꾼다 (5.6.4절).
 *
 * @param[in]  fieldValue Field value. / 필드 값.
 * @param[out] parameters Name to value. / 이름 → 값.
 * @return False for a syntax error or a name given twice, which would
 *         leave the value ambiguous.
 *         문법 오류이거나, 값이 모호해지는 같은 이름이 두 번 나오면 false.
 */
bool parseParameters(const std::string& fieldValue,
                     std::map<std::string, std::string>& parameters);

#endif
