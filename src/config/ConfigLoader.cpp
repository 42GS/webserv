/**
 * @file  ConfigLoader.cpp
 * @brief Configuration loading: file, then the three stages.
 *        설정 읽기: 파일을 읽고 세 단계를 거친다.
 */
#include "config/ConfigLoader.hpp"

#include <fstream>
#include <sstream>

#include "config/ConfigBuilder.hpp"
#include "config/ConfigError.hpp"
#include "config/ConfigLexer.hpp"
#include "config/ConfigSyntax.hpp"
#include "fs/FileInfo.hpp"

std::vector<ServerConfig> loadConfigFile(const std::string& path) {
  // Checked first so that a missing path and a directory or device get
  // messages of their own.
  // 경로가 없을 때와 디렉토리·장치 파일일 때 각각의 메시지를 주려고 먼저
  // 확인한다.
  const FileKind kind = fileKind(path);
  if (kind == FILE_MISSING)
    throw ConfigError(path, 0, "cannot open configuration file");
  if (kind != FILE_REGULAR)
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
