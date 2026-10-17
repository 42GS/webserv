/**
 * @file  Connection.cpp
 * @brief Reading, timeouts and sending of one client connection.
 *        클라이언트 연결 하나의 읽기, 시간 초과, 보내기.
 */
#include "http/Connection.hpp"

#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include "core/Limits.hpp"
#include "http/StatusCodes.hpp"

Connection::Connection(int fd, const ListenContext& where,
                       const CgiServices& services, std::time_t now)
    : fd_(fd),
      state_(READ_HEAD),
      processor_(where, services),
      cgi_(0),
      current_(0),
      localRedirects_(0),
      keepAlive_(false),
      requestConsumed_(false),
      now_(now),
      stateSince_(now),
      lastProgress_(now) {}

// The connection is over; a close() error leaves nothing to do.
// 연결이 끝났으므로 close()가 실패해도 할 일이 없다.
Connection::~Connection() {
  delete cgi_;
  close(fd_);
}

int Connection::fd() const { return fd_; }

short Connection::wantedEvents() const {
  if (state_ == READ_HEAD || state_ == READ_BODY || state_ == DRAIN)
    return POLLIN;
  if (state_ == WRITE) return POLLOUT;
  return 0;
}

void Connection::onEvent(short revents) {
  if ((revents & (POLLERR | POLLNVAL)) != 0) {
    changeState(CLOSED);
    return;
  }
  const bool reading = (wantedEvents() == POLLIN);
  // POLLHUP may come with data still unread; recv() returns 0 once the
  // data is consumed, which closes the connection.
  // POLLHUP과 함께 아직 읽지 않은 데이터가 있을 수 있다. 데이터를 다
  // 읽으면 recv()가 0을 돌려주고, 그때 연결을 닫는다.
  if (reading && (revents & (POLLIN | POLLHUP)) != 0)
    receiveOnce();
  else if (state_ == WRITE && (revents & POLLOUT) != 0)
    sendOnce();
  else if ((revents & POLLHUP) != 0)
    changeState(CLOSED);
}

void Connection::onFailure() { changeState(CLOSED); }

// An unfinished request gets 408 before closing (RFC 9112 section 8 allows
// it); a connection waiting for a request, sending or draining is just
// closed.
// 끝나지 않은 요청에는 닫기 전에 408을 보낸다 (RFC 9112 8절이 허용).
// 요청을 기다리거나, 보내거나, 버리던 연결은 그냥 닫는다.
void Connection::checkTimeout(std::time_t now) {
  now_ = now;
  if (state_ == WAIT_CGI) {
    followCgi();
    return;
  }
  if (!hasTimedOut()) return;
  const bool requestUnfinished =
      (state_ == READ_HEAD || state_ == READ_BODY) && parser_.hasStarted();
  if (requestUnfinished)
    startErrorResponse(http_status::kRequestTimeout);
  else
    changeState(CLOSED);
}

// The head must arrive within limits::kHeaderTimeoutSeconds of its first
// byte, so a client that sends a byte now and then cannot hold the
// connection (design 12). Body and send timeouts count only time without
// progress. The clock counts whole seconds, so a difference of N may be
// only a moment past a tick; more than N is always over N real seconds.
// 머리는 첫 바이트부터 모두 합쳐 limits::kHeaderTimeoutSeconds 안에 와야
// 한다. 그래서 가끔 한 바이트씩 보내는 클라이언트가 연결을 붙잡을 수
// 없다 (설계 12절). 본문과 전송 시간 초과는 진행이 없는 시간만 센다.
// 시계는 초 단위라 차이가 N이어도 실제로는 초가 바뀐 직후일 수 있다. N보다
// 커야 실제로 N초가 넘게 지난 것이다.
bool Connection::hasTimedOut() const {
  const std::time_t elapsed = now_ - stateSince_;
  const std::time_t idle = now_ - lastProgress_;
  switch (state_) {
    case READ_HEAD:
      if (!parser_.hasStarted())
        return elapsed > limits::kKeepAliveTimeoutSeconds;
      return elapsed > limits::kHeaderTimeoutSeconds;
    case READ_BODY:
      return idle > limits::kBodyTimeoutSeconds;
    case WAIT_CGI:
      // The CGI process keeps its own time (followCgi()).
      // CGI 프로세스가 자기 시간을 잰다 (followCgi()).
      return false;
    case WRITE:
      return idle > limits::kSendTimeoutSeconds;
    case DRAIN:
      return idle > limits::kDrainIdleSeconds ||
             elapsed > limits::kDrainTotalSeconds;
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
  // 0: 상대가 닫음. -1: 오류. 어느 쪽도 errno를 보지 않는다.
  if (received <= 0) {
    changeState(CLOSED);
    return;
  }
  lastProgress_ = now_;
  if (state_ == DRAIN) return;
  // The head timeout starts with the first byte of a request.
  // 머리 시간 제한은 요청의 첫 바이트부터 잰다.
  if (state_ == READ_HEAD && !parser_.hasStarted()) stateSince_ = now_;
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
        requestConsumed_ = true;
        current_ = &parser_.request();
        localRedirects_ = 0;
        handleOutcome(processor_.process(*current_, now_));
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
  // 더 읽을 수 없는 파일 본문은 이미 보낸 Content-Length와 맞출 수 없으니
  // 연결을 끝내야 한다.
  if (!stream_.consume(static_cast<std::size_t>(sent)))
    changeState(CLOSED);
  else if (!stream_.hasPending())
    finishResponse();
}

// A pipelined request may already be in inBuffer_. Closing at once with
// part of a request unread risks a reset that erases the response at the
// client (RFC 9112 section 9.6), hence DRAIN.
// 파이프라인으로 온 다음 요청이 이미 inBuffer_에 있을 수 있다. 요청을 다
// 읽지 않고 바로 닫으면 reset이 클라이언트의 응답을 지울 수 있으므로
// (RFC 9112 9.6절) DRAIN을 거친다.
void Connection::finishResponse() {
  if (!keepAlive_) {
    changeState(requestConsumed_ ? CLOSED : DRAIN);
    return;
  }
  parser_.reset();
  changeState(READ_HEAD);
  parseInput();
}

// The 500 for a vanished file is built in memory, so it always starts.
// 사라진 파일 대신 보내는 500은 메모리에서 만들므로 항상 시작된다.
void Connection::handleOutcome(const ProcessOutcome& outcome) {
  switch (outcome.kind) {
    case ProcessOutcome::RESPONSE:
      startResponse(outcome.response);
      return;
    case ProcessOutcome::CGI:
      cgi_ = outcome.cgi;
      changeState(WAIT_CGI);
      return;
    case ProcessOutcome::LOCAL_REDIRECT:
      break;
  }
  if (++localRedirects_ > limits::kMaxInternalRedirects) {
    startResponse(processor_.statusResponse(http_status::kInternalServerError,
                                            *current_, now_));
    return;
  }
  redirected_ = outcome.redirect;
  current_ = &redirected_;
  handleOutcome(processor_.process(redirected_, now_));
}

void Connection::followCgi() {
  if (cgi_->isDone()) {
    const ProcessOutcome outcome = processor_.afterCgi(*current_, *cgi_, now_);
    delete cgi_;
    cgi_ = 0;
    handleOutcome(outcome);
  } else if (cgi_->hasTimedOut(now_)) {
    delete cgi_;
    cgi_ = 0;
    startResponse(processor_.statusResponse(http_status::kGatewayTimeout,
                                            *current_, now_));
  }
}

void Connection::startResponse(const Response& response) {
  if (stream_.reset(response)) {
    keepAlive_ = response.keepAlive;
  } else {
    static_cast<void>(stream_.reset(processor_.errorResponse(
        http_status::kInternalServerError, parser_.request().method, now_)));
    keepAlive_ = false;
  }
  changeState(WRITE);
}

void Connection::startErrorResponse(int status) {
  requestConsumed_ = false;
  startResponse(
      processor_.errorResponse(status, parser_.request().method, now_));
}

void Connection::changeState(State next) {
  state_ = next;
  stateSince_ = now_;
  lastProgress_ = now_;
}
