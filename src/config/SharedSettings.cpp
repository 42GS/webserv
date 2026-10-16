/**
 * @file  SharedSettings.cpp
 * @brief Directives shared by server and location blocks.
 *        server와 location 블록이 함께 쓰는 지시어.
 */
#include "config/SharedSettings.hpp"

#include "config/ConfigError.hpp"
#include "config/ConfigLimits.hpp"
#include "config/ConfigValues.hpp"

namespace {

/** NGINX default of "index". / "index"의 NGINX 기본값. */
const char* const kDefaultIndexFile = "index.html";

/**
 * @brief Parses the client_max_body_size argument.
 *        client_max_body_size 인자를 해석한다.
 * @param[in] context   Tree and source name. / 트리와 소스 이름.
 * @param[in] directive The directive. / 지시어.
 * @return Size in bytes, 1 to config_limits::kMaxBodySizeCeiling.
 *         바이트 단위 크기. 1부터 config_limits::kMaxBodySizeCeiling까지.
 * @throws ConfigError For an invalid size, 0, or a size above the
 *         ceiling.
 *         크기가 잘못됐거나, 0이거나, 상한을 넘으면.
 */
std::size_t parseBodySize(const BuildContext& context,
                          const Directive& directive) {
  std::size_t size = 0;
  if (!parseSize(directive.args[0], size))
    throw ConfigError(context.source, directive.line,
                      "invalid size \"" + directive.args[0] + "\"");
  if (size == 0 || size > config_limits::kMaxBodySizeCeiling)
    throw ConfigError(context.source, directive.line,
                      "\"client_max_body_size\" must be between 1 and 100m");
  return size;
}

}  // namespace

SharedSettings SharedSettings::defaults() {
  SharedSettings settings;
  settings.indexFiles.push_back(kDefaultIndexFile);
  settings.autoindex = false;
  settings.maxBodySize = config_limits::kDefaultMaxBodySize;
  return settings;
}

bool applySharedDirective(const BuildContext& context,
                          const Directive& directive,
                          SharedSettings& settings) {
  if (directive.name == "root") {
    settings.root = directive.args[0];
  } else if (directive.name == "index") {
    // A name with "/" (such as "../x") could point outside the root
    // (design decision 14).
    // "/"가 든 이름(예: "../x")은 root 밖을 가리킬 수 있다 (설계 결정 14).
    for (std::size_t i = 0; i < directive.args.size(); ++i) {
      if (directive.args[i].find('/') != std::string::npos)
        throw ConfigError(context.source, directive.line,
                          "\"index\" file name must not contain \"/\"");
    }
    settings.indexFiles = directive.args;
  } else if (directive.name == "autoindex") {
    if (directive.args[0] != "on" && directive.args[0] != "off")
      throw ConfigError(context.source, directive.line,
                        "\"autoindex\" must be \"on\" or \"off\"");
    settings.autoindex = (directive.args[0] == "on");
  } else if (directive.name == "client_max_body_size") {
    settings.maxBodySize = parseBodySize(context, directive);
  } else {
    return false;
  }
  return true;
}

void copySharedSettings(const SharedSettings& settings,
                        LocationConfig& location) {
  location.indexFiles = settings.indexFiles;
  location.autoindex = settings.autoindex;
  location.maxBodySize = settings.maxBodySize;
}
