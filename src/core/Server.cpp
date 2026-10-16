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

// The request handling that uses "where" (server block, addresses) comes
// with the request parser; the fixed response does not need it yet.
void Server::adopt(int clientFd, const ListenContext& /* where */) {
  Connection* connection = 0;
  try {
    connection = new Connection(clientFd, now_);
    connections_[clientFd] = connection;
  } catch (const std::exception&) {
    delete connection;  // closes clientFd if construction succeeded
    if (connection == 0) close(clientFd);  // result unused: fd is dropped
    return;
  }
  registry_.watch(connection);
}

void Server::afterIteration(std::time_t now) {
  now_ = now;
  std::vector<Connection*> closed;
  for (std::map<int, Connection*>::iterator it = connections_.begin();
       it != connections_.end(); ++it) {
    it->second->checkTimeout(now);
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
