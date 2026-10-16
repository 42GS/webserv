#include "http/Router.hpp"

Router::Router(const ServerConfig& server) : server_(server) {}

const LocationConfig* Router::matchLocation(const std::string& path) const {
  for (std::size_t i = 0; i < server_.locations.size(); ++i) {
    const std::string& prefix = server_.locations[i].path;
    if (path.compare(0, prefix.size(), prefix) == 0)
      return &server_.locations[i];
  }
  return 0;
}
