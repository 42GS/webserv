/**
 * @file  Clock.cpp
 * @brief Clock built on a file's modification time.
 *        파일 수정 시각으로 만든 시계.
 */
#include "core/Clock.hpp"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

Clock::Clock(const std::string& path) : path_(path), now_(0) {}

bool Clock::refresh() {
  const int fd =
      open(path_.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);  // rw-r--r--
  if (fd < 0) return false;
  const ssize_t written = write(fd, "t", 1);
  // Nothing to recover from close(); stat() below shows whether the write
  // held.
  // close()가 실패해도 할 일이 없다. 쓰기가 반영됐는지는 아래 stat()이
  // 보여 준다.
  close(fd);
  if (written != 1) return false;

  struct stat info;
  if (stat(path_.c_str(), &info) != 0) return false;
  now_ = info.st_mtime;
  return true;
}

std::time_t Clock::now() const { return now_; }
