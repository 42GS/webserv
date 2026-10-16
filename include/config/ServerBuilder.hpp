#ifndef CONFIG_SERVERBUILDER_HPP
#define CONFIG_SERVERBUILDER_HPP

#include <vector>

#include "config/BuildContext.hpp"
#include "config/ServerConfig.hpp"

struct BuiltServer {
  ServerConfig config;
  // Line of each entry in config.listens, for error messages.
  std::vector<int> listenLines;
};

// Builds one "server { ... }" block, including its locations.
BuiltServer buildServer(const BuildContext& context, const Directive& block);

#endif
