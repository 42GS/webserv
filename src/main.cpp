#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>
#include <vector>

#include "config/ConfigError.hpp"
#include "config/ConfigLoader.hpp"
#include "core/Clock.hpp"
#include "core/EventLoop.hpp"
#include "core/Limits.hpp"
#include "core/Server.hpp"
#include "core/Signals.hpp"

namespace {

int fail(const std::string& message) {
  std::cerr << "webserv: " << message << std::endl;
  return EXIT_FAILURE;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc > limits::kMaxArgc) {
    std::cerr << "usage: " << argv[0] << " [configuration file]" << std::endl;
    return EXIT_FAILURE;
  }
  const char* configPath =
      (argc == limits::kMaxArgc) ? argv[1] : limits::kDefaultConfigPath;
  std::vector<ServerConfig> configs;
  try {
    configs = loadConfigFile(configPath);
  } catch (const ConfigError& error) {
    return fail(error.what());
  }

  Clock clock(limits::kClockFile);
  if (!clock.refresh())
    return fail(std::string("cannot use clock file ") + limits::kClockFile);
  signals::install();

  EventLoop loop(clock);
  Server server(configs, loop, clock.now());
  std::string error;
  if (!server.openListeners(error)) return fail(error);
  std::cout << "webserv: listening (" << configPath << ")" << std::endl;
  // Exceptions from a single connection are handled inside the loop. One
  // that still gets here (std::bad_alloc while polling) ends the server
  // with a message instead of std::terminate().
  try {
    loop.run(server);
  } catch (const std::exception& error) {
    return fail(std::string("stopped: ") + error.what());
  }
  std::cout << "webserv: shutting down" << std::endl;
  return EXIT_SUCCESS;
}
