#ifndef ROUTE_ROUTER_HPP
#define ROUTE_ROUTER_HPP

#include <string>

#include "config/ServerConfig.hpp"
#include "http/Request.hpp"
#include "route/RouteResult.hpp"

// Request + configuration -> how to answer (design 7): path
// normalization, location, return, method check, file system mapping,
// directory handling.
class Router {
 public:
  explicit Router(const ServerConfig& server);

  // The location with the longest path that is a prefix of "path", or 0.
  // Locations are sorted longest first by the configuration loader.
  const LocationConfig* matchLocation(const std::string& path) const;

  RouteResult route(const Request& request) const;

  // File system path for a URI path through its location's root or alias
  // (for error pages). False if the path is invalid or has no location.
  bool mapToFile(const std::string& uriPath, std::string& fsPath) const;

 private:
  RouteResult routePath(const std::string& path, const Request& request,
                        int internalRedirects) const;
  RouteResult routeDirectory(const std::string& path, const Request& request,
                             const LocationConfig& location,
                             int internalRedirects) const;

  const ServerConfig& server_;
};

#endif
