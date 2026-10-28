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

#include "app/ConnectionServices.hpp"
#include "app/ProcessOutcome.hpp"
#include "cgi/CgiProcess.hpp"
#include "core/ListenContext.hpp"
#include "http/Request.hpp"
#include "http/Response.hpp"
#include "route/Router.hpp"

/**
 * @brief One complete request to one response (design 2 "app").
 *        완성된 요청 하나를 응답 하나로 바꾼다 (설계 2절 "app").
 *
 * Router decides, a handler builds the response or a CGI process starts,
 * and the common fields are added.
 * Router가 정하고, handler가 응답을 만들거나 CGI 프로세스를 시작하고,
 * 공통 필드를 붙인다.
 */
class RequestProcessor {
 public:
  /**
   * @brief A processor for one connection.
   *        연결 하나의 processor.
   * @param[in] where    Server block and both ends of the connection;
   *                     copied.
   *                     server 블록과 연결의 두 끝. 복사한다.
   * @param[in] services For CGI and the error log; copied, and the copy
   *                     refers to the same objects.
   *                     CGI와 오류 로그용. 복사하며, 복사본도 같은 객체를
   *                     가리킨다.
   */
  RequestProcessor(const ListenContext& where,
                   const ConnectionServices& services);

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
   * A script is checked (design decision 12) and started; its output is
   * turned into a response by afterCgi().
   * 스크립트는 검사하고 (설계 결정 12) 시작한다. 출력은 afterCgi()가
   * 응답으로 바꾼다.
   *
   * @param[in] request The request; must outlive a started CGI process,
   *                    which reads its body.
   *                    요청. 본문을 읽는 CGI 프로세스보다 오래 살아야 한다.
   * @param[in] now     Time for the Date field and the CGI timeout.
   *                    Date 필드와 CGI 시간 초과에 쓸 시각.
   * @return A response, a started CGI process, or 500 if it could not
   *         start.
   *         응답, 시작한 CGI 프로세스, 또는 시작하지 못했으면 500.
   */
  ProcessOutcome process(const Request& request, std::time_t now) const;

  /**
   * @brief Turns the output of a finished script into the outcome.
   *        끝난 스크립트의 출력을 결과로 바꾼다.
   *
   * Unusable output is 502 (wiki concepts/cgi section 5). A local
   * redirect asks for the path with GET, or HEAD for a HEAD request: the
   * body is not sent twice (RFC 3875 section 6.2.2 leaves the method
   * open).
   * 쓸 수 없는 출력은 502다 (위키 concepts/cgi 5절). 로컬 리다이렉트는
   * 그 경로를 GET으로, HEAD 요청이면 HEAD로 요청한다. 본문을 두 번 보내지
   * 않는다 (RFC 3875 6.2.2절은 메서드를 정하지 않는다).
   *
   * @param[in] request The request the script answered. / 스크립트가 답한 요청.
   * @param[in,out] cgi The finished process; its output is taken.
   *                    끝난 프로세스. 출력을 가져간다.
   * @param[in] now     Time for the Date field. / Date 필드에 쓸 시각.
   * @return A response or a local redirect. / 응답이나 로컬 리다이렉트.
   */
  ProcessOutcome afterCgi(const Request& request, CgiProcess& cgi,
                          std::time_t now) const;

  /**
   * @brief A status page for a request that was read completely (CGI
   *        timeout, too many redirects).
   *        끝까지 읽은 요청에 보내는 상태 페이지 (CGI 시간 초과, 너무 많은
   *        리다이렉트).
   * @param[in] status  Status code. / 상태 코드.
   * @param[in] request The request. / 요청.
   * @param[in] now     Time for the Date field. / Date 필드에 쓸 시각.
   * @return The response; the connection may stay open.
   *         응답. 연결은 유지될 수 있다.
   */
  Response statusResponse(int status, const Request& request,
                          std::time_t now) const;

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
   * @return The response before the common fields.
   *         공통 필드를 붙이기 전의 응답.
   */
  Response respond(const RouteResult& route, const Request& request) const;

  /**
   * @brief Stores an upload and answers with its result.
   *        업로드를 저장하고 그 결과로 답한다.
   * @param[in] route   UPLOAD decision. / UPLOAD 결정.
   * @param[in] request The request. / 요청.
   * @return 201 or 409 from the upload handler, else a status page.
   *         업로드 handler가 만든 201이나 409, 아니면 상태 페이지.
   */
  Response uploadResponse(const RouteResult& route,
                          const Request& request) const;

  /**
   * @brief Checks a script and its program, then starts it.
   *        스크립트와 프로그램을 검사한 뒤 시작한다.
   *
   * @param[in] script  CGI decision. / CGI 결정.
   * @param[in] request The request. / 요청.
   * @param[in] now     Start time. / 시작 시각.
   * @return The started process, or a response. / 시작한 프로세스, 또는 응답.
   */
  ProcessOutcome startCgi(const RouteResult& script, const Request& request,
                          std::time_t now) const;

  /**
   * @brief A RESPONSE outcome with a status page and the common fields.
   *        상태 페이지와 공통 필드를 담은 RESPONSE 결과.
   * @param[in] status  Status code. / 상태 코드.
   * @param[in] request The request. / 요청.
   * @param[in] now     Time for the Date field. / Date 필드에 쓸 시각.
   * @return The outcome. / 결과.
   */
  ProcessOutcome statusOutcome(int status, const Request& request,
                               std::time_t now) const;

  /**
   * @brief A RESPONSE outcome with the common fields added.
   *        공통 필드를 붙인 RESPONSE 결과.
   * @param[in] response The response. / 응답.
   * @param[in] request  The request. / 요청.
   * @param[in] now      Time for the Date field. / Date 필드에 쓸 시각.
   * @return The outcome. / 결과.
   */
  ProcessOutcome finished(const Response& response, const Request& request,
                          std::time_t now) const;

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

  /** Server block and connection ends. / server 블록과 연결의 두 끝. */
  ListenContext where_;
  /** For CGI processes and the error log. / CGI 프로세스와 오류 로그용. */
  ConnectionServices services_;
  /** Router of the server block. / server 블록의 router. */
  Router router_;
};

#endif
