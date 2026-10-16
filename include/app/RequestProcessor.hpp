#ifndef APP_REQUESTPROCESSOR_HPP
#define APP_REQUESTPROCESSOR_HPP

#include <cstddef>
#include <ctime>
#include <string>

#include "config/ServerConfig.hpp"
#include "http/Request.hpp"
#include "http/Response.hpp"
#include "route/Router.hpp"

// One complete request to one response (design 2 "app"): Router decides,
// a handler builds the response, and the common fields are added.
class RequestProcessor {
 public:
  explicit RequestProcessor(const ServerConfig& server);

  // client_max_body_size of the location for this request. A path that
  // cannot be normalized or matched gets the default; routing answers it
  // with 400 or 404 after the body.
  std::size_t bodyLimitFor(const Request& request) const;
  Response process(const Request& request, std::time_t now) const;
  // For a request that could not be processed (parse error, timeout).
  // "method" is empty if the request line was not understood.
  Response errorResponse(int status, const std::string& method,
                         std::time_t now) const;

 private:
  Response respond(const RouteResult& route) const;
  // buildStatusPage() with this server's error pages.
  Response statusPage(int status) const;
  void finish(Response& response, const std::string& method,
              std::time_t now) const;

  const ServerConfig& server_;
  Router router_;
};

#endif
