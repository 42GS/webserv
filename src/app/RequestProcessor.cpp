/**
 * @file  RequestProcessor.cpp
 * @brief Request processing: routing, handlers and common fields.
 *        요청 처리: 라우팅, handler, 공통 필드.
 */
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
  Response response = statusPage(status);
  finish(response, method, now);
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
      response.headers.add(header_name::kLocation, route.redirectTo);
      return response;
    case RouteResult::ERROR:
      break;
  }
  response = statusPage(route.status);
  // 405 must list what the resource supports (RFC 9110 15.5.6).
  // 405는 리소스가 지원하는 메서드를 알려야 한다 (RFC 9110 15.5.6절).
  if (route.status == http_status::kMethodNotAllowed)
    response.headers.add(header_name::kAllow, route.allow);
  return response;
}

// HEAD gets the GET head without content, errors included (RFC 9110 9.3.2,
// decision 5). Date: an origin server with a clock MUST send it (RFC 9110
// 6.6.1, design decision 10). Every connection closes after one response
// until keep-alive comes in step 5.
// HEAD는 오류 응답까지 포함해 GET의 머리만 받고 내용은 받지 않는다
// (RFC 9110 9.3.2절, 결정 5). Date: 시계가 있는 origin server는 반드시
// 보내야 한다 (RFC 9110 6.6.1절, 설계 결정 10). keep-alive가 들어오는
// 5단계 전까지는 모든 연결이 응답 하나 뒤에 닫힌다.
void RequestProcessor::finish(Response& response, const std::string& method,
                              std::time_t now) const {
  response.omitBody = (method == methods::kHead);
  response.headers.add(header_name::kDate, formatHttpDate(now));
  response.headers.add(header_name::kConnection, header_value::kClose);
}
