/**
 * @file  CgiOutput.hpp
 * @brief Turning CGI output into a response (RFC 3875 section 6).
 *        CGI 출력을 응답으로 바꾸기 (RFC 3875 6절).
 */
#ifndef CGI_CGIOUTPUT_HPP
#define CGI_CGIOUTPUT_HPP

#include <string>

#include "http/Response.hpp"

/**
 * @brief What the script's output asks for.
 *        스크립트 출력이 요구하는 것.
 */
struct CgiOutput {
  /**
   * @brief Kinds of output.
   *        출력의 종류.
   */
  enum Kind {
    /**
     * @brief A document or a client redirect, ready in response.
     *        문서나 클라이언트 리다이렉트. response에 준비돼 있다.
     */
    RESPONSE,
    /**
     * @brief Answer as if localTarget had been requested.
     *        localTarget을 요청받은 것처럼 답한다.
     */
    LOCAL_REDIRECT,
    /** Not a CGI response: 502. / CGI 응답이 아님: 502. */
    INVALID
  };

  /** @brief An INVALID output. / INVALID 출력. */
  CgiOutput() : kind(INVALID) {}

  /** Kind of output. / 출력의 종류. */
  Kind kind;
  /** RESPONSE: status, fields and body. / RESPONSE: 상태, 필드, 본문. */
  Response response;
  /** LOCAL_REDIRECT: path and query. / LOCAL_REDIRECT: 경로와 query. */
  std::string localTarget;
};

/**
 * @brief Parses the complete output of a script.
 *        스크립트의 출력 전체를 해석한다.
 *
 * Lines may end in LF or CRLF (section 7.2 asks servers to accept both).
 * Fields that would change the framing or the connection (Content-Length,
 * Transfer-Encoding, Connection) are dropped, as is Date, which the
 * server sends itself (section 6.3.4 lets the server resolve such
 * conflicts).
 * 줄 끝은 LF나 CRLF다 (7.2절은 서버가 둘 다 받으라고 한다). 메시지 길이나
 * 연결을 바꾸는 필드(Content-Length, Transfer-Encoding, Connection)는
 * 버린다. Date도 서버가 직접 보내므로 버린다 (6.3.4절은 이런 충돌을 서버가
 * 해결하게 한다).
 *
 * @param[in,out] output Everything the script wrote; its body is moved
 *                       into the result, not copied, and the text is left
 *                       unspecified.
 *                       스크립트가 쓴 전부. 본문은 복사하지 않고 결과로
 *                       옮기며, 남은 텍스트는 정하지 않는다.
 * @return The parsed output; INVALID if it breaks the grammar of section
 *         6.2.
 *         해석한 출력. 6.2절 문법에 맞지 않으면 INVALID.
 */
CgiOutput parseCgiOutput(std::string& output);

#endif
