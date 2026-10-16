/**
 * @file  Response.hpp
 * @brief A response before serialization.
 *        직렬화하기 전의 응답.
 */
#ifndef HTTP_RESPONSE_HPP
#define HTTP_RESPONSE_HPP

#include <cstddef>
#include <string>

#include "http/HeaderMap.hpp"

/**
 * @brief A response before serialization (design 8).
 *        직렬화하기 전의 응답 (설계 8절).
 *
 * The body is either in memory or, for a large static file, a path read
 * in pieces by ResponseStream.
 * 본문은 메모리에 있거나, 큰 정적 파일이면 ResponseStream이 나눠 읽을
 * 경로로 둔다.
 */
struct Response {
  /** @brief An empty response with status 0. / 상태 0인 빈 응답. */
  Response() : status(0), bodyFileSize(0), omitBody(false), keepAlive(false) {}

  /** Status code. / 상태 코드. */
  int status;
  /**
   * @brief Fields without Content-Length, which ResponseStream adds.
   *        Content-Length를 뺀 필드. 그것은 ResponseStream이 붙인다.
   */
  HeaderMap headers;
  /**
   * @brief Body in memory, used when bodyFile is empty.
   *        메모리의 본문. bodyFile이 비었을 때 쓴다.
   */
  std::string body;
  /**
   * @brief File to stream as the body, or empty.
   *        본문으로 나눠 보낼 파일, 없으면 빈 값.
   */
  std::string bodyFile;
  /**
   * @brief Size of bodyFile, measured before sending.
   *        보내기 전에 잰 bodyFile의 크기.
   */
  std::size_t bodyFileSize;
  /**
   * @brief HEAD: same head as GET, no content (RFC 9110 9.3.2).
   *        HEAD: GET과 같은 머리, 내용 없음 (RFC 9110 9.3.2).
   */
  bool omitBody;
  /**
   * @brief Keep the connection open after sending; otherwise the response
   *        carries "Connection: close".
   *        보낸 뒤 연결을 유지한다. 아니면 응답에 "Connection: close"가
   *        붙는다.
   */
  bool keepAlive;
};

#endif
