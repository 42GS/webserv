#include "core/HttpDate.hpp"
#include "http/MimeTypes.hpp"
#include "unit/TestFramework.hpp"

// Expected strings were computed with Python's datetime in UTC.
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
