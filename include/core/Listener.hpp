#ifndef CORE_LISTENER_HPP
#define CORE_LISTENER_HPP

#include <sys/socket.h>

#include <string>

#include "config/ServerConfig.hpp"
#include "core/IoHandler.hpp"
#include "core/ListenContext.hpp"

// Receives sockets accepted by a Listener.
namespace listener {

const int kBacklog = SOMAXCONN;  // the system's largest listen queue

}  // namespace listener

class AcceptHandler {
 public:
  virtual ~AcceptHandler() {}
  virtual void adopt(int clientFd, const ListenContext& where) = 0;
};

// A non-blocking listen socket for one interface:port.
class Listener : public IoHandler {
 public:
  Listener(const ServerConfig& server, const ListenAddr& addr,
           AcceptHandler& acceptor);
  ~Listener();

  // socket, SO_REUSEADDR, non-blocking, bind, listen. On failure returns
  // false and sets error to a message naming the address and the cause.
  bool open(std::string& error);

  int fd() const;
  short wantedEvents() const;
  void onEvent(short revents);  // accept() once
  void onFailure();

 private:
  Listener(const Listener&);
  Listener& operator=(const Listener&);

  const ServerConfig& server_;
  ListenAddr addr_;
  AcceptHandler& acceptor_;
  int fd_;
};

#endif
