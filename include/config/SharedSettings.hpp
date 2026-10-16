/**
 * @file  SharedSettings.hpp
 * @brief Directives allowed in both server and location blocks.
 *        server 블록과 location 블록에 모두 쓸 수 있는 지시어.
 */
#ifndef CONFIG_SHAREDSETTINGS_HPP
#define CONFIG_SHAREDSETTINGS_HPP

#include <cstddef>
#include <string>
#include <vector>

#include "config/BuildContext.hpp"
#include "config/ServerConfig.hpp"

/**
 * @brief Settings that may appear in both server and location blocks.
 *        server 블록과 location 블록에 모두 나올 수 있는 설정.
 *
 * A location starts from its server's values and overrides what it sets
 * itself.
 * location은 server의 값에서 시작해서, 자기가 지정한 값만 덮어쓴다.
 */
struct SharedSettings {
  /** "root". */
  std::string root;
  /** "index". */
  std::vector<std::string> indexFiles;
  /** "autoindex". */
  bool autoindex;
  /** "client_max_body_size". */
  std::size_t maxBodySize;

  /**
   * @brief NGINX defaults: index.html, autoindex off, 1m, no root.
   *        NGINX 기본값: index.html, autoindex off, 1m, root 없음.
   * @return The defaults. / 기본값.
   */
  static SharedSettings defaults();
};

/**
 * @brief Applies root, index, autoindex or client_max_body_size.
 *        root, index, autoindex, client_max_body_size 중 하나를 적용한다.
 * @param[in]     context   Tree and source name. / 트리와 소스 이름.
 * @param[in]     directive A directive whose shape is already checked.
 *                          모양 검사를 마친 지시어.
 * @param[in,out] settings  Settings to update. / 고칠 설정.
 * @return False if the directive is not one of the four.
 *         네 지시어 중 하나가 아니면 false.
 * @throws ConfigError For an invalid value. / 값이 잘못됐으면.
 */
bool applySharedDirective(const BuildContext& context,
                          const Directive& directive, SharedSettings& settings);

/**
 * @brief Copies index, autoindex and body size into the location.
 *        index, autoindex, 본문 크기를 location에 복사한다.
 *
 * Root and alias are decided by the caller.
 * root와 alias는 호출자가 정한다.
 *
 * @param[in]  settings The values. / 복사할 값.
 * @param[out] location Location to fill. / 채울 location.
 */
void copySharedSettings(const SharedSettings& settings,
                        LocationConfig& location);

#endif
