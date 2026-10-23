/**
 * @file  path_resolver_test.cpp
 * @brief Unit tests of path normalization and encoding.
 *        경로 정규화와 인코딩의 단위 테스트.
 *
 * Test IDs (R-xx) refer to the test plan in the wiki
 * (analysis/test-plan).
 * 테스트 ID(R-xx)는 위키의 테스트 계획(analysis/test-plan)을 가리킨다.
 */
#include "route/PathResolver.hpp"
#include "unit/TestConstants.hpp"
#include "unit/TestFramework.hpp"

namespace {

/**
 * @brief normalizePath() as one string for CHECK_EQ.
 *        CHECK_EQ에 쓰도록 normalizePath()를 문자열 하나로 돌려준다.
 * @param[in] encoded Request path. / 요청 경로.
 * @return The normalized path, or path_resolver_test::kRejected.
 *         정규화한 경로, 또는 path_resolver_test::kRejected.
 */
std::string normalized(const std::string& encoded) {
  std::string result;
  return normalizePath(encoded, result) ? result
                                        : path_resolver_test::kRejected;
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
  CHECK_EQ(std::string(path_resolver_test::kRejected), normalized("/a%2"));
  CHECK_EQ(std::string(path_resolver_test::kRejected), normalized("/a%"));
  CHECK_EQ(std::string(path_resolver_test::kRejected), normalized("/a%zz"));
  CHECK_EQ(std::string(path_resolver_test::kRejected), normalized("/a%00b"));
  CHECK_EQ(std::string(path_resolver_test::kRejected), normalized("/a%2Fb"));
  CHECK_EQ(std::string(path_resolver_test::kRejected), normalized("/a%2fb"));
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
  CHECK_EQ(std::string(path_resolver_test::kRejected), normalized("/.."));
  CHECK_EQ(std::string(path_resolver_test::kRejected),
           normalized("/../etc/passwd"));
  CHECK_EQ(std::string(path_resolver_test::kRejected),
           normalized("/a/../../etc"));
  CHECK_EQ(std::string(path_resolver_test::kRejected),
           normalized("/%2e%2e/etc"));
}
