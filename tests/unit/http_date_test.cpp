/**
 * @file  http_date_test.cpp
 * @brief Unit tests of the HTTP date and the MIME types.
 *        HTTP 날짜와 MIME 타입의 단위 테스트.
 *
 * Test IDs (R-xx) refer to the test plan in the wiki
 * (analysis/test-plan).
 * 테스트 ID(R-xx)는 위키의 테스트 계획(analysis/test-plan)을 가리킨다.
 */
#include "core/HttpDate.hpp"
#include "http/MimeTypes.hpp"
#include "unit/TestFramework.hpp"

// Expected strings were computed with Python's datetime in UTC.
// 기대 문자열은 Python의 datetime으로 UTC 기준으로 계산했다.
TEST_CASE(r15_formatsImfFixdate) {
  CHECK_EQ(std::string("Sun, 06 Nov 1994 08:49:37 GMT"),
           formatHttpDate(784111777));
  CHECK_EQ(std::string("Thu, 01 Jan 1970 00:00:00 GMT"), formatHttpDate(0));
  CHECK_EQ(std::string("Tue, 29 Feb 2000 23:59:59 GMT"),
           formatHttpDate(951868799));
  CHECK_EQ(std::string("Fri, 16 Oct 2026 12:00:00 GMT"),
           formatHttpDate(1792152000));
  CHECK_EQ(std::string("Mon, 01 Mar 2100 00:00:00 GMT"),
           formatHttpDate(static_cast<std::time_t>(4107542400LL)));
}

TEST_CASE(r06_mimeTypeByExtension) {
  CHECK_EQ(std::string("text/html"), mimeTypeFor("/a/index.html"));
  CHECK_EQ(std::string("image/png"), mimeTypeFor("/a/B.PNG"));
  CHECK_EQ(std::string("application/octet-stream"), mimeTypeFor("/a/file"));
  CHECK_EQ(std::string("application/octet-stream"), mimeTypeFor("/a.d/file"));
  CHECK_EQ(std::string("application/octet-stream"), mimeTypeFor("/a/x.bin"));
}
