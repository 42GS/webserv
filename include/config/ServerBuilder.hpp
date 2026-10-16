/**
 * @file  ServerBuilder.hpp
 * @brief Builds the settings of "server" blocks.
 *        "server" 블록의 설정을 만든다.
 */
#ifndef CONFIG_SERVERBUILDER_HPP
#define CONFIG_SERVERBUILDER_HPP

#include <vector>

#include "config/BuildContext.hpp"
#include "config/ServerConfig.hpp"

/**
 * @brief A built server with the line of each listen directive.
 *        만든 server와 각 listen 지시어의 줄 번호.
 */
struct BuiltServer {
  /** The settings. / 설정. */
  ServerConfig config;
  /**
   * @brief Line of each entry in config.listens, for error messages.
   *        config.listens 항목마다의 줄 번호. 오류 메시지에 쓴다.
   */
  std::vector<int> listenLines;
};

/**
 * @brief Builds one "server { ... }" block, including its locations.
 *        "server { ... }" 블록 하나를 location까지 포함해 만든다.
 * @param[in] context Tree and source name. / 트리와 소스 이름.
 * @param[in] block   The "server" directive. / "server" 지시어.
 * @return The server. / 만든 server.
 * @throws ConfigError For arguments after "server", no "listen", or an
 *         invalid directive inside.
 *         "server" 뒤에 인자가 있거나, "listen"이 없거나, 안의 지시어가
 *         잘못됐으면.
 */
BuiltServer buildServer(const BuildContext& context, const Directive& block);

#endif
