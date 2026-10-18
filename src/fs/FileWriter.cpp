/**
 * @file  FileWriter.cpp
 * @brief New-file writing.
 *        새 파일 쓰기.
 */
#include "fs/FileWriter.hpp"

#include <fcntl.h>
#include <unistd.h>

#include <cstdio>

namespace {

/** rw-r--r--, before the umask. / umask 적용 전의 rw-r--r--. */
const mode_t kNewFileMode = 0644;

}  // namespace

bool writeNewFile(const std::string& path, const char* data, std::size_t size) {
  const int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL, kNewFileMode);
  if (fd < 0) return false;

  bool ok = true;
  std::size_t written = 0;
  while (ok && written < size) {
    const ssize_t put = write(fd, data + written, size - written);
    // 0 for a non-zero count would repeat forever, so it fails too.
    // 0이 아닌 크기에 0이 돌아오면 끝없이 반복하므로 이것도 실패로 본다.
    if (put <= 0)
      ok = false;
    else
      written += static_cast<std::size_t>(put);
  }

  // close() can report a write error that came late.
  // close()가 늦게 드러난 쓰기 오류를 알릴 수 있다.
  if (close(fd) != 0) ok = false;
  // std::remove: unlink() is not on the list of allowed functions (design
  // decision 15). If it fails too, nothing more can be done here.
  // std::remove: unlink()는 허용 함수 목록에 없다 (설계 결정 15). 이것도
  // 실패하면 여기서 더 할 수 있는 것이 없다.
  if (!ok) static_cast<void>(std::remove(path.c_str()));
  return ok;
}
