#ifndef CONFIG_CONFIGBUILDER_HPP
#define CONFIG_CONFIGBUILDER_HPP

#include "config/ConfigSyntax.hpp"
#include "config/ServerConfig.hpp"

#include <string>
#include <vector>

// Turns the directive tree into validated server settings: checks each
// directive's arguments, applies server-level values to locations, and
// rejects conflicting listen addresses. Throws ConfigError.
std::vector<ServerConfig> buildServerConfigs(const DirectiveTree& tree, const std::string& source);

#endif
