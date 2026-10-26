/**
 * @file  RequestProcessor.cpp
 * @brief Request processing: routing, handlers and common fields.
 *        요청 처리: 라우팅, handler, 공통 필드.
 */
#include "app/RequestProcessor.hpp"

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

/**
 * @brief True if the connection stays open after this response.
 *        이 응답 뒤에 연결을 유지하면 true.
 *
 * When the request allows it and the response is not a 400: a bad path
 * closes it, as NGINX does (design decision 2), and so does a malformed
 * upload body.
 * 요청이 허용하고 응답이 400이 아닐 때다. 잘못된 경로는 NGINX처럼 연결을
 * 닫고 (설계 결정 2), 잘못된 업로드 본문도 닫는다.
 *
 * @param[in] request  The request. / 요청.
 * @param[in] response The response. / 응답.
 * @return Whether to keep it open. / 유지할지.
 */
bool keepsConnection(const Request& request, const Response& response) {
  return request.persistent && response.status != http_status::kBadRequest;
}

/**
 * @brief Moves a response without copying its body.
 *        본문을 복사하지 않고 응답을 옮긴다.
 *
 * A CGI body may be limits::kMaxCgiOutputBytes long.
 * CGI 본문은 limits::kMaxCgiOutputBytes까지 길 수 있다.
 *
 * @param[in,out] from Source; its body is left empty. / 원본. 본문은 빈다.
 * @param[out]    to   Destination. / 대상.
 */
void moveResponse(Response& from, Response& to) {
  to.status = from.status;
  to.headers = from.headers;
  to.body.clear();
  to.body.swap(from.body);
}

/**
 * @brief The status that refuses a script before fork() (design decision
 *        12), or 0 if it may run.
 *        fork() 전에 스크립트를 거절하는 상태 코드 (설계 결정 12). 실행해도
 *        되면 0.
 *
 * Checking first keeps execve() from failing in the child for the usual
 * causes.
 * 먼저 검사해서 흔한 원인으로 자식에서 execve()가 실패하는 일을 막는다.
 *
 * @param[in] script CGI decision. / CGI 결정.
 * @return 403 for an unreadable script or a program that cannot run, 404
 *         for a missing program, or 0.
 *         읽을 수 없는 스크립트나 실행할 수 없는 프로그램이면 403, 프로그램이
 *         없으면 404, 아니면 0.
 */
int cgiRefusal(const RouteResult& script) {
  if (!isReadable(script.fsPath)) return http_status::kForbidden;
  if (fileKind(script.interpreter) == FILE_MISSING)
    return http_status::kNotFound;
  if (!isExecutable(script.interpreter)) return http_status::kForbidden;
  return 0;
}

/**
 * @brief What to run for a script: the program, the script's directory
 *        and file name, and the meta-variables.
 *        스크립트에 대해 실행할 것: 프로그램, 스크립트의 디렉토리와 파일
 *        이름, 메타변수.
 * @param[in] script  CGI decision. / CGI 결정.
 * @param[in] request The request. / 요청.
 * @param[in] where   Both ends of the connection. / 연결의 두 끝.
 * @return The launch. / 실행 정보.
 */
CgiLaunch launchFor(const RouteResult& script, const Request& request,
                    const ListenContext& where) {
  CgiLaunch launch;
  launch.interpreter = script.interpreter;
  launch.directory = parentDirectory(script.fsPath);
  launch.scriptFile = baseName(script.fsPath);
  launch.environment = buildCgiEnvironment(request, script, where);
  return launch;
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

// The body is moved, not copied, from the script's output to the outcome
// (it may be limits::kMaxCgiOutputBytes long).
// 본문은 스크립트 출력에서 결과로 복사하지 않고 옮긴다
// (limits::kMaxCgiOutputBytes까지 길 수 있다).
ProcessOutcome RequestProcessor::afterCgi(const Request& request,
                                          CgiProcess& cgi,
                                          std::time_t now) const {
  std::string text;
  cgi.takeOutput(text);
  // Initialized, not assigned, so the result is not copied once more.
  // 대입하지 않고 초기화해서 결과를 한 번 더 복사하지 않는다.
  CgiOutput output = cgi.failed() ? CgiOutput() : parseCgiOutput(text);
  if (output.kind == CgiOutput::INVALID)
    return statusOutcome(http_status::kBadGateway, request, now);

  ProcessOutcome outcome;
  if (output.kind == CgiOutput::LOCAL_REDIRECT) {
    outcome.kind = ProcessOutcome::LOCAL_REDIRECT;
    outcome.redirect = localRedirectRequest(request, output.localTarget);
    return outcome;
  }

  moveResponse(output.response, outcome.response);
  finish(outcome.response, request.method,
         keepsConnection(request, outcome.response), now);
  return outcome;
}

Response RequestProcessor::statusResponse(int status, const Request& request,
                                          std::time_t now) const {
  return statusOutcome(status, request, now).response;
}

ProcessOutcome RequestProcessor::startCgi(const RouteResult& script,
                                          const Request& request,
                                          std::time_t now) const {
  const int refusal = cgiRefusal(script);
  if (refusal != 0) return statusOutcome(refusal, request, now);

  // Everything that may throw before the process exists is done first;
  // after that the process is deleted on every path until the outcome
  // owns it (no std::auto_ptr, see Server::openListeners()).
  // 프로세스를 만들기 전에 던질 수 있는 일을 먼저 한다. 그 뒤로는 결과가
  // 프로세스를 가질 때까지 모든 경로에서 지운다 (std::auto_ptr는 쓰지
  // 않는다. Server::openListeners() 참고).
  const CgiLaunch launch = launchFor(script, request, where_);
  ProcessOutcome outcome;
  CgiProcess* cgi = new CgiProcess(launch, request.body, services_);
  bool started = false;
  try {
    started = cgi->start(now);
  } catch (...) {
    delete cgi;
    throw;
  }
  if (!started) {
    delete cgi;
    return statusOutcome(http_status::kInternalServerError, request, now);
  }

  outcome.kind = ProcessOutcome::CGI;
  outcome.cgi = cgi;
  return outcome;
}

ProcessOutcome RequestProcessor::statusOutcome(int status,
                                               const Request& request,
                                               std::time_t now) const {
  return finished(statusPage(status), request, now);
}

ProcessOutcome RequestProcessor::finished(const Response& response,
                                          const Request& request,
                                          std::time_t now) const {
  ProcessOutcome outcome;
  outcome.response = response;
  finish(outcome.response, request.method,
         keepsConnection(request, outcome.response), now);
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
  // 405 must list what the resource supports (RFC 9110 section 15.5.6).
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

// HEAD gets the GET head without content, errors included (RFC 9110
// section 9.3.2, design decision 5). Date: an origin server with a clock
// MUST send it in 2xx to 4xx responses and MAY in the others; it is sent
// in all of them (RFC 9110 section 6.6.1, design decision 10). In a
// response, "Connection: close" says that the server closes the
// connection after it (RFC 9112 section 9.6).
// HEAD는 오류 응답까지 포함해 GET의 머리만 받고 내용은 받지 않는다
// (RFC 9110 9.3.2절, 설계 결정 5). Date: 시계가 있는 origin server는
// 2xx~4xx 응답에 반드시 보내야 하고 나머지에는 보내도 된다. 모든 응답에
// 보낸다 (RFC 9110 6.6.1절, 설계 결정 10). 응답의 "Connection: close"는
// 서버가 그 응답 뒤에 연결을 닫는다는 뜻이다 (RFC 9112 9.6절).
void RequestProcessor::finish(Response& response, const std::string& method,
                              bool keepAlive, std::time_t now) const {
  response.omitBody = (method == methods::kHead);
  response.keepAlive = keepAlive;
  response.headers.add(header_name::kDate, formatHttpDate(now));
  if (!keepAlive)
    response.headers.add(header_name::kConnection, header_value::kClose);
}
