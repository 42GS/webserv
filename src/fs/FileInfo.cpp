#include "fs/FileInfo.hpp"

#include <sys/stat.h>
#include <unistd.h>

FileKind fileKind(const std::string& path) {
  struct stat info;
  if (stat(path.c_str(), &info) != 0) return FILE_MISSING;
  if (S_ISREG(info.st_mode)) return FILE_REGULAR;
  if (S_ISDIR(info.st_mode)) return FILE_DIRECTORY;
  return FILE_OTHER;
}

bool isReadable(const std::string& path) {
  return access(path.c_str(), R_OK) == 0;
}

bool regularFileSize(const std::string& path, std::size_t& size) {
  struct stat info;
  if (stat(path.c_str(), &info) != 0 || !S_ISREG(info.st_mode)) return false;
  size = static_cast<std::size_t>(info.st_size);
  return true;
}
