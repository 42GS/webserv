/**
 * @file  ConfigBuilder.hpp
 * @brief Last stage of configuration loading: directive tree to settings.
 *        설정 읽기의 마지막 단계: 지시어 트리를 설정 값으로 바꾼다.
 */
#ifndef CONFIG_CONFIGBUILDER_HPP
#define CONFIG_CONFIGBUILDER_HPP

#include <string>
#include <vector>

#include "config/ConfigSyntax.hpp"
#include "config/ServerConfig.hpp"

/**
 * @brief Turns the directive tree into validated server settings.
 *        지시어 트리를 검증된 server 설정으로 바꾼다.
 *
 * Checks each directive's arguments, applies server-level values to
 * locations, and rejects conflicting listen addresses.
 * 지시어마다 인자를 검사하고, server 단계 값을 location에 물려주고,
 * 충돌하는 listen 주소를 거부한다.
 *
 * @param[in] tree   Directives from parseDirectives().
 *                   parseDirectives()가 만든 지시어.
 * @param[in] source File name used in error messages.
 *                   오류 메시지에 쓸 파일 이름.
 * @return One ServerConfig per "server" block, in file order.
 *         "server" 블록마다 하나씩, 파일 순서대로.
 * @throws ConfigError On the first invalid directive.
 *                     처음 만난 잘못된 지시어에서.
 */
std::vector<ServerConfig> buildServerConfigs(const DirectiveTree& tree,
                                             const std::string& source);

#endif
