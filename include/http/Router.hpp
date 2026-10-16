#ifndef HTTP_ROUTER_HPP
#define HTTP_ROUTER_HPP

#include <string>

#include "config/ServerConfig.hpp"

// Chooses how a request is handled. Step 3 needs only the location (for
// its body size limit); routing to handlers comes in step 4.
class Router {
 public:
  explicit Router(const ServerConfig& server);

  // The location with the longest path that is a prefix of "path", or 0.
  // Locations are sorted longest first by the configuration loader.
  const LocationConfig* matchLocation(const std::string& path) const;

 private:
  const ServerConfig& server_;
};

#endif
