#include "app/RequestProcessor.hpp"

#include "config/ConfigLimits.hpp"
#include "core/HttpDate.hpp"
#include "handler/AutoindexHandler.hpp"
#include "handler/StaticHandler.hpp"
#include "handler/StatusPage.hpp"
#include "http/HeaderNames.hpp"
#include "http/Methods.hpp"
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
  finish(response, request.method, now);
  return response;
}

Response RequestProcessor::errorResponse(int status, const std::string& method,
                                         std::time_t now) const {
  Response response = buildStatusPage(status, server_.errorPages, router_);
  finish(response, method, now);
  return response;
}

Response RequestProcessor::respond(const RouteResult& route) const {
  Response response;
  switch (route.kind) {
    case RouteResult::STATIC_FILE:
      if (serveStaticFile(route, response)) return response;
      return buildStatusPage(http_status::kInternalServerError,
                             server_.errorPages, router_);
    case RouteResult::DIRECTORY_LISTING:
      if (listDirectory(route, response)) return response;
      return buildStatusPage(http_status::kForbidden, server_.errorPages,
                             router_);
    case RouteResult::REDIRECT:
      response = buildStatusPage(route.status, server_.errorPages, router_);
      response.headers.add(header_name::kLocation, route.redirectTo);
      return response;
    case RouteResult::ERROR:
      break;
  }
  response = buildStatusPage(route.status, server_.errorPages, router_);
  // 405 must list what the resource supports (RFC 9110 15.5.6).
  if (route.status == http_status::kMethodNotAllowed)
    response.headers.add(header_name::kAllow, route.allow);
  return response;
}

// HEAD gets the GET head without content, errors included (RFC 9110 9.3.2,
// decision 5). Date: an origin server with a clock MUST send it (RFC 9110
// 6.6.1, design decision 10). Every connection closes after one response
// until keep-alive comes in step 5.
void RequestProcessor::finish(Response& response, const std::string& method,
                              std::time_t now) const {
  response.omitBody = (method == methods::kHead);
  response.headers.add(header_name::kDate, formatHttpDate(now));
  response.headers.add(header_name::kConnection, header_value::kClose);
}
