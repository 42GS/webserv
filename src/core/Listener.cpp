/**
 * @file  Listener.cpp
 * @brief Listen sockets.
 *        listen 소켓.
 */
#include "core/Listener.hpp"

#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>

#include "config/ConfigValues.hpp"
#include "core/SocketAddress.hpp"

Listener::Listener(const ServerConfig& server, const ListenAddr& addr,
                   AcceptHandler& acceptor)
    : server_(server),
      addr_(addr),
      acceptor_(acceptor),
      fd_(-1),
      paused_(false) {}

// close() results are not checked in this file: the descriptor is given up
// either way and there is no caller left to report to.
// 이 파일에서는 close() 결과를 보지 않는다. 어느 쪽이든 디스크립터는
// 버려지고, 알릴 호출자도 없다.
Listener::~Listener() {
  if (fd_ >= 0) close(fd_);
}

bool Listener::open(std::string& error) {
  const int enable = 1;
  const sockaddr_in address = toSockaddr(addr_);
  const sockaddr* rawAddress = reinterpret_cast<const sockaddr*>(&address);
  bool ok = (fd_ = socket(AF_INET, SOCK_STREAM, 0)) >= 0;
  ok = ok &&
       setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) == 0;
  ok = ok && bind(fd_, rawAddress, sizeof(address)) == 0;
  // SOMAXCONN: the longest queue the system allows.
  // SOMAXCONN: 시스템이 허용하는 가장 긴 대기열.
  ok = ok && listen(fd_, SOMAXCONN) == 0;
  // Non-blocking only after bind: a non-blocking bind() may fail with
  // EINPROGRESS and finish later (POSIX bind).
  // bind 뒤에 non-blocking으로 바꾼다. non-blocking bind()는 EINPROGRESS로
  // 실패하고 나중에 끝날 수 있다 (POSIX bind).
  ok = ok && fcntl(fd_, F_SETFL, O_NONBLOCK) == 0;
  // errno is read only to build the startup error message, never after a
  // read or write on a client socket.
  // errno는 시작할 때의 오류 메시지를 만들 때만 읽는다. 클라이언트
  // 소켓의 읽기·쓰기 뒤에는 읽지 않는다.
  if (!ok)
    error = "cannot listen on " + formatListenAddr(addr_) + ": " +
            std::strerror(errno);
  return ok;
}

int Listener::fd() const { return fd_; }

short Listener::wantedEvents() const { return paused_ ? 0 : POLLIN; }

void Listener::onEvent(short revents) {
  if ((revents & POLLIN) == 0) return;
  sockaddr_in remote = sockaddr_in();
  socklen_t remoteSize = sizeof(remote);
  const int clientFd =
      accept(fd_, reinterpret_cast<sockaddr*>(&remote), &remoteSize);
  // A failed accept (the client already gave up, or no descriptor is
  // free) is not an error for the server; it tries again after resume().
  // accept 실패(클라이언트가 이미 포기했거나 남은 fd가 없음)는 서버의
  // 오류가 아니다. resume() 뒤에 다시 시도한다.
  if (clientFd < 0) {
    paused_ = true;
    return;
  }
  // The local end tells which address a "0.0.0.0" listener was reached on.
  // "0.0.0.0" listener가 어느 주소로 연결됐는지는 로컬 끝이 알려 준다.
  sockaddr_in local = sockaddr_in();
  socklen_t localSize = sizeof(local);
  if (fcntl(clientFd, F_SETFL, O_NONBLOCK) < 0 ||
      getsockname(clientFd, reinterpret_cast<sockaddr*>(&local), &localSize) <
          0) {
    close(clientFd);
    return;
  }
  ListenContext where;
  where.server = &server_;
  where.remoteAddress = formatIpv4(remote.sin_addr.s_addr);
  where.localAddress = formatIpv4(local.sin_addr.s_addr);
  where.localPort = ntohs(local.sin_port);
  acceptor_.adopt(clientFd, where);
}

void Listener::onFailure() {}

void Listener::resume() { paused_ = false; }
