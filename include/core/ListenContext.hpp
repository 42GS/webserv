/**
 * @file  ListenContext.hpp
 * @brief Where a connection came in.
 *        연결이 들어온 곳.
 */
#ifndef CORE_LISTENCONTEXT_HPP
#define CORE_LISTENCONTEXT_HPP

#include <string>

#include "config/ServerConfig.hpp"

/**
 * @brief Where a connection came in: the server block that owns the
 *        listen address, and both ends of the connection (for CGI).
 *        연결이 들어온 곳: 그 listen 주소를 가진 server 블록과 연결의 두
 *        끝 주소 (CGI용).
 */
struct ListenContext {
  /** @brief No server, port 0. / server 없음, 포트 0. */
  ListenContext() : server(0), localPort(0) {}

  /**
   * @brief Owned by main, lives for the whole run.
   *        main이 소유하고 실행 내내 산다.
   */
  const ServerConfig* server;
  /** Client address, dotted. / 점 표기 클라이언트 주소. */
  std::string remoteAddress;
  /** Server address of the connection, dotted. / 연결의 서버 쪽 주소. */
  std::string localAddress;
  /** Server port of the connection. / 연결의 서버 쪽 포트. */
  unsigned short localPort;
};

#endif
