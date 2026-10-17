/**
 * @file  RequestProcessor.hpp
 * @brief From a complete request to a response.
 *        완성된 요청에서 응답까지.
 */
#ifndef APP_REQUESTPROCESSOR_HPP
#define APP_REQUESTPROCESSOR_HPP

#include <cstddef>
#include <ctime>
#include <string>

#include "config/ServerConfig.hpp"
#include "handler/UploadHandler.hpp"
#include "http/Request.hpp"
#include "http/Response.hpp"
#include "route/Router.hpp"

/**
 * @brief One complete request to one response (design 2 "app").
 *        완성된 요청 하나를 응답 하나로 바꾼다 (설계 2절 "app").
 *
 * Router decides, a handler builds the response, and the common fields
 * are added.
 * Router가 정하고, handler가 응답을 만들고, 공통 필드를 붙인다.
 */
class RequestProcessor {
 public:
  /**
   * @brief A processor for one server block.
   *        server 블록 하나의 processor.
   * @param[in] server Settings; kept by reference. / 설정. 참조로 보관한다.
   */
  explicit RequestProcessor(const ServerConfig& server);

  /**
   * @brief client_max_body_size of the location for this request.
   *        이 요청의 location에 정한 client_max_body_size.
   *
   * A path that cannot be normalized or matched gets the default; routing
   * answers it with 400 or 404 after the body.
   * 정규화하거나 매칭할 수 없는 경로는 기본값을 받는다. 그런 요청은 본문
   * 뒤에 라우팅이 400이나 404로 답한다.
   *
   * @param[in] request Request whose head is parsed. / 머리를 파싱한 요청.
   * @return Limit in bytes. / 바이트 단위 상한.
   */
  std::size_t bodyLimitFor(const Request& request) const;

  /**
   * @brief Builds the response to a complete request.
   *        완성된 요청의 응답을 만든다.
   *
   * The connection stays open if the request allows it and the response
   * is not a 400: a bad path closes it, as NGINX does (design decision 2),
   * and so does a malformed upload body.
   * 요청이 허용하고 응답이 400이 아니면 연결을 유지한다. 잘못된 경로는
   * NGINX처럼 연결을 닫고 (설계 결정 2), 잘못된 업로드 본문도 닫는다.
   *
   * @param[in] request The request. / 요청.
   * @param[in] now     Time for the Date field. / Date 필드에 쓸 시각.
   * @return The response. / 응답.
   */
  Response process(const Request& request, std::time_t now) const;

  /**
   * @brief Response for a request that could not be processed (parse
   *        error, timeout).
   *        처리할 수 없었던 요청(파싱 오류, 시간 초과)의 응답.
   *
   * The rest of such a request cannot be trusted, so the connection
   * closes after it (RFC 9112 section 9.3).
   * 그런 요청의 나머지는 믿을 수 없으므로 응답 뒤 연결을 닫는다 (RFC 9112
   * 9.3절).
   *
   * @param[in] status Status code. / 상태 코드.
   * @param[in] method Request method, empty if the request line was not
   *                   understood.
   *                   요청 메서드. 요청 줄을 이해하지 못했으면 빈 값.
   * @param[in] now    Time for the Date field. / Date 필드에 쓸 시각.
   * @return The response. / 응답.
   */
  Response errorResponse(int status, const std::string& method,
                         std::time_t now) const;

 private:
  /**
   * @brief Calls the handler for a routing decision.
   *        라우팅 결정에 맞는 handler를 부른다.
   * @param[in] route   The decision. / 결정.
   * @param[in] request The request, for uploads. / 요청. 업로드에 쓴다.
   * @return The response before the common fields. / 공통 필드를 붙이기 전의
   * 응답.
   */
  Response respond(const RouteResult& route, const Request& request) const;

  /**
   * @brief Stores an upload and answers 201 or the error.
   *        업로드를 저장하고 201이나 오류로 답한다.
   *
   * A 201 has the first stored file in Location and lists the stored
   * files as text (RFC 9110 section 9.3.3).
   * 201은 처음 저장한 파일을 Location에 담고, 저장한 파일을 텍스트로
   * 나열한다 (RFC 9110 9.3.3절).
   *
   * @param[in] route   UPLOAD decision. / UPLOAD 결정.
   * @param[in] request The request. / 요청.
   * @return The response. / 응답.
   */
  Response uploadResponse(const RouteResult& route,
                          const Request& request) const;

  /**
   * @brief The URI of an uploaded file, if GET reaches it.
   *        GET으로 닿는다면, 업로드한 파일의 URI.
   *
   * The file is taken to sit next to the POST target; that URI counts
   * only if it maps back to the stored file, since the store is a file
   * system path that need not be under any location.
   * 파일이 POST 대상 옆에 있다고 보고, 그 URI가 저장한 파일로 다시 매핑될
   * 때만 쓴다. 저장 위치는 어느 location 아래에도 없을 수 있는 파일
   * 시스템 경로이기 때문이다.
   *
   * @param[in] targetPath Normalized POST path. / 정규화한 POST 경로.
   * @param[in] file       The stored file. / 저장한 파일.
   * @return Percent-encoded URI, or empty. / 퍼센트 인코딩한 URI, 또는 빈 값.
   */
  std::string uriOfUpload(const std::string& targetPath,
                          const UploadedFile& file) const;

  /**
   * @brief buildStatusPage() with this server's error pages.
   *        이 server의 오류 페이지로 buildStatusPage()를 부른다.
   * @param[in] status Status code. / 상태 코드.
   * @return The response. / 응답.
   */
  Response statusPage(int status) const;

  /**
   * @brief Adds the fields every response has, and drops the body for
   *        HEAD.
   *        모든 응답에 붙는 필드를 넣고, HEAD면 본문을 뺀다.
   * @param[in,out] response  Response to finish. / 마무리할 응답.
   * @param[in]     method    Request method. / 요청 메서드.
   * @param[in]     keepAlive Keep the connection open after it.
   *                          응답 뒤에 연결을 유지할지.
   * @param[in]     now       Time for the Date field. / Date 필드에 쓸 시각.
   */
  void finish(Response& response, const std::string& method, bool keepAlive,
              std::time_t now) const;

  /** Settings of the server block. / server 블록의 설정. */
  const ServerConfig& server_;
  /** Router of the server block. / server 블록의 router. */
  Router router_;
};

#endif
