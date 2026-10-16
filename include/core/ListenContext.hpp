#ifndef CORE_LISTENCONTEXT_HPP
#define CORE_LISTENCONTEXT_HPP

#include "config/ServerConfig.hpp"

// Where a connection came in: the server block that owns the listen
// address. The local and peer addresses come back when CGI needs them
// (step 7).
struct ListenContext {
  const ServerConfig* server;  // owned by main, lives for the whole run
};

#endif
