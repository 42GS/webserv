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

Server::Server(const std::vector<ServerConfig>& configs, IoRegistry& registry,
               std::time_t now)
    : configs_(configs), registry_(registry), now_(now) {}

Server::~Server() {
  while (!connections_.empty()) closeConnection(connections_.begin()->second);
  for (std::size_t i = 0; i < listeners_.size(); ++i) {
    registry_.unwatch(listeners_[i]);
    delete listeners_[i];
  }
}

bool Server::openListeners(std::string& error) {
  for (std::size_t i = 0; i < configs_.size(); ++i) {
    const ServerConfig& config = configs_[i];
    for (std::size_t j = 0; j < config.listens.size(); ++j) {
      Listener* listener = new Listener(config, config.listens[j], *this);
      listeners_.push_back(listener);
      if (!listener->open(error)) return false;
      registry_.watch(listener);
    }
  }
  return true;
}

void Server::adopt(int clientFd, const ListenContext& where) {
  Connection* connection = 0;
  try {
    connection = new Connection(clientFd, where, now_);
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
  registry_.watch(connection);
}

// A timeout may build a 408 response; if that throws (std::bad_alloc), only
// that connection is given up, as EventLoop does for onEvent().
// 시간 초과는 408 응답을 만들 수 있다. 그때 예외(std::bad_alloc)가 나면,
// EventLoop가 onEvent()에서 하듯이 그 연결만 포기한다.
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
}

bool Server::shouldStop() const { return signals::stopRequested(); }

void Server::closeConnection(Connection* connection) {
  registry_.unwatch(connection);
  connections_.erase(connection->fd());
  delete connection;
}
