#ifndef CORE_SERVER_HPP
#define CORE_SERVER_HPP

#include <ctime>
#include <map>
#include <string>
#include <vector>

#include "config/ServerConfig.hpp"
#include "core/IoHandler.hpp"
#include "core/Listener.hpp"

class Connection;

// Owns every Listener and Connection, registers them with the event loop,
// and deletes connections once they are closed.
class Server : public LoopObserver, public AcceptHandler {
 public:
  Server(const std::vector<ServerConfig>& configs, IoRegistry& registry,
         std::time_t now);
  ~Server();

  // Opens every listen address. On failure returns false with a message;
  // listeners opened so far are closed by the destructor.
  bool openListeners(std::string& error);

  void adopt(int clientFd, const ListenContext& where);
  void afterIteration(std::time_t now);
  bool shouldStop() const;

 private:
  Server(const Server&);
  Server& operator=(const Server&);

  void closeConnection(Connection* connection);

  const std::vector<ServerConfig>& configs_;
  IoRegistry& registry_;
  std::time_t now_;
  std::vector<Listener*> listeners_;
  std::map<int, Connection*> connections_;
};

#endif
