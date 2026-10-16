#ifndef CONFIG_SERVERBUILDER_HPP
#define CONFIG_SERVERBUILDER_HPP

#include "config/BuildContext.hpp"
#include "config/ServerConfig.hpp"

#include <vector>

struct BuiltServer {
    ServerConfig     config;
    std::vector<int> listenLines;   // line of each entry in config.listens, for error messages
};

// Builds one "server { ... }" block, including its locations.
BuiltServer buildServer(const BuildContext& context, const Directive& block);

#endif
