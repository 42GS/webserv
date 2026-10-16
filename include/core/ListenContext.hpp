/**
 * @file  ListenContext.hpp
 * @brief Where a connection came in.
 *        연결이 들어온 곳.
 */
#ifndef CORE_LISTENCONTEXT_HPP
#define CORE_LISTENCONTEXT_HPP

#include "config/ServerConfig.hpp"

/**
 * @brief Where a connection came in: the server block that owns the
 *        listen address.
 *        연결이 들어온 곳: 그 listen 주소를 가진 server 블록.
 *
 * The local and peer addresses come back when CGI needs them (step 7).
 * 로컬·상대 주소는 CGI에서 필요해지면(7단계) 다시 넣는다.
 */
struct ListenContext {
  /**
   * @brief Owned by main, lives for the whole run.
   *        main이 소유하고 실행 내내 산다.
   */
  const ServerConfig* server;
};

#endif
