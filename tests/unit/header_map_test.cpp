#include "http/HeaderMap.hpp"
#include "unit/TestFramework.hpp"

TEST_CASE(headerNamesIgnoreCase) {
  HeaderMap headers;
  headers.add("Content-Length", "5");
  CHECK(headers.has("content-length"));
  CHECK(headers.first("CONTENT-LENGTH") != 0);
  if (headers.first("CONTENT-LENGTH") != 0)
    CHECK_EQ(std::string("5"), *headers.first("CONTENT-LENGTH"));
  CHECK(headers.first("Host") == 0);
}

TEST_CASE(headerKeepsRepeatedLinesInOrder) {
  HeaderMap headers;
  headers.add("Set-Cookie", "a=1");
  headers.add("Host", "x");
  headers.add("set-cookie", "b=2");
  CHECK_EQ(2u, headers.count("Set-Cookie"));
  const std::vector<std::string> cookies = headers.all("SET-COOKIE");
  CHECK_EQ(2u, cookies.size());
  if (cookies.size() == 2) {
    CHECK_EQ(std::string("a=1"), cookies[0]);
    CHECK_EQ(std::string("b=2"), cookies[1]);
  }
  headers.clear();
  CHECK_EQ(0u, headers.count("Host"));
}
