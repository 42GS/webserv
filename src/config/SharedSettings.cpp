#include "config/SharedSettings.hpp"

#include "config/ConfigLimits.hpp"
#include "config/ConfigValues.hpp"

namespace {

std::size_t parseBodySize(const BuildContext& context,
                          const Directive& directive) {
  std::size_t size = 0;
  if (!parseSize(directive.args[0], size))
    failAt(context, directive, "invalid size \"" + directive.args[0] + "\"");
  if (size == 0 || size > config_limits::kMaxBodySizeCeiling)
    failAt(context, directive,
           "\"client_max_body_size\" must be between 1 and 100m");
  return size;
}

}  // namespace

SharedSettings SharedSettings::defaults() {
  SharedSettings settings;
  settings.indexFiles.push_back(shared_settings::kDefaultIndexFile);
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
    for (std::size_t i = 0; i < directive.args.size(); ++i) {
      if (directive.args[i].find('/') != std::string::npos)
        failAt(context, directive,
               "\"index\" file name must not contain \"/\"");
    }
    settings.indexFiles = directive.args;
  } else if (directive.name == "autoindex") {
    if (directive.args[0] != "on" && directive.args[0] != "off")
      failAt(context, directive, "\"autoindex\" must be \"on\" or \"off\"");
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
