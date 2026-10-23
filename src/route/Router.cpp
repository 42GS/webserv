/**
 * @file  Router.cpp
 * @brief Routing in the order of design section 7.
 *        설계 7절 순서의 라우팅.
 */
#include "route/Router.hpp"

#include <cstddef>
#include <map>

#include "core/Limits.hpp"
#include "fs/FileInfo.hpp"
#include "fs/FilePath.hpp"
#include "http/Methods.hpp"
#include "http/StatusCodes.hpp"
#include "route/PathResolver.hpp"

namespace {

/**
 * @brief An ERROR decision.
 *        ERROR 결정.
 * @param[in] status Status code. / 상태 코드.
 * @return The decision. / 결정.
 */
RouteResult error(int status) {
  RouteResult result;
  result.kind = RouteResult::ERROR;
  result.status = status;
  return result;
}

/**
 * @brief A REDIRECT decision.
 *        REDIRECT 결정.
 * @param[in] status Redirect code. / 리다이렉트 코드.
 * @param[in] target Location value. / Location 값.
 * @return The decision. / 결정.
 */
RouteResult redirect(int status, const std::string& target) {
  RouteResult result;
  result.kind = RouteResult::REDIRECT;
  result.status = status;
  result.redirectTo = target;
  return result;
}

/**
 * @brief True if limit_except of the location allows the method.
 *        location의 limit_except가 이 메서드를 허용하면 true.
 * @param[in] location The location. / location.
 * @param[in] method   Implemented method. / 구현한 메서드.
 * @return Whether it is allowed. / 허용하는지.
 */
bool allows(const LocationConfig& location, const std::string& method) {
  return location.allowedMethods.count(method) != 0;
}

/**
 * @brief True if the location both allows the method and can answer it
 *        for a file.
 *        location이 이 메서드를 허용하고 파일에 대해 답할 수도 있으면 true.
 *
 * POST needs an upload store; GET, HEAD and DELETE work on any file.
 * POST는 업로드 저장 위치가 있어야 한다. GET, HEAD, DELETE는 어느 파일에나
 * 쓸 수 있다.
 *
 * @param[in] location The location. / location.
 * @param[in] method   Implemented method. / 구현한 메서드.
 * @return Whether the location answers it. / location이 답하는지.
 */
bool answers(const LocationConfig& location, const std::string& method) {
  return allows(location, method) &&
         (method != methods::kPost || !location.uploadStore.empty());
}

/** allows() or answers(). / allows()나 answers(). */
typedef bool (*MethodTest)(const LocationConfig&, const std::string&);

/**
 * @brief A 405 decision whose Allow lists the methods @p accepts lets
 *        through (RFC 9110 section 10.2.1).
 *        @p accepts가 통과시키는 메서드를 Allow에 담은 405 결정 (RFC 9110
 *        10.2.1절).
 * @param[in] location The location. / location.
 * @param[in] accepts  Which methods the resource supports.
 *                     리소스가 지원하는 메서드를 가리는 함수.
 * @return The decision. / 결정.
 */
RouteResult methodNotAllowed(const LocationConfig& location,
                             MethodTest accepts) {
  RouteResult result = error(http_status::kMethodNotAllowed);
  for (std::size_t i = 0; i < methods::kImplementedCount; ++i) {
    if (!accepts(location, methods::kImplemented[i])) continue;
    if (!result.allow.empty()) result.allow += ", ";
    result.allow += methods::kImplemented[i];
  }
  return result;
}

/**
 * @brief Maps a normalized path to the file system.
 *        정규화한 경로를 파일 시스템 경로로 바꾼다.
 *
 * root: the whole URI is appended; alias: the location prefix is replaced
 * (concepts/configuration-file).
 * root는 URI 전체를 붙이고, alias는 location prefix를 바꿔 끼운다
 * (concepts/configuration-file).
 *
 * @param[in] location Matched location. / 고른 location.
 * @param[in] path     Normalized path. / 정규화한 경로.
 * @return File system path. / 파일 시스템 경로.
 */
std::string mapPath(const LocationConfig& location, const std::string& path) {
  if (!location.alias.empty())
    return location.alias + path.substr(location.path.size());
  return location.root + path;
}

/**
 * @brief True if the location may serve the path.
 *        location이 이 경로를 맡아도 되면 true.
 *
 * An alias location covers a path only if it ends at a segment boundary:
 * its own path ends in "/", or what is left after it is empty or starts
 * with "/". Otherwise "location /k { alias dir/; }" would map "/k../x" to
 * "dir/../x", outside dir (design decision 14). root appends the whole
 * normalized path and cannot leave its directory.
 * alias location은 조각 경계에서 끝날 때만 경로를 맡는다. location 경로가
 * "/"로 끝나거나, 그 뒤에 남은 부분이 비었거나 "/"로 시작해야 한다.
 * 그렇지 않으면 "location /k { alias dir/; }"가 "/k../x"를 dir 밖의
 * "dir/../x"로 매핑한다 (설계 결정 14). root는 정규화한 경로 전체를
 * 붙이므로 자기 디렉토리를 벗어날 수 없다.
 *
 * @param[in] location Location whose path is a prefix of @p path.
 *                     경로가 @p path의 prefix인 location.
 * @param[in] path     Normalized path. / 정규화한 경로.
 * @return Whether the location covers the path. / location이 경로를 맡는지.
 */
bool coversPath(const LocationConfig& location, const std::string& path) {
  if (location.alias.empty()) return true;
  const std::string::size_type end = location.path.size();
  return path.size() == end || path[end] == '/' ||
         location.path[end - 1] == '/';
}

/**
 * @brief Decides a DELETE of a mapped path.
 *        매핑한 경로의 DELETE를 정한다.
 *
 * Only regular files are deleted: directories and other kinds get 403,
 * as does a file whose directory the server cannot change.
 * 일반 파일만 지운다. 디렉토리와 다른 종류는 403이고, 서버가 바꿀 수
 * 없는 디렉토리 안의 파일도 403이다.
 *
 * @param[in] fsPath File system path. / 파일 시스템 경로.
 * @return DELETE, or ERROR 404 or 403. / DELETE, 또는 ERROR 404나 403.
 */
RouteResult deletion(const std::string& fsPath) {
  const FileKind kind = fileKind(fsPath);
  if (kind == FILE_MISSING) return error(http_status::kNotFound);
  if (kind != FILE_REGULAR || !canChangeEntries(parentDirectory(fsPath)))
    return error(http_status::kForbidden);

  RouteResult result;
  result.kind = RouteResult::DELETE;
  result.fsPath = fsPath;
  return result;
}

/**
 * @brief An UPLOAD decision into the location's store.
 *        location의 저장 위치로 가는 UPLOAD 결정.
 * @param[in] location Location with an upload store.
 *                    업로드 저장 위치가 있는 location.
 * @param[in] path     Normalized request path. / 정규화한 요청 경로.
 * @return The decision. / 결정.
 */
RouteResult upload(const LocationConfig& location, const std::string& path) {
  RouteResult result;
  result.kind = RouteResult::UPLOAD;
  result.uploadStore = location.uploadStore;
  result.urlPath = path;
  return result;
}

/**
 * @brief True if @p name is longer than @p extension and ends with it.
 *        @p name이 @p extension보다 길고 그것으로 끝나면 true.
 * @param[in] name      Last path segment. / 경로의 마지막 조각.
 * @param[in] extension Such as ".py". / ".py" 같은 것.
 * @return Whether the name has the extension. / 이름에 그 확장자가 있는지.
 */
bool hasExtension(const std::string& name, const std::string& extension) {
  return name.size() > extension.size() &&
         name.compare(name.size() - extension.size(), extension.size(),
                      extension) == 0;
}

/**
 * @brief The interpreter for a file name by its extension.
 *        파일 이름의 확장자에 맞는 인터프리터.
 * @param[in] location The location. / location.
 * @param[in] name     Last segment of a candidate script path.
 *                     후보 스크립트 경로의 마지막 조각.
 * @return The interpreter, or 0 if no "cgi" extension matches.
 *         인터프리터. 맞는 "cgi" 확장자가 없으면 0.
 */
const std::string* interpreterFor(const LocationConfig& location,
                                  const std::string& name) {
  for (std::map<std::string, std::string>::const_iterator it =
           location.cgiByExtension.begin();
       it != location.cgiByExtension.end(); ++it) {
    if (hasExtension(name, it->first)) return &it->second;
  }
  return 0;
}

/**
 * @brief Finds the script in a path: the first leading part whose last
 *        segment has a "cgi" extension and is a regular file.
 *        경로 안의 스크립트를 찾는다: 마지막 조각이 "cgi" 확장자이고 일반
 *        파일인 첫 앞부분.
 *
 * The leading parts end at each "/" after the location, then at the end
 * of the path; the rest is for the script (RFC 3875 section 3.2).
 * 앞부분은 location 뒤의 "/"마다, 마지막에는 경로 끝에서 끊는다. 나머지는
 * 스크립트가 해석한다 (RFC 3875 3.2절).
 *
 * @param[in] location Matched location. / 고른 location.
 * @param[in] path     Normalized path. / 정규화한 경로.
 * @return A CGI decision, or an ERROR with status 0 if there is no
 *         script.
 *         CGI 결정. 스크립트가 없으면 status가 0인 ERROR.
 */
RouteResult findScript(const LocationConfig& location,
                       const std::string& path) {
  RouteResult result;
  if (location.cgiByExtension.empty()) return result;

  std::string::size_type end = path.find('/', location.path.size());
  for (;;) {
    const std::string scriptName = path.substr(0, end);
    const std::string* interpreter =
        interpreterFor(location, baseName(scriptName));
    if (interpreter != 0 &&
        fileKind(mapPath(location, scriptName)) == FILE_REGULAR) {
      result.kind = RouteResult::CGI;
      result.fsPath = mapPath(location, scriptName);
      result.interpreter = *interpreter;
      result.scriptName = scriptName;
      if (end != std::string::npos) result.pathInfo = path.substr(end);
      return result;
    }

    if (end == std::string::npos) return result;
    end = path.find('/', end + 1);
  }
}

/**
 * @brief A STATIC_FILE decision.
 *        STATIC_FILE 결정.
 * @param[in] fsPath File to send. / 보낼 파일.
 * @return The decision. / 결정.
 */
RouteResult staticFile(const std::string& fsPath) {
  RouteResult result;
  result.kind = RouteResult::STATIC_FILE;
  result.fsPath = fsPath;
  return result;
}

/**
 * @brief Appends "?query" when there is one.
 *        query가 있으면 "?query"를 붙인다.
 * @param[in] path  Path. / 경로.
 * @param[in] query Query, possibly empty. / query, 비어 있을 수 있다.
 * @return The URI. / URI.
 */
std::string withQuery(const std::string& path, const std::string& query) {
  return query.empty() ? path : path + "?" + query;
}

}  // namespace

Router::Router(const ServerConfig& server) : server_(server) {}

const LocationConfig* Router::matchLocation(const std::string& path) const {
  for (std::size_t i = 0; i < server_.locations.size(); ++i) {
    const std::string& prefix = server_.locations[i].path;
    if (path.compare(0, prefix.size(), prefix) != 0) continue;
    if (!coversPath(server_.locations[i], path)) return 0;
    return &server_.locations[i];
  }
  return 0;
}

RouteResult Router::route(const Request& request) const {
  std::string path;
  if (!normalizePath(request.path, path))
    return error(http_status::kBadRequest);
  return routePath(path, request, 0);
}

const LocationConfig* Router::locationFor(
    const std::string& encodedPath) const {
  std::string path;
  const LocationConfig* location = 0;
  return resolve(encodedPath, path, location) ? location : 0;
}

bool Router::mapToFile(const std::string& uriPath, std::string& fsPath) const {
  std::string path;
  const LocationConfig* location = 0;
  if (!resolve(uriPath, path, location)) return false;
  fsPath = mapPath(*location, path);
  return true;
}

bool Router::resolve(const std::string& encodedPath, std::string& path,
                     const LocationConfig*& location) const {
  if (!normalizePath(encodedPath, path)) return false;
  location = matchLocation(path);
  return location != 0;
}

// Order: location, return, CGI, limit_except, upload, delete, then the
// file system (design 7). The script is looked up before the method check
// because the two use different tests: a script answers every method
// limit_except allows and reads REQUEST_METHOD itself, while other paths
// also need answers().
// 순서: location, return, CGI, limit_except, 업로드, 삭제, 그다음 파일
// 시스템 (설계 7절). 두 경우의 메서드 검사가 달라서 스크립트를 먼저 찾는다.
// 스크립트는 limit_except가 허용한 메서드를 모두 받고 REQUEST_METHOD를
// 보고 스스로 처리하지만, 다른 경로는 answers()도 통과해야 한다.
RouteResult Router::routePath(const std::string& path, const Request& request,
                              int internalRedirects) const {
  const LocationConfig* location = matchLocation(path);
  if (location == 0) return error(http_status::kNotFound);
  if (location->returnCode != 0)
    return redirect(location->returnCode, location->returnUrl);

  RouteResult script = findScript(*location, path);
  if (script.kind == RouteResult::CGI) {
    if (!allows(*location, request.method))
      return methodNotAllowed(*location, allows);
    translatePathInfo(script);
    return script;
  }

  if (!answers(*location, request.method))
    return methodNotAllowed(*location, answers);
  if (request.method == methods::kPost) return upload(*location, path);

  const std::string fsPath = mapPath(*location, path);
  if (request.method == methods::kDelete) return deletion(fsPath);

  const FileKind kind = fileKind(fsPath);
  if (kind == FILE_MISSING) return error(http_status::kNotFound);
  if (kind == FILE_DIRECTORY)
    return routeDirectory(path, request, *location, internalRedirects);

  // Devices, FIFOs and the like are never served (RFC 9110 section 17.3).
  // 장치 파일이나 FIFO 같은 것은 절대 보내지 않는다 (RFC 9110 17.3절).
  if (kind != FILE_REGULAR || !isReadable(fsPath))
    return error(http_status::kForbidden);
  return staticFile(fsPath);
}

void Router::translatePathInfo(RouteResult& script) const {
  // Left empty if PATH_INFO maps to no location: RFC 3875 section 4.1.6
  // makes the variable a SHOULD, and with no location there is no
  // directory to map it into.
  // PATH_INFO가 어느 location에도 매핑되지 않으면 비워 둔다. RFC 3875
  // 4.1.6절은 이 변수를 SHOULD로 두고, location이 없으면 매핑할
  // 디렉토리가 없다.
  if (!script.pathInfo.empty())
    static_cast<void>(
        mapToFile(encodePath(script.pathInfo), script.pathTranslated));
}

// Follows concepts/configuration-file "디렉토리 요청 처리 순서".
// concepts/configuration-file "디렉토리 요청 처리 순서"를 따른다.
RouteResult Router::routeDirectory(const std::string& path,
                                   const Request& request,
                                   const LocationConfig& location,
                                   int internalRedirects) const {
  if (path[path.size() - 1] != '/')
    return redirect(http_status::kMovedPermanently,
                    withQuery(encodePath(path + "/"), request.query));

  const std::string fsPath = mapPath(location, path);
  for (std::size_t i = 0; i < location.indexFiles.size(); ++i) {
    if (fileKind(fsPath + location.indexFiles[i]) != FILE_REGULAR) continue;
    if (internalRedirects >= limits::kMaxInternalRedirects)
      return error(http_status::kInternalServerError);
    return routePath(path + location.indexFiles[i], request,
                     internalRedirects + 1);
  }

  if (!location.autoindex) return error(http_status::kForbidden);
  RouteResult result;
  result.kind = RouteResult::DIRECTORY_LISTING;
  result.fsPath = fsPath;
  result.urlPath = path;
  return result;
}
