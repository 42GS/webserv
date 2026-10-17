/**
 * @file  FieldSyntax.cpp
 * @brief Tokens, lists and parameters of field values.
 *        필드 값의 token, 목록, parameter.
 */
#include "http/FieldSyntax.hpp"

#include "http/Ascii.hpp"

namespace {

/** tchar (RFC 9110 section 5.6.2). / tchar (RFC 9110 5.6.2절). */
const std::string kTokenChars = ascii::kAlphanumerics + "!#$%&'*+-.^_`|~";
/** DEL, the control character after "~". / "~" 다음의 제어 문자 DEL. */
const unsigned char kDeleteChar = 0x7f;

/**
 * @brief True for an octet a quoted-pair may quote: HTAB, SP, VCHAR or
 *        obs-text (RFC 9110 section 5.6.4).
 *        quoted-pair가 감쌀 수 있는 옥텟이면 true: HTAB, SP, VCHAR,
 *        obs-text (RFC 9110 5.6.4절).
 * @param[in] c Octet. / 옥텟.
 * @return Whether it may be quoted. / 감쌀 수 있는지.
 */
bool isQuotable(unsigned char c) {
  return c == '\t' || (c >= ' ' && c != kDeleteChar);
}

/**
 * @brief Moves @p pos past SP and HTAB.
 *        @p pos를 SP와 HTAB 뒤로 옮긴다.
 * @param[in]     text Text. / 텍스트.
 * @param[in,out] pos  Position. / 위치.
 */
void skipWhitespace(const std::string& text, std::string::size_type& pos) {
  while (pos < text.size() && isWhitespace(text[pos])) ++pos;
}

/**
 * @brief Reads a quoted-string that starts at @p pos.
 *        @p pos에서 시작하는 quoted-string을 읽는다.
 *
 * qdtext is any quotable octet except DQUOTE and backslash.
 * qdtext는 DQUOTE와 backslash를 뺀, 감쌀 수 있는 옥텟이다.
 *
 * @param[in]     text  Text; text[pos] is DQUOTE. / 텍스트. text[pos]는 DQUOTE.
 * @param[in,out] pos   Moved past the closing DQUOTE. / 닫는 DQUOTE 뒤로
 * 옮긴다.
 * @param[out]    value Content without quoting. / 따옴표 처리를 푼 내용.
 * @return False if it is not closed or has a control character.
 *         닫히지 않았거나 제어 문자가 있으면 false.
 */
bool readQuotedString(const std::string& text, std::string::size_type& pos,
                      std::string& value) {
  for (++pos; pos < text.size(); ++pos) {
    unsigned char c = static_cast<unsigned char>(text[pos]);
    if (c == '"') {
      ++pos;
      return true;
    }
    if (c == '\\') {
      if (++pos == text.size()) return false;
      c = static_cast<unsigned char>(text[pos]);
    }
    if (!isQuotable(c)) return false;
    value += static_cast<char>(c);
  }
  return false;
}

/**
 * @brief Reads a parameter-value: a token or a quoted-string.
 *        parameter-value를 읽는다: token이나 quoted-string.
 * @param[in]     text  Text. / 텍스트.
 * @param[in,out] pos   Start; moved past the value. / 시작 위치. 값 뒤로
 * 옮긴다.
 * @param[out]    value The value. / 값.
 * @return False for an empty token or a bad quoted-string.
 *         token이 비었거나 quoted-string이 잘못됐으면 false.
 */
bool readParameterValue(const std::string& text, std::string::size_type& pos,
                        std::string& value) {
  if (pos < text.size() && text[pos] == '"')
    return readQuotedString(text, pos, value);
  std::string::size_type end = text.find_first_not_of(kTokenChars, pos);
  if (end == std::string::npos) end = text.size();
  value = text.substr(pos, end - pos);
  pos = end;
  return !value.empty();
}

}  // namespace

bool isToken(const std::string& text) {
  return !text.empty() && consistsOf(text, kTokenChars);
}

bool isWhitespace(char c) { return c == ' ' || c == '\t'; }

std::string trimWhitespace(const std::string& text) {
  std::string::size_type begin = 0;
  std::string::size_type end = text.size();
  while (begin < end && isWhitespace(text[begin])) ++begin;
  while (end > begin && isWhitespace(text[end - 1])) --end;
  return text.substr(begin, end - begin);
}

std::vector<std::string> splitList(const std::vector<std::string>& lines) {
  std::vector<std::string> items;
  for (std::size_t i = 0; i < lines.size(); ++i) {
    std::string::size_type start = 0;
    while (start <= lines[i].size()) {
      std::string::size_type comma = lines[i].find(',', start);
      if (comma == std::string::npos) comma = lines[i].size();
      const std::string item =
          trimWhitespace(lines[i].substr(start, comma - start));
      if (!item.empty()) items.push_back(item);
      start = comma + 1;
    }
  }
  return items;
}

std::string valueBeforeParameters(const std::string& fieldValue) {
  return toAsciiLower(
      trimWhitespace(fieldValue.substr(0, fieldValue.find(';'))));
}

// parameters = *( OWS ";" OWS [ parameter ] ), and no whitespace around
// "=" (section 5.6.6). pos is at a ";" at the top of each round.
// parameters = *( OWS ";" OWS [ parameter ] )이고 "=" 앞뒤에는 공백이
// 없다 (5.6.6절). 매 회 시작에서 pos는 ";"에 있다.
bool parseParameters(const std::string& fieldValue,
                     std::map<std::string, std::string>& parameters) {
  std::string::size_type pos = fieldValue.find(';');
  while (pos < fieldValue.size()) {
    ++pos;
    skipWhitespace(fieldValue, pos);
    if (pos == fieldValue.size()) break;
    if (fieldValue[pos] == ';') continue;
    const std::string::size_type equals =
        fieldValue.find_first_not_of(kTokenChars, pos);
    if (equals == pos || equals == std::string::npos ||
        fieldValue[equals] != '=')
      return false;
    const std::string name = toAsciiLower(fieldValue.substr(pos, equals - pos));
    pos = equals + 1;
    std::string value;
    if (!readParameterValue(fieldValue, pos, value) ||
        parameters.count(name) != 0)
      return false;
    parameters[name] = value;
    skipWhitespace(fieldValue, pos);
    if (pos < fieldValue.size() && fieldValue[pos] != ';') return false;
  }
  return true;
}
