/**
 * @file  Request.hpp
 * @brief A parsed request.
 *        파싱한 요청.
 */
#ifndef HTTP_REQUEST_HPP
#define HTTP_REQUEST_HPP

#include <string>

#include "http/HeaderMap.hpp"

/**
 * @brief A parsed request.
 *        파싱한 요청.
 *
 * "path" is still percent-encoded; decoding and dot segments are the
 * router's job (concepts/security, step 4).
 * "path"는 아직 퍼센트 인코딩된 상태다. 디코딩과 dot segment 처리는
 * router가 한다 (concepts/security, 4단계).
 */
struct Request {
  /** Method, one of the implemented ones. / 메서드, 구현한 것 중 하나. */
  std::string method;
  /** request-target as received. / 받은 그대로의 request-target. */
  std::string target;
  /** Path before the first "?". / 첫 "?" 앞의 경로. */
  std::string path;
  /**
   * @brief Text after the first "?", not decoded.
   *        첫 "?" 뒤의 텍스트, 디코딩 안 함.
   */
  std::string query;
  /** "HTTP/1.0" or "HTTP/1.1". / "HTTP/1.0" 또는 "HTTP/1.1". */
  std::string version;
  /** Header fields. / 헤더 필드. */
  HeaderMap headers;
  /**
   * @brief Body with the chunked coding already removed.
   *        chunked 인코딩을 푼 본문.
   */
  std::string body;
};

#endif
