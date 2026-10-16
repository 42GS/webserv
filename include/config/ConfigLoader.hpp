/**
 * @file  ConfigLoader.hpp
 * @brief Entry point of configuration loading.
 *        설정 읽기의 진입점.
 *
 * Loading runs three stages: tokenizeConfig(), parseDirectives(),
 * buildServerConfigs().
 * 읽기는 세 단계로 진행한다: tokenizeConfig(), parseDirectives(),
 * buildServerConfigs().
 */
#ifndef CONFIG_CONFIGLOADER_HPP
#define CONFIG_CONFIGLOADER_HPP

#include <string>
#include <vector>

#include "config/ServerConfig.hpp"

/**
 * @brief Reads and validates a configuration file.
 *        설정 파일을 읽고 검증한다.
 * @param[in] path Path of the file. / 파일 경로.
 * @return One ServerConfig per "server" block.
 *         "server" 블록마다 ServerConfig 하나.
 * @throws ConfigError If the file is missing, not a regular file,
 *         unreadable, or invalid.
 *         파일이 없거나, 일반 파일이 아니거나, 읽을 수 없거나, 내용이
 *         잘못됐으면.
 */
std::vector<ServerConfig> loadConfigFile(const std::string& path);

/**
 * @brief Same as loadConfigFile(), for text already in memory.
 *        loadConfigFile()과 같지만 이미 메모리에 있는 텍스트를 읽는다.
 * @param[in] text   Configuration text. / 설정 텍스트.
 * @param[in] source Name of the text in error messages.
 *                   오류 메시지에서 텍스트를 가리킬 이름.
 * @return One ServerConfig per "server" block.
 *         "server" 블록마다 ServerConfig 하나.
 * @throws ConfigError If the text is invalid. / 텍스트가 잘못됐으면.
 */
std::vector<ServerConfig> loadConfigText(const std::string& text,
                                         const std::string& source);

#endif
