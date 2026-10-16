#include "route/Router.hpp"

#include <sys/stat.h>
#include <unistd.h>

#include "core/Limits.hpp"
#include "http/StatusCodes.hpp"
#include "route/PathResolver.hpp"

namespace {

RouteResult error(int status) {
  RouteResult result;
  result.kind = RouteResult::ERROR;
  result.status = status;
  return result;
}

RouteResult redirect(int status, const std::string& target) {
  RouteResult result;
  result.kind = RouteResult::REDIRECT;
  result.status = status;
  result.redirectTo = target;
  return result;
}

bool locationAllows(const LocationConfig& location, const std::string& method) {
  return location.allowedMethods.empty() ||
         location.allowedMethods.count(method) != 0;
}

bool isStaticMethod(const std::string& method) {
  for (std::size_t i = 0; i < router::kStaticMethodCount; ++i) {
    if (method == router::kStaticMethods[i]) return true;
  }
  return false;
}

// The Allow value of a 405: methods this location both permits and can
// serve (RFC 9110 section 10.2.1).
std::string allowFor(const LocationConfig& location) {
  std::string allow;
  for (std::size_t i = 0; i < router::kStaticMethodCount; ++i) {
    if (!locationAllows(location, router::kStaticMethods[i])) continue;
    if (!allow.empty()) allow += ", ";
    allow += router::kStaticMethods[i];
  }
  return allow;
}

RouteResult methodNotAllowed(const LocationConfig& location) {
  RouteResult result = error(http_status::kMethodNotAllowed);
  result.allow = allowFor(location);
  result.location = &location;
  return result;
}

// root: the whole URI is appended; alias: the location prefix is replaced
// (concepts/configuration-file).
std::string mapPath(const LocationConfig& location, const std::string& path) {
  if (!location.alias.empty())
    return location.alias + path.substr(location.path.size());
  return location.root + path;
}

std::string withQuery(const std::string& path, const std::string& query) {
  return query.empty() ? path : path + "?" + query;
}

bool isRegularFile(const std::string& fsPath) {
  struct stat info;
  return stat(fsPath.c_str(), &info) == 0 && S_ISREG(info.st_mode);
}

}  // namespace

Router::Router(const ServerConfig& server) : server_(server) {}

const LocationConfig* Router::matchLocation(const std::string& path) const {
  for (std::size_t i = 0; i < server_.locations.size(); ++i) {
    const std::string& prefix = server_.locations[i].path;
    if (path.compare(0, prefix.size(), prefix) == 0)
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

bool Router::mapToFile(const std::string& uriPath, std::string& fsPath) const {
  std::string path;
  if (!normalizePath(uriPath, path)) return false;
  const LocationConfig* location = matchLocation(path);
  if (location == 0) return false;
  fsPath = mapPath(*location, path);
  return true;
}

// Order of design 7: location, return, limit_except, then the file system.
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
  struct stat info;
  // stat() failing means missing (or unreachable): 404 without errno.
  if (stat(fsPath.c_str(), &info) != 0) return error(http_status::kNotFound);
  if (S_ISDIR(info.st_mode))
    return routeDirectory(path, request, *location, internalRedirects);
  // Devices, FIFOs and the like are never served (RFC 9110 section 17.3).
  if (!S_ISREG(info.st_mode) || access(fsPath.c_str(), R_OK) != 0)
    return error(http_status::kForbidden);
  RouteResult result;
  result.kind = RouteResult::STATIC_FILE;
  result.fsPath = fsPath;
  result.urlPath = path;
  result.location = location;
  return result;
}

// concepts/configuration-file "디렉토리 요청 처리 순서".
RouteResult Router::routeDirectory(const std::string& path,
                                   const Request& request,
                                   const LocationConfig& location,
                                   int internalRedirects) const {
  if (path[path.size() - 1] != '/')
    return redirect(http_status::kMovedPermanently,
                    withQuery(encodePath(path + "/"), request.query));
  const std::string fsPath = mapPath(location, path);
  for (std::size_t i = 0; i < location.indexFiles.size(); ++i) {
    if (!isRegularFile(fsPath + location.indexFiles[i])) continue;
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
  result.location = &location;
  return result;
}
