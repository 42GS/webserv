/**
 * @file  ProcessOutcome.hpp
 * @brief What processing a request led to.
 *        요청을 처리한 결과.
 */
#ifndef APP_PROCESSOUTCOME_HPP
#define APP_PROCESSOUTCOME_HPP

#include "cgi/CgiProcess.hpp"
#include "http/Request.hpp"
#include "http/Response.hpp"

/**
 * @brief A response, a started CGI process, or a request to answer
 *        instead (design 7).
 *        응답, 시작한 CGI 프로세스, 또는 대신 답할 요청 (설계 7절).
 */
struct ProcessOutcome {
  /**
   * @brief Kinds of outcome.
   *        결과의 종류.
   */
  enum Kind {
    /** Send response. / response를 보낸다. */
    RESPONSE,
    /**
     * @brief Wait for cgi, which the caller now owns.
     *        cgi를 기다린다. 이제 호출자가 소유한다.
     */
    CGI,
    /**
     * @brief Answer redirect instead: a CGI local redirect (RFC 3875
     *        section 6.2.2).
     *        대신 redirect에 답한다: CGI 로컬 리다이렉트 (RFC 3875 6.2.2절).
     */
    LOCAL_REDIRECT
  };

  /** @brief A RESPONSE with an empty response. / 빈 응답의 RESPONSE. */
  ProcessOutcome() : kind(RESPONSE), cgi(0) {}

  /** Kind of outcome. / 결과의 종류. */
  Kind kind;
  /** RESPONSE: the response. / RESPONSE: 응답. */
  Response response;
  /** CGI: the started process. / CGI: 시작한 프로세스. */
  CgiProcess* cgi;
  /** LOCAL_REDIRECT: the request to answer. / LOCAL_REDIRECT: 답할 요청. */
  Request redirect;
};

#endif
