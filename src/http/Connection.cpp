#include "http/Connection.hpp"

#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include "core/Limits.hpp"
#include "http/StatusCodes.hpp"

Connection::Connection(int fd, const ListenContext& where, std::time_t now)
    : fd_(fd),
      state_(READ_HEAD),
      parser_(defaultParserLimits()),
      processor_(*where.server),
      now_(now),
      stateSince_(now),
      lastProgress_(now) {}

// The connection is over; a close() error leaves nothing to do.
Connection::~Connection() { close(fd_); }

int Connection::fd() const { return fd_; }

short Connection::wantedEvents() const {
  if (state_ == READ_HEAD || state_ == READ_BODY) return POLLIN;
  if (state_ == WRITE) return POLLOUT;
  return 0;
}

void Connection::onEvent(short revents) {
  if ((revents & (POLLERR | POLLNVAL)) != 0) {
    changeState(CLOSED);
    return;
  }
  const bool reading = (state_ == READ_HEAD || state_ == READ_BODY);
  // POLLHUP may come with data still unread; recv() returns 0 once the
  // data is consumed, which closes the connection.
  if (reading && (revents & (POLLIN | POLLHUP)) != 0)
    receiveOnce();
  else if (state_ == WRITE && (revents & POLLOUT) != 0)
    sendOnce();
  else if ((revents & POLLHUP) != 0)
    changeState(CLOSED);
}

void Connection::onFailure() { changeState(CLOSED); }

// An unfinished request gets 408 before closing (RFC 9112 section 8 allows
// it); a connection that never sent anything is just closed.
void Connection::checkTimeout(std::time_t now) {
  now_ = now;
  if (!hasTimedOut()) return;
  if (state_ == WRITE || !parser_.hasStarted())
    changeState(CLOSED);
  else
    startErrorResponse(http_status::kRequestTimeout);
}

// The head must arrive within limits::kHeaderTimeoutSeconds in total, so a
// client that sends a byte now and then cannot hold the connection (design
// 12). Body and send timeouts count only time without progress.
bool Connection::hasTimedOut() const {
  const std::time_t idle = now_ - lastProgress_;
  switch (state_) {
    case READ_HEAD:
      return now_ - stateSince_ >= limits::kHeaderTimeoutSeconds;
    case READ_BODY:
      return idle >= limits::kBodyTimeoutSeconds;
    case WRITE:
      return idle >= limits::kSendTimeoutSeconds;
    case CLOSED:
      break;
  }
  return false;
}

bool Connection::isClosed() const { return state_ == CLOSED; }

void Connection::receiveOnce() {
  char buffer[limits::kRecvBufferSize];
  const ssize_t received = recv(fd_, buffer, sizeof(buffer), 0);
  // 0: the peer closed. -1: an error. Neither looks at errno.
  if (received <= 0) {
    changeState(CLOSED);
    return;
  }
  lastProgress_ = now_;
  inBuffer_.append(buffer, static_cast<std::string::size_type>(received));
  parseInput();
}

void Connection::parseInput() {
  for (;;) {
    switch (parser_.feed(inBuffer_)) {
      case RequestParser::NEED_MORE:
        return;
      case RequestParser::HEAD_DONE:
        parser_.setMaxBodySize(processor_.bodyLimitFor(parser_.request()));
        changeState(READ_BODY);
        break;
      case RequestParser::DONE:
        startResponse(processor_.process(parser_.request(), now_));
        return;
      case RequestParser::ERROR:
        startErrorResponse(parser_.errorStatus());
        return;
    }
  }
}

void Connection::sendOnce() {
  const ssize_t sent = send(fd_, stream_.data(), stream_.size(), 0);
  if (sent <= 0) {
    changeState(CLOSED);
    return;
  }
  lastProgress_ = now_;
  // A file body that cannot be read further cannot match the
  // Content-Length already sent: the connection has to end.
  if (!stream_.consume(static_cast<std::size_t>(sent)) || !stream_.hasPending())
    changeState(CLOSED);
}

// A file that vanished between routing and opening becomes a 500, which
// is built in memory and always starts.
void Connection::startResponse(const Response& response) {
  if (!stream_.reset(response))
    static_cast<void>(stream_.reset(processor_.errorResponse(
        http_status::kInternalServerError, parser_.request().method, now_)));
  changeState(WRITE);
}

void Connection::startErrorResponse(int status) {
  startResponse(
      processor_.errorResponse(status, parser_.request().method, now_));
}

void Connection::changeState(State next) {
  state_ = next;
  stateSince_ = now_;
  lastProgress_ = now_;
}
