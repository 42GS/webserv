/**
 * @file  Server.cpp
 * @brief Ownership and timeouts of listeners and connections.
 *        listener와 연결의 소유와 시간 초과 처리.
 */
#include "core/Server.hpp"

#include <unistd.h>

#include <exception>

#include "core/Signals.hpp"
#include "http/Connection.hpp"

Server::Server(const std::vector<ServerConfig>& configs,
               const ConnectionServices& services, std::time_t now)
    : configs_(configs),
      services_(services),
      now_(now),
      listenersResumedAt_(now) {}

Server::~Server() {
  while (!connections_.empty()) closeConnection(connections_.begin()->second);
  for (std::size_t i = 0; i < listeners_.size(); ++i) {
    services_.registry.unwatch(listeners_[i]);
    delete listeners_[i];
  }
}

bool Server::openListeners(std::string& error) {
  for (std::size_t i = 0; i < configs_.size(); ++i) {
    const ServerConfig& config = configs_[i];
    for (std::size_t j = 0; j < config.listens.size(); ++j) {
      // Deleted here if push_back() fails, before listeners_ owns it. No
      // std::auto_ptr: it is deprecated in later standards, which the code
      // must also build with (raw/subject.md:49-50).
      // listeners_가 갖기 전에 push_back()이 실패하면 여기서 지운다.
      // std::auto_ptr는 이후 표준에서 deprecated이고, 코드는 그 표준으로도
      // 빌드돼야 해서 쓰지 않는다 (raw/subject.md:49-50).
      Listener* listener = new Listener(config, config.listens[j], *this);
      try {
        listeners_.push_back(listener);
      } catch (...) {
        delete listener;
        throw;
      }
      if (!listener->open(error)) return false;
      services_.registry.watch(listener);
    }
  }
  return true;
}

void Server::adopt(int clientFd, const ListenContext& where) {
  Connection* connection = 0;
  try {
    connection = new Connection(clientFd, where, services_, now_);
    connections_[clientFd] = connection;
  } catch (const std::exception&) {
    // If construction succeeded, deleting the connection closes clientFd;
    // otherwise it is closed here (the result is unused: the fd is dropped).
    // 생성에 성공했다면 연결을 지울 때 clientFd도 닫힌다. 아니면 여기서
    // 닫는다 (fd를 버리므로 결과는 보지 않는다).
    delete connection;
    if (connection == 0) close(clientFd);
    return;
  }
  services_.registry.watch(connection);
}

// checkTimeout() may build a response (408, 504, or the result of a CGI
// script); if that throws (std::bad_alloc), only that connection is given
// up, as EventLoop does for onEvent().
// checkTimeout()은 응답(408, 504, CGI 스크립트의 결과)을 만들 수 있다.
// 그때 예외(std::bad_alloc)가 나면, EventLoop가 onEvent()에서 하듯이 그
// 연결만 포기한다.
void Server::afterIteration(std::time_t now) {
  now_ = now;
  std::vector<Connection*> closed;
  for (std::map<int, Connection*>::iterator it = connections_.begin();
       it != connections_.end(); ++it) {
    try {
      it->second->checkTimeout(now);
    } catch (const std::exception&) {
      it->second->onFailure();
    }
    if (it->second->isClosed()) closed.push_back(it->second);
  }

  for (std::size_t i = 0; i < closed.size(); ++i) closeConnection(closed[i]);
  services_.reaper.reap();

  // A paused listener is retried once a descriptor may be free: a
  // connection closed, or a second passed (the clock's step).
  // 멈춘 listener는 fd가 풀렸을 수 있을 때 다시 시도한다: 연결이 닫혔거나
  // 1초가 지났을 때 (시계의 단위).
  if (!closed.empty() || now != listenersResumedAt_) {
    for (std::size_t i = 0; i < listeners_.size(); ++i) listeners_[i]->resume();
    listenersResumedAt_ = now;
  }
}

// Logged once per pause: the listener stops polling until resume().
// 멈출 때마다 한 번 남는다. listener는 resume() 전까지 poll하지 않는다.
void Server::acceptFailed(const std::string& listenAddress) {
  services_.logger.error(now_, "accept() failed on " + listenAddress +
                                   "; listener paused (descriptors used up?)");
}

bool Server::shouldStop() const { return signals::stopRequested(); }

void Server::closeConnection(Connection* connection) {
  services_.registry.unwatch(connection);
  connections_.erase(connection->fd());
  delete connection;
}
