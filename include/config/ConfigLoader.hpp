#ifndef CONFIG_CONFIGLOADER_HPP
#define CONFIG_CONFIGLOADER_HPP

#include <string>
#include <vector>

#include "config/ServerConfig.hpp"

// Reads and validates a configuration file. Throws ConfigError.
std::vector<ServerConfig> loadConfigFile(const std::string& path);

// Same as loadConfigFile, for text already in memory. "source" names the
// text in error messages.
std::vector<ServerConfig> loadConfigText(const std::string& text,
                                         const std::string& source);

#endif
