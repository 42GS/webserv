#include "app/RequestProcessor.hpp"

#include "config/ConfigLimits.hpp"
#include "core/HttpDate.hpp"
#include "handler/AutoindexHandler.hpp"
#include "handler/StaticHandler.hpp"
#include "handler/StatusPage.hpp"
#include "http/StatusCodes.hpp"

RequestProcessor::RequestProcessor(const ServerConfig& server)
    : server_(server), router_(server) {}

std::size_t RequestProcessor::bodyLimitFor(const Request& request) const {
  const LocationConfig* location = router_.locationFor(request.path);
  return location == 0 ? config_limits::kDefaultMaxBodySize
                       : location->maxBodySize;
}

Response RequestProcessor::process(const Request& request,
                                   std::time_t now) const {
  Response response = respond(router_.route(request));
  // HEAD gets the GET head without content (RFC 9110 9.3.2, decision 5).
  response.omitBody = (request.method == "HEAD");
  addCommonFields(response, now);
  return response;
}

Response RequestProcessor::errorResponse(int status, std::time_t now) const {
  Response response = statusPage(status);
  addCommonFields(response, now);
  return response;
}

Response RequestProcessor::statusPage(int status) const {
  return buildStatusPage(status, server_.errorPages, router_);
}

Response RequestProcessor::respond(const RouteResult& route) const {
  Response response;
  switch (route.kind) {
    case RouteResult::STATIC_FILE:
      if (serveStaticFile(route, response)) return response;
      return statusPage(http_status::kInternalServerError);
    case RouteResult::DIRECTORY_LISTING:
      if (listDirectory(route, response)) return response;
      return statusPage(http_status::kForbidden);
    case RouteResult::REDIRECT:
      response = statusPage(route.status);
      response.headers.add("Location", route.redirectTo);
      return response;
    case RouteResult::ERROR:
      break;
  }
  response = statusPage(route.status);
  // 405 must list what the resource supports (RFC 9110 15.5.6).
  if (route.status == http_status::kMethodNotAllowed)
    response.headers.add("Allow", route.allow);
  return response;
}

// Date: an origin server with a clock MUST send it (RFC 9110 6.6.1,
// design decision 10). Every connection closes after one response until
// keep-alive comes in step 5.
void RequestProcessor::addCommonFields(Response& response,
                                       std::time_t now) const {
  response.headers.add("Date", formatHttpDate(now));
  response.headers.add("Connection", "close");
}
