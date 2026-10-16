#include "config/ConfigError.hpp"
#include "config/ConfigLoader.hpp"

#include <iostream>

namespace {

const char* const kDefaultConfigPath = "conf/default.conf";
const int kExitSuccess = 0;
const int kExitFailure = 1;

}  // namespace

int main(int argc, char** argv)
{
    if (argc > 2) {
        std::cerr << "usage: " << argv[0] << " [configuration file]" << std::endl;
        return kExitFailure;
    }
    const char* configPath = (argc == 2) ? argv[1] : kDefaultConfigPath;
    std::vector<ServerConfig> servers;
    try {
        servers = loadConfigFile(configPath);
    } catch (const ConfigError& error) {
        std::cerr << "webserv: " << error.what() << std::endl;
        return kExitFailure;
    }
    std::cout << "webserv: " << configPath << ": " << servers.size() << " server(s) loaded" << std::endl;
    return kExitSuccess;
}
