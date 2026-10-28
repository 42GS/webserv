/**
 * @file  logger_test.cpp
 * @brief Unit tests of the log files.
 *        로그 파일의 단위 테스트.
 */
#include "core/Logger.hpp"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

#include "unit/TestConstants.hpp"
#include "unit/TestFramework.hpp"

namespace {

/**
 * @brief Everything in a file, or "" if it cannot be read.
 *        파일의 내용 전부. 읽을 수 없으면 "".
 * @param[in] path File. / 파일.
 * @return The content. / 내용.
 */
std::string contentOf(const char* path) {
  std::ifstream in(path);
  std::ostringstream content;
  content << in.rdbuf();
  return content.str();
}

/**
 * @brief Removes both test logs, so each test starts empty.
 *        두 테스트 로그를 지워서 테스트마다 빈 상태로 시작한다.
 */
void removeTestLogs() {
  std::remove(logger_test::kAccessFile);
  std::remove(logger_test::kErrorFile);
}

}  // namespace

TEST_CASE(logLinesAreDatedAndAppended) {
  removeTestLogs();
  {
    Logger logger;
    CHECK(logger.open(logger_test::kAccessFile, logger_test::kErrorFile));
    logger.access(0, "first");
    logger.access(784111777, "second");
    logger.error(0, "trouble");
  }
  CHECK_EQ(std::string("[Thu, 01 Jan 1970 00:00:00 GMT] first\n"
                       "[Sun, 06 Nov 1994 08:49:37 GMT] second\n"),
           contentOf(logger_test::kAccessFile));
  CHECK_EQ(std::string("[Thu, 01 Jan 1970 00:00:00 GMT] trouble\n"),
           contentOf(logger_test::kErrorFile));

  Logger reopened;
  CHECK(reopened.open(logger_test::kAccessFile, logger_test::kErrorFile));
  reopened.access(0, "third");
  CHECK_EQ(std::string("[Thu, 01 Jan 1970 00:00:00 GMT] first\n"
                       "[Sun, 06 Nov 1994 08:49:37 GMT] second\n"
                       "[Thu, 01 Jan 1970 00:00:00 GMT] third\n"),
           contentOf(logger_test::kAccessFile));
  removeTestLogs();
}

// A client can put a line end into a request; it must not start a line.
// 클라이언트가 요청에 줄 끝을 넣을 수 있다. 그것이 새 줄을 만들면 안 된다.
TEST_CASE(controlCharactersStayOnOneLine) {
  removeTestLogs();
  {
    Logger logger;
    CHECK(logger.open(logger_test::kAccessFile, logger_test::kErrorFile));
    logger.access(0, std::string("a\nb\rc\td\x7f", 8));
  }
  CHECK_EQ(std::string("[Thu, 01 Jan 1970 00:00:00 GMT] a?b?c?d?\n"),
           contentOf(logger_test::kAccessFile));
  removeTestLogs();
}

TEST_CASE(unopenableLogWritesNothing) {
  removeTestLogs();
  Logger logger;
  CHECK(!logger.open(logger_test::kAccessFile, logger_test::kUnopenableFile));
  logger.access(0, "lost");
  logger.error(0, "lost");
  CHECK_EQ(std::string(""), contentOf(logger_test::kAccessFile));
  removeTestLogs();
}
