/**
 * @file  field_syntax_test.cpp
 * @brief Unit tests of the field value syntax.
 *        필드 값 문법의 단위 테스트.
 *
 * Test IDs (F-xx) refer to the test plan in the wiki
 * (analysis/test-plan).
 * 테스트 ID(F-xx)는 위키의 테스트 계획(analysis/test-plan)을 가리킨다.
 */
#include <map>
#include <string>

#include "http/FieldSyntax.hpp"
#include "unit/TestFramework.hpp"

namespace {

/** Parameters by lower-case name. / 소문자 이름별 매개변수. */
typedef std::map<std::string, std::string> Parameters;

/**
 * @brief parseParameters() without the result.
 *        결과를 버리는 parseParameters().
 * @param[in] value Field value. / 필드 값.
 * @return Whether it parses. / 해석되는지.
 */
bool parses(const std::string& value) {
  Parameters parameters;
  return parseParameters(value, parameters);
}

}  // namespace

TEST_CASE(f11_valueBeforeParametersIsLowercased) {
  CHECK_EQ(std::string("multipart/form-data"),
           valueBeforeParameters(" Multipart/Form-Data ; boundary=x"));
  CHECK_EQ(std::string("form-data"), valueBeforeParameters("form-data"));
}

TEST_CASE(f11_parametersTokensAndQuotedStrings) {
  Parameters parameters;
  CHECK(parseParameters(
      "form-data; NAME=\"a b\"; filename=\"q\\\"x\\\\y\" ;; size=3",
      parameters));
  CHECK_EQ(3u, parameters.size());
  CHECK_EQ(std::string("a b"), parameters["name"]);
  CHECK_EQ(std::string("q\"x\\y"), parameters["filename"]);
  CHECK_EQ(std::string("3"), parameters["size"]);
  parameters.clear();
  CHECK(parseParameters("text/plain", parameters));
  CHECK(parameters.empty());
  CHECK(parseParameters("x; name=\"\"", parameters));
  CHECK_EQ(std::string(""), parameters["name"]);
  // UTF-8 (obs-text) stays as sent.
  // UTF-8(obs-text)은 보낸 그대로 남는다.
  parameters.clear();
  CHECK(parseParameters("x; filename=\"\xed\x95\x9c.txt\"", parameters));
  CHECK_EQ(std::string("\xed\x95\x9c.txt"), parameters["filename"]);
}

TEST_CASE(f11_badParametersAreRejected) {
  CHECK(!parses("x; name = y"));                    // no whitespace around "="
  CHECK(!parses("x; name"));                        // no value
  CHECK(!parses("x; name="));                       // empty token
  CHECK(!parses("x; =y"));                          // no name
  CHECK(!parses("x; name=\"open"));                 // unclosed quote
  CHECK(!parses("x; name=\"a\"b"));                 // text after the quote
  CHECK(!parses("x; name=a; NAME=b"));              // same name twice
  CHECK(!parses(std::string("x; name=\"a\nb\"")));  // control character
}
