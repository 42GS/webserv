#ifndef CORE_LISTENCONTEXT_HPP
#define CORE_LISTENCONTEXT_HPP

#include <string>

#include "config/ServerConfig.hpp"

// Where a connection came in: the server block that owns the listen
// address, the local address, and the peer's address.
struct ListenContext {
  const ServerConfig* server;  // owned by main, lives for the whole run
  ListenAddr local;
  std::string remoteAddress;
};

#endif
