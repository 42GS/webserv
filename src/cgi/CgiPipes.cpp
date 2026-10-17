/**
 * @file  CgiPipes.cpp
 * @brief Pipe I/O with a CGI script.
 *        CGI 스크립트와의 pipe 입출력.
 */
#include "cgi/CgiPipes.hpp"

#include <poll.h>
#include <unistd.h>

#include "core/Limits.hpp"

CgiStdin::CgiStdin(int fd, const std::string& body, IoRegistry& registry)
    : fd_(fd), body_(body), written_(0), registry_(registry) {}

CgiStdin::~CgiStdin() { close(); }

int CgiStdin::fd() const { return fd_; }

short CgiStdin::wantedEvents() const { return fd_ < 0 ? 0 : POLLOUT; }

void CgiStdin::onEvent(short revents) {
  if ((revents & (POLLERR | POLLHUP | POLLNVAL)) != 0 ||
      (revents & POLLOUT) == 0) {
    close();
    return;
  }
  std::size_t size = body_.size() - written_;
  if (size > limits::kPipeIoSize) size = limits::kPipeIoSize;
  const ssize_t put = write(fd_, body_.data() + written_, size);
  // -1: the script stopped reading (errno is not read; SIGPIPE is
  // ignored). / -1: 스크립트가 읽기를 멈췄다 (errno는 읽지 않는다.
  // SIGPIPE는 무시한다).
  if (put <= 0) {
    close();
    return;
  }
  written_ += static_cast<std::size_t>(put);
  if (written_ == body_.size()) close();
}

void CgiStdin::onFailure() { close(); }

void CgiStdin::close() {
  if (fd_ < 0) return;
  registry_.unwatch(this);
  ::close(fd_);
  fd_ = -1;
}

CgiStdout::CgiStdout(int fd, IoRegistry& registry)
    : fd_(fd), failed_(false), registry_(registry) {}

CgiStdout::~CgiStdout() { close(false); }

int CgiStdout::fd() const { return fd_; }

short CgiStdout::wantedEvents() const { return fd_ < 0 ? 0 : POLLIN; }

void CgiStdout::onEvent(short revents) {
  if ((revents & POLLNVAL) != 0) {
    close(true);
    return;
  }
  char buffer[limits::kPipeIoSize];
  const ssize_t got = read(fd_, buffer, sizeof(buffer));
  if (got == 0) {
    close(false);
  } else if (got < 0) {
    close(true);
  } else {
    data_.append(buffer, static_cast<std::size_t>(got));
    if (data_.size() > limits::kMaxCgiOutputBytes) close(true);
  }
}

void CgiStdout::onFailure() { close(true); }

bool CgiStdout::isClosed() const { return fd_ < 0; }

bool CgiStdout::failed() const { return failed_; }

const std::string& CgiStdout::data() const { return data_; }

void CgiStdout::close(bool failed) {
  if (fd_ < 0) return;
  failed_ = failed;
  registry_.unwatch(this);
  ::close(fd_);
  fd_ = -1;
}
