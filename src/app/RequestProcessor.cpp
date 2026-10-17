/**
 * @file  RequestProcessor.cpp
 * @brief Request processing: routing, handlers and common fields.
 *        요청 처리: 라우팅, handler, 공통 필드.
 */
#include "app/RequestProcessor.hpp"

#include <cstdio>

#include "config/ConfigLimits.hpp"
#include "core/HttpDate.hpp"
#include "handler/AutoindexHandler.hpp"
#include "handler/StaticHandler.hpp"
#include "handler/StatusPage.hpp"
#include "http/HeaderNames.hpp"
#include "http/Methods.hpp"
#include "http/MimeTypes.hpp"
#include "http/StatusCodes.hpp"
#include "route/PathResolver.hpp"

RequestProcessor::RequestProcessor(const ServerConfig& server)
    : server_(server), router_(server) {}

std::size_t RequestProcessor::bodyLimitFor(const Request& request) const {
  const LocationConfig* location = router_.locationFor(request.path);
  return location == 0 ? config_limits::kDefaultMaxBodySize
                       : location->maxBodySize;
}

Response RequestProcessor::process(const Request& request,
                                   std::time_t now) const {
  Response response = respond(router_.route(request), request);
  finish(response, request.method,
         request.persistent && response.status != http_status::kBadRequest,
         now);
  return response;
}

Response RequestProcessor::errorResponse(int status, const std::string& method,
                                         std::time_t now) const {
  Response response = statusPage(status);
  finish(response, method, false, now);
  return response;
}

Response RequestProcessor::statusPage(int status) const {
  return buildStatusPage(status, server_.errorPages, router_);
}

Response RequestProcessor::respond(const RouteResult& route,
                                   const Request& request) const {
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
    case RouteResult::UPLOAD:
      return uploadResponse(route, request);
    case RouteResult::DELETE:
      // std::remove: unlink() is not on the list of allowed functions
      // (design decision 15). 204: done, with nothing more to say (RFC 9110
      // 9.3.5).
      // std::remove: unlink()는 허용 함수 목록에 없다 (설계 결정 15).
      // 204: 실행했고 더 알릴 것이 없다 (RFC 9110 9.3.5절).
      if (std::remove(route.fsPath.c_str()) != 0)
        return statusPage(http_status::kInternalServerError);
      response.status = http_status::kNoContent;
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

Response RequestProcessor::uploadResponse(const RouteResult& route,
                                          const Request& request) const {
  const UploadResult result = storeUpload(route, request);
  if (result.status != http_status::kCreated) return statusPage(result.status);
  Response response;
  response.status = http_status::kCreated;
  response.headers.add(header_name::kContentType, mime_types::kTextType);
  for (std::size_t i = 0; i < result.files.size(); ++i) {
    const std::string uri = uriOfUpload(route.urlPath, result.files[i]);
    if (!uri.empty() && !response.headers.has(header_name::kLocation))
      response.headers.add(header_name::kLocation, uri);
    response.body += (uri.empty() ? result.files[i].name : uri) + "\n";
  }
  return response;
}

std::string RequestProcessor::uriOfUpload(const std::string& targetPath,
                                          const UploadedFile& file) const {
  const std::string uri =
      encodePath(targetPath.substr(0, targetPath.rfind('/') + 1) + file.name);
  std::string mapped;
  if (router_.mapToFile(uri, mapped) && mapped == file.path) return uri;
  return "";
}

// HEAD gets the GET head without content, errors included (RFC 9110 9.3.2,
// decision 5). Date: an origin server with a clock MUST send it (RFC 9110
// 6.6.1, design decision 10). In a response, "Connection: close" says that
// the server closes the connection after it (RFC 9112 9.6).
// HEAD는 오류 응답까지 포함해 GET의 머리만 받고 내용은 받지 않는다
// (RFC 9110 9.3.2절, 결정 5). Date: 시계가 있는 origin server는 반드시
// 보내야 한다 (RFC 9110 6.6.1절, 설계 결정 10). 응답의
// "Connection: close"는 서버가 그 응답 뒤에 연결을 닫는다는 뜻이다
// (RFC 9112 9.6절).
void RequestProcessor::finish(Response& response, const std::string& method,
                              bool keepAlive, std::time_t now) const {
  response.omitBody = (method == methods::kHead);
  response.keepAlive = keepAlive;
  response.headers.add(header_name::kDate, formatHttpDate(now));
  if (!keepAlive)
    response.headers.add(header_name::kConnection, header_value::kClose);
}
