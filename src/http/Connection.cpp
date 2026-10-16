#include "http/Connection.hpp"

#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <sstream>

#include "config/ConfigLimits.hpp"
#include "http/Router.hpp"
#include "http/StatusText.hpp"

namespace {

const std::size_t kRecvBufferSize = 65536;
const std::time_t kHeaderTimeoutSeconds = 30;
const std::time_t kBodyTimeoutSeconds = 30;
const std::time_t kSendTimeoutSeconds = 30;
const int kRequestTimeout = 408;

const char* const kFixedResponse =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/plain\r\n"
    "Content-Length: 13\r\n"
    "Connection: close\r\n"
    "\r\n"
    "Hello, world\n";

// A minimal error response until the error pages of step 4. The status
// line keeps its SP even without a reason phrase (RFC 9112 section 4).
std::string errorResponse(int status) {
  std::ostringstream body;
  body << status << " " << reasonPhrase(status) << "\n";
  std::ostringstream out;
  out << "HTTP/1.1 " << status << " " << reasonPhrase(status) << "\r\n"
      << "Content-Type: text/plain\r\n"
      << "Content-Length: " << body.str().size() << "\r\n"
      << "Connection: close\r\n"
      << "\r\n"
      << body.str();
  return out.str();
}

}  // namespace

Connection::Connection(int fd, const ListenContext& where, std::time_t now)
    : fd_(fd),
      where_(where),
      state_(READ_HEAD),
      parser_(defaultParserLimits()),
      outSent_(0),
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

// The head must arrive within kHeaderTimeoutSeconds in total, so a client
// that sends a byte now and then cannot hold the connection (design 12).
// Body and send timeouts count only time without progress. An unfinished
// request gets 408 before closing (RFC 9112 section 8 allows it); a
// connection that never sent anything is just closed.
void Connection::checkTimeout(std::time_t now) {
  now_ = now;
  const std::time_t idle = now - lastProgress_;
  if ((state_ == READ_HEAD && now - stateSince_ >= kHeaderTimeoutSeconds) ||
      (state_ == READ_BODY && idle >= kBodyTimeoutSeconds)) {
    if (parser_.hasStarted())
      startErrorResponse(kRequestTimeout);
    else
      changeState(CLOSED);
  } else if (state_ == WRITE && idle >= kSendTimeoutSeconds) {
    changeState(CLOSED);
  }
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
  parseInput();
}

void Connection::parseInput() {
  for (;;) {
    switch (parser_.feed(inBuffer_)) {
      case RequestParser::NEED_MORE:
        return;
      case RequestParser::HEAD_DONE:
        parser_.setMaxBodySize(bodyLimitFor(parser_.request()));
        changeState(READ_BODY);
        break;
      case RequestParser::DONE:
        startResponse(kFixedResponse);
        return;
      case RequestParser::ERROR:
        startErrorResponse(parser_.errorStatus());
        return;
    }
  }
}

// The limit of the matching location. The path is matched before
// percent-decoding and dot-segment removal, which come with the router in
// step 4. Without a matching location the request will be 404, and the
// NGINX default limit applies to its body.
std::size_t Connection::bodyLimitFor(const Request& request) const {
  const LocationConfig* location =
      Router(*where_.server).matchLocation(request.path);
  return location == 0 ? config_limits::kDefaultMaxBodySize
                       : location->maxBodySize;
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

void Connection::startResponse(const std::string& bytes) {
  outBuffer_ = bytes;
  outSent_ = 0;
  changeState(WRITE);
}

void Connection::startErrorResponse(int status) {
  startResponse(errorResponse(status));
}

void Connection::changeState(State next) {
  state_ = next;
  stateSince_ = now_;
  lastProgress_ = now_;
}
