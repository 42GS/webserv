#include "core/Listener.hpp"

#include <arpa/inet.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <sstream>

#include "core/SocketAddress.hpp"

namespace {

std::string describe(const ListenAddr& addr) {
  std::ostringstream out;
  out << addr.host << ":" << addr.port;
  return out.str();
}

}  // namespace

Listener::Listener(const ServerConfig& server, const ListenAddr& addr,
                   AcceptHandler& acceptor)
    : server_(server), addr_(addr), acceptor_(acceptor), fd_(-1) {}

// close() results are not checked in this file: the descriptor is given up
// either way and there is no caller left to report to.
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
  ok = ok && listen(fd_, listener::kBacklog) == 0;
  // Non-blocking only after bind: a non-blocking bind() may fail with
  // EINPROGRESS and finish later (POSIX bind).
  ok = ok && fcntl(fd_, F_SETFL, O_NONBLOCK) == 0;
  // errno is read only to build the startup error message, never after a
  // read or write on a client socket.
  if (!ok)
    error = "cannot listen on " + describe(addr_) + ": " + std::strerror(errno);
  return ok;
}

int Listener::fd() const { return fd_; }

short Listener::wantedEvents() const { return POLLIN; }

void Listener::onEvent(short revents) {
  if ((revents & POLLIN) == 0) return;
  sockaddr_in peer;
  socklen_t peerLength = sizeof(peer);
  const int clientFd =
      accept(fd_, reinterpret_cast<sockaddr*>(&peer), &peerLength);
  // A failed accept (for example the client already gave up) is not an
  // error for the server; the next readiness will try again.
  if (clientFd < 0) return;
  if (fcntl(clientFd, F_SETFL, O_NONBLOCK) < 0) {
    close(clientFd);
    return;
  }
  ListenContext where;
  where.server = &server_;
  where.local = addr_;
  where.remoteAddress = formatIpv4(ntohl(peer.sin_addr.s_addr));
  acceptor_.adopt(clientFd, where);
}

// A listen socket has no per-request state; there is nothing to undo.
void Listener::onFailure() {}
