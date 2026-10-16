#include "config/ConfigLoader.hpp"

#include <sys/stat.h>

#include <fstream>
#include <sstream>

#include "config/ConfigBuilder.hpp"
#include "config/ConfigError.hpp"
#include "config/ConfigLexer.hpp"
#include "config/ConfigSyntax.hpp"

std::vector<ServerConfig> loadConfigFile(const std::string& path) {
  struct stat info;
  if (stat(path.c_str(), &info) != 0)
    throw ConfigError(path, 0, "cannot open configuration file");
  if (!S_ISREG(info.st_mode))
    throw ConfigError(path, 0, "configuration file is not a regular file");
  std::ifstream file(path.c_str());
  if (!file) throw ConfigError(path, 0, "cannot open configuration file");
  std::ostringstream content;
  content << file.rdbuf();
  if (file.bad()) throw ConfigError(path, 0, "cannot read configuration file");
  return loadConfigText(content.str(), path);
}

std::vector<ServerConfig> loadConfigText(const std::string& text,
                                         const std::string& source) {
  const std::vector<Token> tokens = tokenizeConfig(text, source);
  const DirectiveTree tree = parseDirectives(tokens, source);
  return buildServerConfigs(tree, source);
}
