/**
 * @file  FilePath.hpp
 * @brief Splitting a file system path.
 *        파일 시스템 경로 나누기.
 */
#ifndef FS_FILEPATH_HPP
#define FS_FILEPATH_HPP

#include <string>

/**
 * @brief The directory that holds a path.
 *        경로가 들어 있는 디렉토리.
 * @param[in] path File system path. / 파일 시스템 경로.
 * @return Text before the last "/", "/" for an entry at the top, or "."
 *         without a "/".
 *         마지막 "/" 앞의 텍스트. 맨 위의 항목이면 "/", "/"가 없으면 ".".
 */
std::string parentDirectory(const std::string& path);

/**
 * @brief The last component of a path.
 *        경로의 마지막 요소.
 * @param[in] path File system path. / 파일 시스템 경로.
 * @return Text after the last "/", or the whole path without one.
 *         마지막 "/" 뒤의 텍스트. "/"가 없으면 경로 전체.
 */
std::string baseName(const std::string& path);

#endif
