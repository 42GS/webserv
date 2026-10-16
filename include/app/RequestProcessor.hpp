#ifndef APP_REQUESTPROCESSOR_HPP
#define APP_REQUESTPROCESSOR_HPP

#include <cstddef>
#include <ctime>

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
  Response errorResponse(int status, std::time_t now) const;

 private:
  Response respond(const RouteResult& route) const;
  void addCommonFields(Response& response, std::time_t now) const;

  const ServerConfig& server_;
  Router router_;
};

#endif
