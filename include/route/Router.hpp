#ifndef ROUTE_ROUTER_HPP
#define ROUTE_ROUTER_HPP

#include <cstddef>
#include <string>

#include "config/ServerConfig.hpp"
#include "http/Methods.hpp"
#include "http/Request.hpp"
#include "route/RouteResult.hpp"

namespace router {

// Methods that static resources answer in step 4. POST and DELETE get their
// handlers in step 6.
const char* const kStaticMethods[] = {methods::kGet, methods::kHead};
const std::size_t kStaticMethodCount =
    sizeof(kStaticMethods) / sizeof(kStaticMethods[0]);

}  // namespace router

// Request + configuration -> how to answer (design 7): path
// normalization, location, return, method check, file system mapping,
// directory handling.
class Router {
 public:
  explicit Router(const ServerConfig& server);

  // The location with the longest path that is a prefix of "path", or 0.
  // Locations are sorted longest first by the configuration loader. An
  // alias location that does not cover the path at a segment boundary
  // also gives 0 (design decision 14).
  const LocationConfig* matchLocation(const std::string& path) const;

  // The location for a percent-encoded request path, or 0 if the path
  // cannot be normalized or no location matches.
  const LocationConfig* locationFor(const std::string& encodedPath) const;

  RouteResult route(const Request& request) const;

  // File system path for a URI path through its location's root or alias
  // (for error pages). False if the path is invalid or has no location.
  bool mapToFile(const std::string& uriPath, std::string& fsPath) const;

 private:
  // Normalizes and matches; false as for locationFor().
  bool resolve(const std::string& encodedPath, std::string& path,
               const LocationConfig*& location) const;
  RouteResult routePath(const std::string& path, const Request& request,
                        int internalRedirects) const;
  RouteResult routeDirectory(const std::string& path, const Request& request,
                             const LocationConfig& location,
                             int internalRedirects) const;

  const ServerConfig& server_;
};

#endif
