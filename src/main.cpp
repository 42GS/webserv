#include <iostream>
#include <string>
#include <vector>

#include "config/ConfigError.hpp"
#include "config/ConfigLoader.hpp"
#include "core/Clock.hpp"
#include "core/EventLoop.hpp"
#include "core/Server.hpp"
#include "core/Signals.hpp"

namespace {

const char* const kDefaultConfigPath = "conf/default.conf";
const char* const kClockFile = ".webserv_clock";
const int kExitSuccess = 0;
const int kExitFailure = 1;

int fail(const std::string& message) {
  std::cerr << "webserv: " << message << std::endl;
  return kExitFailure;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc > 2) {
    std::cerr << "usage: " << argv[0] << " [configuration file]" << std::endl;
    return kExitFailure;
  }
  const char* configPath = (argc == 2) ? argv[1] : kDefaultConfigPath;
  std::vector<ServerConfig> configs;
  try {
    configs = loadConfigFile(configPath);
  } catch (const ConfigError& error) {
    return fail(error.what());
  }

  Clock clock(kClockFile);
  if (!clock.refresh())
    return fail(std::string("cannot use clock file ") + kClockFile);
  signals::install();

  EventLoop loop(clock);
  Server server(configs, loop, clock.now());
  std::string error;
  if (!server.openListeners(error)) return fail(error);
  std::cout << "webserv: listening (" << configPath << ")" << std::endl;
  loop.run(server);
  std::cout << "webserv: shutting down" << std::endl;
  return kExitSuccess;
}
