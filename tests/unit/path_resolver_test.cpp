#include "route/PathResolver.hpp"
#include "unit/TestFramework.hpp"

namespace {

const char* const kRejected = "(400)";

std::string normalized(const std::string& encoded) {
  std::string result;
  return normalizePath(encoded, result) ? result : kRejected;
}

}  // namespace

TEST_CASE(r01_keepsPlainPaths) {
  CHECK_EQ(std::string("/"), normalized("/"));
  CHECK_EQ(std::string("/a/b.html"), normalized("/a/b.html"));
  CHECK_EQ(std::string("/a/b/"), normalized("/a/b/"));
}

TEST_CASE(r01_decodesOnceAndCaseInsensitively) {
  CHECK_EQ(std::string("/a b"), normalized("/a%20b"));

  CHECK_EQ(std::string("/%41"), normalized("/%2541"));
  CHECK_EQ(std::string("/\xff"), normalized("/%fF"));
}

TEST_CASE(r01_rejectsBadEncodings) {
  CHECK_EQ(std::string(kRejected), normalized("/a%2"));
  CHECK_EQ(std::string(kRejected), normalized("/a%"));
  CHECK_EQ(std::string(kRejected), normalized("/a%zz"));
  CHECK_EQ(std::string(kRejected), normalized("/a%00b"));
  CHECK_EQ(std::string(kRejected), normalized("/a%2Fb"));
  CHECK_EQ(std::string(kRejected), normalized("/a%2fb"));
}

TEST_CASE(r01_mergesSlashesAndResolvesDots) {
  CHECK_EQ(std::string("/a/b"), normalized("//a///b"));
  CHECK_EQ(std::string("/a/c"), normalized("/a/./b/../c"));
  CHECK_EQ(std::string("/a/"), normalized("/a/b/.."));
  CHECK_EQ(std::string("/a/"), normalized("/a/."));
  CHECK_EQ(std::string("/"), normalized("/a/.."));
  CHECK_EQ(std::string("/c"), normalized("/a/%2e%2E/c"));
  CHECK_EQ(std::string("/..a/b."), normalized("/..a/b."));
}

TEST_CASE(r01_encodesPathsForUris) {
  CHECK_EQ(std::string("/a%20b/c%3Fd%23%25"), encodePath("/a b/c?d#%"));
  CHECK_EQ(std::string("/x-y_z.~!$&'()*+,;=:@"),
           encodePath("/x-y_z.~!$&'()*+,;=:@"));
  CHECK_EQ(std::string("/%FF%00"), encodePath(std::string("/\xff\0", 3)));
}

TEST_CASE(r01_rejectsGoingAboveTheRoot) {
  CHECK_EQ(std::string(kRejected), normalized("/.."));
  CHECK_EQ(std::string(kRejected), normalized("/../etc/passwd"));
  CHECK_EQ(std::string(kRejected), normalized("/a/../../etc"));
  CHECK_EQ(std::string(kRejected), normalized("/%2e%2e/etc"));
}
