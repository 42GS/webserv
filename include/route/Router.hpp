/**
 * @file  Router.hpp
 * @brief Decides how a request is answered.
 *        요청에 어떻게 답할지 정한다.
 */
#ifndef ROUTE_ROUTER_HPP
#define ROUTE_ROUTER_HPP

#include <string>

#include "config/ServerConfig.hpp"
#include "http/Request.hpp"
#include "route/RouteResult.hpp"

/**
 * @brief Request and configuration to how to answer (design 7).
 *        요청과 설정으로 답할 방법을 정한다 (설계 7절).
 *
 * Path normalization, location, return, method check, file system mapping
 * and directory handling.
 * 경로 정규화, location 선택, return, 메서드 검사, 파일 시스템 매핑,
 * 디렉토리 처리를 한다.
 */
class Router {
 public:
  /**
   * @brief A router for one server block.
   *        server 블록 하나의 router.
   * @param[in] server Settings; kept by reference. / 설정. 참조로 보관한다.
   */
  explicit Router(const ServerConfig& server);

  /**
   * @brief The location with the longest path that is a prefix of @p path.
   *        경로가 @p path의 prefix인 location 중 가장 긴 것.
   *
   * Locations are sorted longest first by the configuration loader. An
   * alias location that does not cover the path at a segment boundary
   * also gives 0 (design decision 14).
   * location은 설정 loader가 긴 것부터 정렬해 두었다. 조각 경계에서
   * 경로를 덮지 못하는 alias location이면 역시 0이다 (설계 결정 14).
   *
   * @param[in] path Normalized path. / 정규화한 경로.
   * @return The location, or 0. / location, 또는 0.
   */
  const LocationConfig* matchLocation(const std::string& path) const;

  /**
   * @brief The location for a percent-encoded request path.
   *        퍼센트 인코딩된 요청 경로의 location.
   * @param[in] encodedPath Request path. / 요청 경로.
   * @return The location, or 0 if the path cannot be normalized or no
   *         location matches.
   *         location. 경로를 정규화할 수 없거나 맞는 location이 없으면 0.
   */
  const LocationConfig* locationFor(const std::string& encodedPath) const;

  /**
   * @brief Decides how to answer a request.
   *        요청에 답할 방법을 정한다.
   * @param[in] request Parsed request. / 파싱한 요청.
   * @return The decision. / 결정.
   */
  RouteResult route(const Request& request) const;

  /**
   * @brief File system path for a URI path through its location's root or
   *        alias (for error pages).
   *        URI 경로를 location의 root나 alias로 파일 시스템 경로로 바꾼다
   *        (오류 페이지용).
   * @param[in]  uriPath URI path, percent-encoded. / 퍼센트 인코딩된 URI 경로.
   * @param[out] fsPath  File system path. / 파일 시스템 경로.
   * @return False if the path is invalid or has no location.
   *         경로가 잘못됐거나 location이 없으면 false.
   */
  bool mapToFile(const std::string& uriPath, std::string& fsPath) const;

 private:
  /**
   * @brief Normalizes and matches.
   *        정규화하고 location을 고른다.
   * @param[in]  encodedPath Request path. / 요청 경로.
   * @param[out] path        Normalized path. / 정규화한 경로.
   * @param[out] location    Matched location. / 고른 location.
   * @return False as for locationFor(). / locationFor()와 같은 경우에 false.
   */
  bool resolve(const std::string& encodedPath, std::string& path,
               const LocationConfig*& location) const;

  /**
   * @brief Routes a normalized path in the order of design 7.
   *        정규화한 경로를 설계 7절의 순서로 처리한다.
   * @param[in] path              Normalized path. / 정규화한 경로.
   * @param[in] request           The request. / 요청.
   * @param[in] internalRedirects Index redirects so far. / 지금까지의 index
   * 리다이렉트 수.
   * @return The decision. / 결정.
   */
  RouteResult routePath(const std::string& path, const Request& request,
                        int internalRedirects) const;

  /**
   * @brief Fills pathTranslated of a CGI decision: PATH_INFO mapped like a
   *        request path.
   *        CGI 결정의 pathTranslated를 채운다: PATH_INFO를 요청 경로처럼
   *        매핑한 것.
   * @param[in,out] script CGI decision. / CGI 결정.
   */
  void translatePathInfo(RouteResult& script) const;

  /**
   * @brief Handles a directory: redirect to "/", index file, listing or 403.
   *        디렉토리를 처리한다: "/"로 리다이렉트, index 파일, 목록, 또는 403.
   * @param[in] path              Normalized path. / 정규화한 경로.
   * @param[in] request           The request. / 요청.
   * @param[in] location          Its location. / 경로의 location.
   * @param[in] internalRedirects Index redirects so far. / 지금까지의 index
   * 리다이렉트 수.
   * @return The decision. / 결정.
   */
  RouteResult routeDirectory(const std::string& path, const Request& request,
                             const LocationConfig& location,
                             int internalRedirects) const;

  /** Settings of the server block. / server 블록의 설정. */
  const ServerConfig& server_;
};

#endif
