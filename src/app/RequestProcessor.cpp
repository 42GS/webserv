/**
 * @file  RequestProcessor.cpp
 * @brief Request processing: routing, handlers and common fields.
 *        요청 처리: 라우팅, handler, 공통 필드.
 */
#include "app/RequestProcessor.hpp"

#include <memory>

#include "cgi/CgiEnv.hpp"
#include "cgi/CgiOutput.hpp"
#include "config/ConfigLimits.hpp"
#include "core/HttpDate.hpp"
#include "fs/FileInfo.hpp"
#include "fs/FilePath.hpp"
#include "handler/AutoindexHandler.hpp"
#include "handler/DeleteHandler.hpp"
#include "handler/StaticHandler.hpp"
#include "handler/StatusPage.hpp"
#include "handler/UploadHandler.hpp"
#include "http/HeaderNames.hpp"
#include "http/Methods.hpp"
#include "http/StatusCodes.hpp"

namespace {

/**
 * @brief The request a CGI local redirect asks for.
 *        CGI 로컬 리다이렉트가 요청하는 것.
 *
 * Kept from the original: version and persistence. Body fields are not
 * kept, as the body is not sent again.
 * 원래 요청에서 버전과 연결 유지 여부를 가져온다. 본문을 다시 보내지
 * 않으므로 본문 필드는 가져오지 않는다.
 *
 * @param[in] original The request the script answered. / 스크립트가 답한 요청.
 * @param[in] target   Location value: path and optional query.
 *                     Location 값: 경로와 선택적 query.
 * @return The new request. / 새 요청.
 */
Request localRedirectRequest(const Request& original,
                             const std::string& target) {
  Request request;
  request.method =
      original.method == methods::kHead ? methods::kHead : methods::kGet;
  request.target = target;
  const std::string::size_type question = target.find('?');
  request.path = target.substr(0, question);
  if (question != std::string::npos)
    request.query = target.substr(question + 1);
  request.version = original.version;
  request.persistent = original.persistent;
  const std::string* host = original.headers.first(header_name::kHost);
  if (host != 0) request.headers.add(header_name::kHost, *host);
  return request;
}

}  // namespace

RequestProcessor::RequestProcessor(const ListenContext& where,
                                   const CgiServices& services)
    : where_(where), services_(services), router_(*where.server) {}

std::size_t RequestProcessor::bodyLimitFor(const Request& request) const {
  const LocationConfig* location = router_.locationFor(request.path);
  return location == 0 ? config_limits::kDefaultMaxBodySize
                       : location->maxBodySize;
}

ProcessOutcome RequestProcessor::process(const Request& request,
                                         std::time_t now) const {
  const RouteResult route = router_.route(request);
  if (route.kind == RouteResult::CGI) return startCgi(route, request, now);
  return finished(respond(route, request), request, now);
}

ProcessOutcome RequestProcessor::afterCgi(const Request& request,
                                          const CgiProcess& cgi,
                                          std::time_t now) const {
  const CgiOutput output =
      cgi.failed() ? CgiOutput() : parseCgiOutput(cgi.output());
  if (output.kind == CgiOutput::INVALID)
    return finished(statusPage(http_status::kBadGateway), request, now);
  if (output.kind == CgiOutput::RESPONSE)
    return finished(output.response, request, now);
  ProcessOutcome outcome;
  outcome.kind = ProcessOutcome::LOCAL_REDIRECT;
  outcome.redirect = localRedirectRequest(request, output.localTarget);
  return outcome;
}

Response RequestProcessor::statusResponse(int status, const Request& request,
                                          std::time_t now) const {
  return finished(statusPage(status), request, now).response;
}

ProcessOutcome RequestProcessor::startCgi(const RouteResult& script,
                                          const Request& request,
                                          std::time_t now) const {
  if (!isReadable(script.fsPath))
    return finished(statusPage(http_status::kForbidden), request, now);
  if (fileKind(script.interpreter) == FILE_MISSING)
    return finished(statusPage(http_status::kNotFound), request, now);
  if (!isExecutable(script.interpreter))
    return finished(statusPage(http_status::kForbidden), request, now);
  CgiLaunch launch;
  launch.interpreter = script.interpreter;
  launch.directory = parentDirectory(script.fsPath);
  launch.scriptFile = baseName(script.fsPath);
  launch.environment = buildCgiEnvironment(request, script, where_);
  std::auto_ptr<CgiProcess> cgi(
      new CgiProcess(launch, request.body, services_));
  if (!cgi->start(now))
    return finished(statusPage(http_status::kInternalServerError), request,
                    now);
  ProcessOutcome outcome;
  outcome.kind = ProcessOutcome::CGI;
  outcome.cgi = cgi.release();
  return outcome;
}

ProcessOutcome RequestProcessor::finished(const Response& response,
                                          const Request& request,
                                          std::time_t now) const {
  ProcessOutcome outcome;
  outcome.response = response;
  finish(
      outcome.response, request.method,
      request.persistent && outcome.response.status != http_status::kBadRequest,
      now);
  return outcome;
}

Response RequestProcessor::errorResponse(int status, const std::string& method,
                                         std::time_t now) const {
  Response response = statusPage(status);
  finish(response, method, false, now);
  return response;
}

Response RequestProcessor::statusPage(int status) const {
  return buildStatusPage(status, where_.server->errorPages, router_);
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
      if (deleteFile(route, response)) return response;
      return statusPage(http_status::kInternalServerError);
    case RouteResult::CGI:
      // Not reached: process() starts scripts before respond().
      // 오지 않는다: process()가 respond() 전에 스크립트를 시작한다.
      return statusPage(http_status::kInternalServerError);
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
  if (result.status == http_status::kCreated)
    return createdResponse(result, route.urlPath, router_);
  if (result.status == http_status::kConflict) return conflictResponse(result);
  return statusPage(result.status);
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
