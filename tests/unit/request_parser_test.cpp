/**
 * @file  request_parser_test.cpp
 * @brief Unit tests of RequestParser.
 *        RequestParser의 단위 테스트.
 *
 * Test IDs (P-xx, K-xx) refer to the test plan in the wiki
 * (analysis/test-plan).
 * 테스트 ID(P-xx, K-xx)는 위키의 테스트 계획(analysis/test-plan)을 가리킨다.
 */
#include <string>

#include "http/RequestParser.hpp"
#include "unit/TestConstants.hpp"
#include "unit/TestFramework.hpp"

namespace {

/**
 * @brief Feeds the whole text, setting the body limit after the head as
 *        the connection does.
 *        연결이 하듯이 머리 다음에 본문 상한을 정하며 텍스트 전체를 넣는다.
 * @param[in,out] parser    Parser. / 파서.
 * @param[in,out] buffer    Input; leftover bytes stay in it.
 *                          입력. 남은 바이트는 여기에 남는다.
 * @param[in]     bodyLimit Body size limit. / 본문 크기 상한.
 * @return The parser's last result. / 파서의 마지막 결과.
 */
RequestParser::Result parse(RequestParser& parser, std::string& buffer,
                            std::size_t bodyLimit) {
  RequestParser::Result result = parser.feed(buffer);
  if (result == RequestParser::HEAD_DONE) {
    parser.setMaxBodySize(bodyLimit);
    result = parser.feed(buffer);
  }
  return result;
}

/**
 * @brief The outcome of parsing a text as a number.
 *        텍스트를 해석한 결과를 숫자로.
 * @param[in] text      Request text. / 요청 텍스트.
 * @param[in] bodyLimit Body size limit. / 본문 크기 상한.
 * @return request_parser_test::kParsed, request_parser_test::kIncomplete,
 *         or the error status code.
 *         request_parser_test::kParsed, request_parser_test::kIncomplete,
 *         또는 오류 상태 코드.
 */
int statusOf(const std::string& text,
             std::size_t bodyLimit = request_parser_test::kBodyLimit) {
  RequestParser parser;
  std::string buffer(text);
  const RequestParser::Result result = parse(parser, buffer, bodyLimit);
  if (result == RequestParser::DONE) return request_parser_test::kParsed;
  if (result == RequestParser::ERROR) return parser.errorStatus();
  return request_parser_test::kIncomplete;
}

/**
 * @brief A GET request for target.
 *        target에 대한 GET 요청.
 * @param[in] target Request target. / 요청 대상.
 * @return Request text. / 요청 텍스트.
 */
std::string get(const std::string& target) {
  return "GET " + target + " HTTP/1.1\r\nHost: x\r\n\r\n";
}

/**
 * @brief A POST request to "/".
 *        "/"로 가는 POST 요청.
 * @param[in] headers Extra header lines, each ending in CRLF.
 *                    덧붙일 헤더 줄. 각각 CRLF로 끝난다.
 * @param[in] body    Body. / 본문.
 * @return Request text. / 요청 텍스트.
 */
std::string post(const std::string& headers, const std::string& body) {
  return "POST / HTTP/1.1\r\nHost: x\r\n" + headers + "\r\n" + body;
}

/**
 * @brief Whether the connection stays open after the request.
 *        요청 뒤에 연결이 열려 있는지.
 * @param[in] text A complete request; the test fails otherwise.
 *                 끝난 요청. 아니면 테스트가 실패한다.
 * @return Request::persistent. / Request::persistent.
 */
bool persistentAfter(const std::string& text) {
  RequestParser parser;
  std::string buffer(text);
  const RequestParser::Result result =
      parse(parser, buffer, request_parser_test::kBodyLimit);
  CHECK_EQ(static_cast<int>(RequestParser::DONE), static_cast<int>(result));
  return parser.request().persistent;
}

}  // namespace

TEST_CASE(p01_sameResultWhereverTheInputIsSplit) {
  const std::string text =
      post("Transfer-Encoding: chunked\r\n",
           "3;ext=1\r\nabc\r\n2\r\nde\r\n0\r\nX-Trailer: t\r\n\r\n");
  for (std::string::size_type split = 0; split <= text.size(); ++split) {
    RequestParser parser;
    std::string buffer = text.substr(0, split);
    RequestParser::Result result =
        parse(parser, buffer, request_parser_test::kBodyLimit);
    buffer += text.substr(split);
    if (result != RequestParser::DONE)
      result = parse(parser, buffer, request_parser_test::kBodyLimit);
    CHECK_EQ(static_cast<int>(RequestParser::DONE), static_cast<int>(result));
    CHECK_EQ(std::string("abcde"), parser.request().body);
  }
}

TEST_CASE(p01_incompleteInputAsksForMore) {
  CHECK_EQ(request_parser_test::kIncomplete,
           statusOf("GET / HTTP/1.1\r\nHost: x\r\n"));
  CHECK_EQ(request_parser_test::kIncomplete,
           statusOf(post("Content-Length: 5\r\n", "abc")));
  RequestParser parser;
  std::string empty;
  CHECK(!parser.hasStarted());
  parser.feed(empty);
  CHECK(!parser.hasStarted());
  std::string partial("GE");
  parser.feed(partial);
  CHECK(parser.hasStarted());
}

TEST_CASE(p02_leavesTheNextRequestInTheBuffer) {
  RequestParser parser;
  std::string buffer = post("Content-Length: 2\r\n", "hi") + get("/next");
  CHECK_EQ(
      static_cast<int>(RequestParser::DONE),
      static_cast<int>(parse(parser, buffer, request_parser_test::kBodyLimit)));
  CHECK_EQ(std::string("hi"), parser.request().body);
  CHECK_EQ(get("/next"), buffer);
  parser.reset();
  CHECK_EQ(
      static_cast<int>(RequestParser::DONE),
      static_cast<int>(parse(parser, buffer, request_parser_test::kBodyLimit)));
  CHECK_EQ(std::string("/next"), parser.request().path);
  CHECK(buffer.empty());
}

TEST_CASE(p03_lineEndings) {
  CHECK_EQ(request_parser_test::kParsed,
           statusOf("GET / HTTP/1.1\nHost: x\n\n"));
  CHECK_EQ(400, statusOf("GET / HTTP/1.1\rX\r\nHost: x\r\n\r\n"));
  CHECK_EQ(400, statusOf("GET / HTTP/1.1\r\nHost: a\rb\r\n\r\n"));
  // Chunk extensions are ignored, so only the line check sees this CR.
  // chunk 확장은 무시하므로, 이 CR은 줄 검사만 본다.
  CHECK_EQ(400, statusOf(post("Transfer-Encoding: chunked\r\n",
                              "1;a\rb\r\nx\r\n0\r\n\r\n")));
}

TEST_CASE(p04_ignoresEmptyLinesBeforeTheRequestLine) {
  CHECK_EQ(request_parser_test::kParsed, statusOf("\r\n\r\n" + get("/")));
  CHECK_EQ(400, statusOf(std::string(8193, '\n') + get("/")));
}

TEST_CASE(p05_requestLineNeedsSingleSpaces) {
  CHECK_EQ(400, statusOf("GET  / HTTP/1.1\r\nHost: x\r\n\r\n"));
  CHECK_EQ(400, statusOf("GET\t/ HTTP/1.1\r\nHost: x\r\n\r\n"));
  CHECK_EQ(400, statusOf("GET / HTTP/1.1 \r\nHost: x\r\n\r\n"));
  CHECK_EQ(400, statusOf("GET /a b HTTP/1.1\r\nHost: x\r\n\r\n"));
  CHECK_EQ(400, statusOf("GET /\r\nHost: x\r\n\r\n"));
  CHECK_EQ(400, statusOf(" / HTTP/1.1\r\nHost: x\r\n\r\n"));
}

TEST_CASE(p06_methods) {
  CHECK_EQ(request_parser_test::kParsed,
           statusOf("DELETE /a HTTP/1.1\r\nHost: x\r\n\r\n"));
  CHECK_EQ(request_parser_test::kParsed,
           statusOf("HEAD / HTTP/1.1\r\nHost: x\r\n\r\n"));
  CHECK_EQ(501, statusOf("get / HTTP/1.1\r\nHost: x\r\n\r\n"));
  CHECK_EQ(501, statusOf("PUT / HTTP/1.1\r\nHost: x\r\n\r\n"));
  CHECK_EQ(501, statusOf("UNKNOWN / HTTP/1.1\r\nHost: x\r\n\r\n"));
  CHECK_EQ(400, statusOf("G(T / HTTP/1.1\r\nHost: x\r\n\r\n"));
  CHECK_EQ(501, statusOf(std::string(9000, 'A') + " / HTTP/1.1\r\n"));
  CHECK_EQ(501, statusOf(std::string(9000, 'A')));
}

TEST_CASE(p07_longTargetIs414) {
  const std::string prefix = "GET /";
  const std::string suffix = " HTTP/1.1";
  const std::size_t fill = 8192 - prefix.size() - suffix.size();
  CHECK_EQ(request_parser_test::kParsed,
           statusOf(prefix + std::string(fill, 'a') + suffix +
                    "\r\nHost: x\r\n\r\n"));
  CHECK_EQ(414, statusOf(prefix + std::string(fill + 1, 'a') + suffix +
                         "\r\nHost: x\r\n\r\n"));
  CHECK_EQ(414, statusOf(prefix + std::string(9000, 'a')));
}

TEST_CASE(p08_versions) {
  CHECK_EQ(request_parser_test::kParsed, statusOf("GET / HTTP/1.0\r\n\r\n"));
  CHECK_EQ(505, statusOf("GET / HTTP/2.0\r\nHost: x\r\n\r\n"));
  CHECK_EQ(505, statusOf("GET / HTTP/0.9\r\nHost: x\r\n\r\n"));
  CHECK_EQ(400, statusOf("GET / http/1.1\r\nHost: x\r\n\r\n"));
  CHECK_EQ(400, statusOf("GET / HTTP/1\r\nHost: x\r\n\r\n"));
  CHECK_EQ(400, statusOf("GET / HTTP/1.10\r\nHost: x\r\n\r\n"));
  CHECK_EQ(400, statusOf("GET / HTTP/1x1\r\nHost: x\r\n\r\n"));
  CHECK_EQ(400, statusOf("GET / HTTP/x.1\r\nHost: x\r\n\r\n"));
  CHECK_EQ(400, statusOf("GET /\r\n\r\n"));
  RequestParser parser;
  std::string buffer("GET / HTTP/1.2\r\nHost: x\r\n\r\n");
  CHECK_EQ(
      static_cast<int>(RequestParser::DONE),
      static_cast<int>(parse(parser, buffer, request_parser_test::kBodyLimit)));
  CHECK_EQ(std::string("HTTP/1.1"), parser.request().version);
}

TEST_CASE(p09_targets) {
  RequestParser parser;
  std::string buffer = get("/a/b?x=1?y");
  parse(parser, buffer, request_parser_test::kBodyLimit);
  CHECK_EQ(std::string("/a/b"), parser.request().path);
  CHECK_EQ(std::string("x=1?y"), parser.request().query);

  parser.reset();
  buffer = get("http://example.com:8080/p?q");
  parse(parser, buffer, request_parser_test::kBodyLimit);
  CHECK_EQ(std::string("/p"), parser.request().path);
  CHECK_EQ(std::string("q"), parser.request().query);

  parser.reset();
  buffer = get("HTTP://example.com?q");
  parse(parser, buffer, request_parser_test::kBodyLimit);
  CHECK_EQ(std::string("/"), parser.request().path);
  CHECK_EQ(std::string("q"), parser.request().query);

  CHECK_EQ(400, statusOf(get("*")));
  CHECK_EQ(400, statusOf(get("a/b")));
  CHECK_EQ(400, statusOf(get("http:///p")));
  CHECK_EQ(400, statusOf(get("https://h/p")));
  CHECK_EQ(400, statusOf(get("/a#frag")));
  CHECK_EQ(501, statusOf("OPTIONS * HTTP/1.1\r\nHost: x\r\n\r\n"));
}

TEST_CASE(p10_headerFieldSyntax) {
  RequestParser parser;
  std::string buffer("GET / HTTP/1.1\r\nhost:  x  \r\nX-A:\t1\r\n\r\n");
  parse(parser, buffer, request_parser_test::kBodyLimit);
  const std::string* host = parser.request().headers.first("HOST");
  CHECK(host != 0);
  if (host != 0) CHECK_EQ(std::string("x"), *host);
  CHECK_EQ(400, statusOf("GET / HTTP/1.1\r\nHost : x\r\n\r\n"));
  CHECK_EQ(400, statusOf("GET / HTTP/1.1\r\nHost: x\r\nNoColon\r\n\r\n"));
  CHECK_EQ(400, statusOf("GET / HTTP/1.1\r\nHost: x\r\n: v\r\n\r\n"));
  CHECK_EQ(400, statusOf("GET / HTTP/1.1\r\nHost: x\r\nA: \x01\r\n\r\n"));
}

TEST_CASE(p11_linesStartingWithWhitespace) {
  CHECK_EQ(400, statusOf("GET / HTTP/1.1\r\n X: 1\r\nHost: x\r\n\r\n"));
  CHECK_EQ(400, statusOf("GET / HTTP/1.1\r\nHost: x\r\nX: 1\r\n\t2\r\n\r\n"));
}

TEST_CASE(p12_hostRules) {
  CHECK_EQ(400, statusOf("GET / HTTP/1.1\r\n\r\n"));
  CHECK_EQ(request_parser_test::kParsed, statusOf("GET / HTTP/1.0\r\n\r\n"));
  CHECK_EQ(400, statusOf("GET / HTTP/1.1\r\nHost: a\r\nHost: a\r\n\r\n"));
  CHECK_EQ(400, statusOf("GET / HTTP/1.0\r\nHost: a\r\nhost: b\r\n\r\n"));
  CHECK_EQ(400, statusOf("GET / HTTP/1.1\r\nHost: a b\r\n\r\n"));
  CHECK_EQ(400, statusOf("GET / HTTP/1.1\r\nHost: u@h\r\n\r\n"));
  CHECK_EQ(request_parser_test::kParsed,
           statusOf("GET / HTTP/1.1\r\nHost: [::1]:8080\r\n\r\n"));
  CHECK_EQ(request_parser_test::kParsed,
           statusOf("GET / HTTP/1.1\r\nHost:\r\n\r\n"));
}

TEST_CASE(p13_largeHeaderSectionIs431) {
  const std::string big(16384, 'a');
  CHECK_EQ(431,
           statusOf("GET / HTTP/1.1\r\nHost: x\r\nX: " + big + "\r\n\r\n"));
  CHECK_EQ(431, statusOf("GET / HTTP/1.1\r\nHost: x\r\nX: " + big));
  CHECK_EQ(request_parser_test::kParsed,
           statusOf("GET / HTTP/1.1\r\nHost: x\r\nX: " +
                    std::string(16000, 'a') + "\r\n\r\n"));
}

TEST_CASE(p14_bodyLengthRules) {
  CHECK_EQ(400, statusOf(post("Transfer-Encoding: chunked\r\n"
                              "Content-Length: 3\r\n",
                              "0\r\n\r\n")));
  CHECK_EQ(400, statusOf(post("Transfer-Encoding: gzip\r\n", "")));
  CHECK_EQ(501, statusOf(post("Transfer-Encoding: gzip, chunked\r\n", "")));
  CHECK_EQ(400, statusOf(post("Transfer-Encoding: chunked, chunked\r\n", "")));
  CHECK_EQ(400, statusOf(post("Transfer-Encoding:\r\n", "")));
  CHECK_EQ(400, statusOf("POST / HTTP/1.0\r\nTransfer-Encoding: chunked\r\n"
                         "\r\n0\r\n\r\n"));
  CHECK_EQ(400, statusOf(post("Content-Length: -1\r\n", "")));
  CHECK_EQ(400, statusOf(post("Content-Length: 1x\r\n", "a")));
  CHECK_EQ(400, statusOf(post("Content-Length: 3, 4\r\n", "abc")));
  CHECK_EQ(400,
           statusOf(post("Content-Length: 3\r\nContent-Length: 4\r\n", "abc")));
  CHECK_EQ(request_parser_test::kParsed,
           statusOf(post("Content-Length: 3, 3\r\n", "abc")));
  CHECK_EQ(request_parser_test::kParsed,
           statusOf(post("Content-Length: 3\r\nContent-Length: 3\r\n", "abc")));
  CHECK_EQ(request_parser_test::kParsed, statusOf(post("", "")));
  CHECK_EQ(400, statusOf("POST / HTTP/1.0\r\n\r\n"));
  CHECK_EQ(request_parser_test::kParsed,
           statusOf("POST / HTTP/1.0\r\nContent-Length: 0\r\n\r\n"));
  CHECK_EQ(request_parser_test::kParsed,
           statusOf(post("Transfer-Encoding: Chunked\r\n", "0\r\n\r\n")));
}

TEST_CASE(p15_hugeContentLengthDoesNotOverflow) {
  CHECK_EQ(400,
           statusOf(post("Content-Length: 99999999999999999999999\r\n", "")));
}

TEST_CASE(p16_contentLengthOverTheLimitIs413) {
  CHECK_EQ(request_parser_test::kParsed,
           statusOf(post("Content-Length: 100\r\n", std::string(100, 'a'))));
  CHECK_EQ(413, statusOf(post("Content-Length: 101\r\n", "")));
  RequestParser parser;
  std::string buffer = post("Content-Length: 1\r\n", "a");
  CHECK_EQ(static_cast<int>(RequestParser::HEAD_DONE),
           static_cast<int>(parser.feed(buffer)));
  // Without a limit from the location, any body is refused.
  // location에서 상한을 받지 않으면 어떤 본문도 거부한다.
  CHECK_EQ(static_cast<int>(RequestParser::ERROR),
           static_cast<int>(parser.feed(buffer)));
  CHECK_EQ(413, parser.errorStatus());
}

TEST_CASE(p17_chunkedBodies) {
  CHECK_EQ(request_parser_test::kParsed,
           statusOf(post("Transfer-Encoding: chunked\r\n",
                         "A\r\n0123456789\r\n0\r\n\r\n")));
  CHECK_EQ(request_parser_test::kParsed,
           statusOf(post("Transfer-Encoding: chunked\r\n",
                         "1 ; a=b\r\nx\r\n000\r\n\r\n")));
  CHECK_EQ(400, statusOf(post("Transfer-Encoding: chunked\r\n",
                              "g\r\nx\r\n0\r\n\r\n")));
  CHECK_EQ(400, statusOf(post("Transfer-Encoding: chunked\r\n",
                              "1 x\r\nx\r\n0\r\n\r\n")));
  CHECK_EQ(400, statusOf(post("Transfer-Encoding: chunked\r\n",
                              "1\r\nxy\r\n0\r\n\r\n")));
  CHECK_EQ(400, statusOf(post("Transfer-Encoding: chunked\r\n",
                              "FFFFFFFFFFFFFFFFFFFF\r\n")));
  CHECK_EQ(400, statusOf(post("Transfer-Encoding: chunked\r\n",
                              "1;" + std::string(5000, 'e'))));
  CHECK_EQ(413, statusOf(post("Transfer-Encoding: chunked\r\n",
                              "64\r\n" + std::string(100, 'a') + "\r\n1\r\n")));
  CHECK_EQ(400, statusOf(post("Transfer-Encoding: chunked\r\n",
                              "0\r\nBad Trailer\r\n\r\n")));
  CHECK_EQ(request_parser_test::kIncomplete,
           statusOf(post("Transfer-Encoding: chunked\r\n", "1\r\nx\r\n")));
}

TEST_CASE(k02_k03_persistence) {
  CHECK(persistentAfter("GET / HTTP/1.1\r\nHost: x\r\n\r\n"));
  CHECK(persistentAfter("GET / HTTP/1.2\r\nHost: x\r\n\r\n"));
  CHECK(!persistentAfter(
      "GET / HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n"));
  CHECK(
      !persistentAfter("GET / HTTP/1.1\r\nHost: x\r\n"
                       "Connection: Keep-Alive, CLOSE\r\n\r\n"));
  CHECK(
      !persistentAfter("GET / HTTP/1.1\r\nHost: x\r\n"
                       "Connection: ,\r\nConnection: x ,close\r\n\r\n"));
  CHECK(persistentAfter(
      "GET / HTTP/1.1\r\nHost: x\r\nConnection: closed\r\n\r\n"));
  CHECK(!persistentAfter("GET / HTTP/1.0\r\n\r\n"));
  CHECK(!persistentAfter("GET / HTTP/1.0\r\nConnection: keep-alive\r\n\r\n"));
}
