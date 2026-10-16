/**
 * @file  LocationBuilder.hpp
 * @brief Builds the settings of "location" blocks.
 *        "location" 블록의 설정을 만든다.
 */
#ifndef CONFIG_LOCATIONBUILDER_HPP
#define CONFIG_LOCATIONBUILDER_HPP

#include "config/BuildContext.hpp"
#include "config/ServerConfig.hpp"
#include "config/SharedSettings.hpp"

/**
 * @brief Builds one "location PATH { ... }" block on top of the server's
 *        settings.
 *        server 설정 위에 "location PATH { ... }" 블록 하나를 만든다.
 * @param[in] context Tree and source name. / 트리와 소스 이름.
 * @param[in] block   The "location" directive. / "location" 지시어.
 * @param[in] server  Values inherited from the server block.
 *                    server 블록에서 물려받는 값.
 * @return The location. / 만든 location.
 * @throws ConfigError For modifiers, a path not starting with "/", both
 *         root and alias, no root or alias without "return", or an
 *         invalid directive inside.
 *         modifier가 있거나, 경로가 "/"로 시작하지 않거나, root와 alias가
 *         함께 있거나, "return" 없이 root도 alias도 없거나, 안의 지시어가
 *         잘못됐으면.
 */
LocationConfig buildLocation(const BuildContext& context,
                             const Directive& block,
                             const SharedSettings& server);

/**
 * @brief The location "/" a server gets when it has a root but no
 *        "location /".
 *        root는 있지만 "location /"가 없는 server에 붙는 location "/".
 * @param[in] server The server's settings. / server 설정.
 * @return The location. / 만든 location.
 */
LocationConfig makeImplicitRootLocation(const SharedSettings& server);

#endif
