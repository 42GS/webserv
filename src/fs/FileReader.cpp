/**
 * @file  FileReader.cpp
 * @brief Whole-file reading.
 *        파일 전체 읽기.
 */
#include "fs/FileReader.hpp"

#include <fcntl.h>
#include <unistd.h>

#include "core/Limits.hpp"

bool readWholeFile(const std::string& path, std::size_t maxSize,
                   std::string& content) {
  const int fd = open(path.c_str(), O_RDONLY);
  if (fd < 0) return false;

  content.clear();
  char buffer[limits::kFileReadSize];
  bool ok = true;
  for (;;) {
    const ssize_t got = read(fd, buffer, sizeof(buffer));
    if (got == 0) break;
    if (got < 0 || content.size() + static_cast<std::size_t>(got) > maxSize) {
      ok = false;
      break;
    }
    content.append(buffer, static_cast<std::size_t>(got));
  }

  // Read-only descriptor: a close() error loses nothing.
  // 읽기 전용 디스크립터라 close()가 실패해도 잃는 것이 없다.
  close(fd);
  return ok;
}
