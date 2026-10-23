/**
 * @file  fs_test.cpp
 * @brief Unit tests of the file helpers.
 *        파일 도우미의 단위 테스트.
 */
#include <fstream>
#include <string>

#include "fs/FileInfo.hpp"
#include "fs/FileReader.hpp"
#include "unit/TestConstants.hpp"
#include "unit/TestFramework.hpp"

namespace {

/**
 * @brief Writes fs_test::kContent to fs_test::kFile.
 *        fs_test::kFile에 fs_test::kContent를 쓴다.
 */
void writeTestFile() {
  std::ofstream out(fs_test::kFile);
  out << fs_test::kContent;
}

}  // namespace

TEST_CASE(fileKindAndSize) {
  writeTestFile();
  CHECK_EQ(static_cast<int>(FILE_REGULAR),
           static_cast<int>(fileKind(fs_test::kFile)));
  CHECK_EQ(static_cast<int>(FILE_DIRECTORY), static_cast<int>(fileKind("obj")));
  CHECK_EQ(static_cast<int>(FILE_MISSING),
           static_cast<int>(fileKind("obj/no-such-file")));
  std::size_t size = 0;
  CHECK(regularFileSize(fs_test::kFile, size));
  CHECK_EQ(fs_test::kContentSize, size);
  CHECK(!regularFileSize("obj", size));
  CHECK(isReadable(fs_test::kFile));
}

TEST_CASE(readWholeFileRespectsTheLimit) {
  writeTestFile();
  std::string content;
  CHECK(readWholeFile(fs_test::kFile, fs_test::kContentSize, content));
  CHECK_EQ(std::string(fs_test::kContent), content);
  CHECK(!readWholeFile(fs_test::kFile, fs_test::kContentSize - 1, content));
  CHECK(!readWholeFile("obj/no-such-file", fs_test::kContentSize, content));
}
