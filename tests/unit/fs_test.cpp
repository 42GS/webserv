#include <fstream>
#include <string>

#include "fs/FileInfo.hpp"
#include "fs/FileReader.hpp"
#include "unit/TestFramework.hpp"

namespace {

// Relative to the repository root, where "make test" runs the tests.
const char* const kFile = "obj/fs-test.txt";
const char* const kContent = "0123456789";
const std::size_t kContentSize = 10;

void writeTestFile() {
  std::ofstream out(kFile);
  out << kContent;
}

}  // namespace

TEST_CASE(fileKindAndSize) {
  writeTestFile();
  CHECK_EQ(static_cast<int>(FILE_REGULAR), static_cast<int>(fileKind(kFile)));
  CHECK_EQ(static_cast<int>(FILE_DIRECTORY), static_cast<int>(fileKind("obj")));
  CHECK_EQ(static_cast<int>(FILE_MISSING),
           static_cast<int>(fileKind("obj/no-such-file")));
  std::size_t size = 0;
  CHECK(regularFileSize(kFile, size));
  CHECK_EQ(kContentSize, size);
  CHECK(!regularFileSize("obj", size));
  CHECK(isReadable(kFile));
}

TEST_CASE(readWholeFileRespectsTheLimit) {
  writeTestFile();
  std::string content;
  CHECK(readWholeFile(kFile, kContentSize, content));
  CHECK_EQ(std::string(kContent), content);
  CHECK(!readWholeFile(kFile, kContentSize - 1, content));
  CHECK(!readWholeFile("obj/no-such-file", kContentSize, content));
}
