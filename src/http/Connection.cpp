#include "http/Connection.hpp"

#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

namespace {

const std::size_t kRecvBufferSize = 65536;
const std::time_t kHeaderTimeoutSeconds = 30;
const std::time_t kSendTimeoutSeconds = 30;
const std::string::size_type kMaxHeadBytes = 16384;
const char* const kHeadTerminator = "\r\n\r\n";

const char* const kFixedResponse =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/plain\r\n"
    "Content-Length: 13\r\n"
    "Connection: close\r\n"
    "\r\n"
    "Hello, world\n";

}  // namespace

Connection::Connection(int fd, std::time_t now)
    : fd_(fd), state_(READ_HEAD), outSent_(0), now_(now), lastProgress_(now) {}

Connection::~Connection() { close(fd_); }

int Connection::fd() const { return fd_; }

short Connection::wantedEvents() const {
  if (state_ == READ_HEAD) return POLLIN;
  if (state_ == WRITE) return POLLOUT;
  return 0;
}

void Connection::onEvent(short revents) {
  if ((revents & (POLLERR | POLLNVAL)) != 0) {
    changeState(CLOSED);
    return;
  }
  // POLLHUP may come with data still unread; recv() returns 0 once the
  // data is consumed, which closes the connection.
  if (state_ == READ_HEAD && (revents & (POLLIN | POLLHUP)) != 0)
    receiveOnce();
  else if (state_ == WRITE && (revents & POLLOUT) != 0)
    sendOnce();
  else if ((revents & POLLHUP) != 0)
    changeState(CLOSED);
}

void Connection::onFailure() { changeState(CLOSED); }

void Connection::checkTimeout(std::time_t now) {
  now_ = now;
  const std::time_t idle = now - lastProgress_;
  if ((state_ == READ_HEAD && idle >= kHeaderTimeoutSeconds) ||
      (state_ == WRITE && idle >= kSendTimeoutSeconds))
    changeState(CLOSED);
}

bool Connection::isClosed() const { return state_ == CLOSED; }

void Connection::receiveOnce() {
  char buffer[kRecvBufferSize];
  const ssize_t received = recv(fd_, buffer, sizeof(buffer), 0);
  // 0: the peer closed. -1: an error. Neither looks at errno.
  if (received <= 0) {
    changeState(CLOSED);
    return;
  }
  lastProgress_ = now_;
  inBuffer_.append(buffer, static_cast<std::string::size_type>(received));
  if (inBuffer_.find(kHeadTerminator) != std::string::npos)
    startResponse();
  else if (inBuffer_.size() > kMaxHeadBytes)
    changeState(CLOSED);
}

void Connection::sendOnce() {
  const ssize_t sent =
      send(fd_, outBuffer_.data() + outSent_, outBuffer_.size() - outSent_, 0);
  if (sent <= 0) {
    changeState(CLOSED);
    return;
  }
  lastProgress_ = now_;
  outSent_ += static_cast<std::string::size_type>(sent);
  if (outSent_ == outBuffer_.size()) changeState(CLOSED);
}

void Connection::startResponse() {
  outBuffer_ = kFixedResponse;
  outSent_ = 0;
  changeState(WRITE);
}

void Connection::changeState(State next) {
  state_ = next;
  lastProgress_ = now_;
}
