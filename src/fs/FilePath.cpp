/**
 * @file  FilePath.cpp
 * @brief Path splitting.
 *        경로 나누기.
 */
#include "fs/FilePath.hpp"

std::string parentDirectory(const std::string& path) {
  const std::string::size_type slash = path.rfind('/');
  if (slash == std::string::npos) return ".";
  return slash == 0 ? "/" : path.substr(0, slash);
}

std::string baseName(const std::string& path) {
  return path.substr(path.rfind('/') + 1);
}
