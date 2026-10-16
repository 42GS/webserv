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
  close(fd);  // nothing to recover; stat() below shows whether the write held
  if (written != 1) return false;
  struct stat info;
  if (stat(path_.c_str(), &info) != 0) return false;
  now_ = info.st_mtime;
  return true;
}

std::time_t Clock::now() const { return now_; }
