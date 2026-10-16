/**
 * @file  Router.cpp
 * @brief Routing in the order of design section 7.
 *        설계 7절 순서의 라우팅.
 */
#include "route/Router.hpp"

#include <cstddef>

#include "core/Limits.hpp"
#include "fs/FileInfo.hpp"
#include "http/Methods.hpp"
#include "http/StatusCodes.hpp"
#include "route/PathResolver.hpp"

namespace {

/**
 * @brief Methods that static resources answer in step 4.
 *        4단계에서 정적 리소스가 답하는 메서드.
 *
 * POST and DELETE get their handlers in step 6.
 * POST와 DELETE의 handler는 6단계에서 생긴다.
 */
const char* const kStaticMethods[] = {methods::kGet, methods::kHead};
/** Number of entries in kStaticMethods. / kStaticMethods의 항목 수. */
const std::size_t kStaticMethodCount =
    sizeof(kStaticMethods) / sizeof(kStaticMethods[0]);

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
 * @param[in] method   Request method. / 요청 메서드.
 * @return Whether it is allowed; true when there is no limit_except.
 *         허용되는지. limit_except가 없으면 true.
 */
bool locationAllows(const LocationConfig& location, const std::string& method) {
  return location.allowedMethods.empty() ||
         location.allowedMethods.count(method) != 0;
}

/**
 * @brief True for the methods static resources answer.
 *        정적 리소스가 답하는 메서드이면 true.
 * @param[in] method Request method. / 요청 메서드.
 * @return Whether it is GET or HEAD. / GET이나 HEAD인지.
 */
bool isStaticMethod(const std::string& method) {
  for (std::size_t i = 0; i < kStaticMethodCount; ++i) {
    if (method == kStaticMethods[i]) return true;
  }
  return false;
}

/**
 * @brief The Allow value of a 405.
 *        405의 Allow 값.
 *
 * Methods this location both permits and can serve (RFC 9110 section
 * 10.2.1).
 * 이 location이 허용하면서 처리할 수도 있는 메서드다 (RFC 9110 10.2.1절).
 *
 * @param[in] location The location. / location.
 * @return Comma-separated methods. / 쉼표로 구분한 메서드.
 */
std::string allowFor(const LocationConfig& location) {
  std::string allow;
  for (std::size_t i = 0; i < kStaticMethodCount; ++i) {
    if (!locationAllows(location, kStaticMethods[i])) continue;
    if (!allow.empty()) allow += ", ";
    allow += kStaticMethods[i];
  }
  return allow;
}

/**
 * @brief A 405 decision with its Allow value.
 *        Allow 값을 담은 405 결정.
 * @param[in] location The location. / location.
 * @return The decision. / 결정.
 */
RouteResult methodNotAllowed(const LocationConfig& location) {
  RouteResult result = error(http_status::kMethodNotAllowed);
  result.allow = allowFor(location);
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
 * An alias location covers a path only at a segment boundary: what is
 * left after the location must be empty or start with "/". Otherwise
 * "location /k { alias dir/; }" would map "/k../x" to "dir/../x", outside
 * dir (design decision 14). root appends the whole normalized path and
 * cannot leave its directory.
 * alias location은 조각 경계에서만 경로를 맡는다. location 뒤에 남은
 * 부분이 비었거나 "/"로 시작해야 한다. 그렇지 않으면
 * "location /k { alias dir/; }"가 "/k../x"를 dir 밖의 "dir/../x"로
 * 매핑한다 (설계 결정 14). root는 정규화한 경로 전체를 붙이므로 자기
 * 디렉토리를 벗어날 수 없다.
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

// Order of design 7: location, return, limit_except, then the file system.
// 설계 7절의 순서: location, return, limit_except, 그다음 파일 시스템.
RouteResult Router::routePath(const std::string& path, const Request& request,
                              int internalRedirects) const {
  const LocationConfig* location = matchLocation(path);
  if (location == 0) return error(http_status::kNotFound);
  if (location->returnCode != 0)
    return redirect(location->returnCode, location->returnUrl);
  if (!locationAllows(*location, request.method) ||
      !isStaticMethod(request.method))
    return methodNotAllowed(*location);

  const std::string fsPath = mapPath(*location, path);
  const FileKind kind = fileKind(fsPath);
  if (kind == FILE_MISSING) return error(http_status::kNotFound);
  if (kind == FILE_DIRECTORY)
    return routeDirectory(path, request, *location, internalRedirects);
  // Devices, FIFOs and the like are never served (RFC 9110 section 17.3).
  // 장치 파일이나 FIFO 같은 것은 절대 보내지 않는다 (RFC 9110 17.3절).
  if (kind != FILE_REGULAR || !isReadable(fsPath))
    return error(http_status::kForbidden);
  RouteResult result;
  result.kind = RouteResult::STATIC_FILE;
  result.fsPath = fsPath;
  result.urlPath = path;
  return result;
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
