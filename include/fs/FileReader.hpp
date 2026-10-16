/**
 * @file  FileReader.hpp
 * @brief Reading a whole small file.
 *        작은 파일 전체 읽기.
 */
#ifndef FS_FILEREADER_HPP
#define FS_FILEREADER_HPP

#include <cstddef>
#include <string>

/**
 * @brief Reads a whole regular file of at most @p maxSize bytes.
 *        @p maxSize 바이트 이하의 일반 파일 전체를 읽는다.
 *
 * Regular files need no poll() (raw/subject.md:134).
 * 일반 파일은 poll()이 필요 없다 (raw/subject.md:134).
 *
 * @param[in]  path    Path. / 경로.
 * @param[in]  maxSize Largest accepted size. / 받을 수 있는 최대 크기.
 * @param[out] content File content. / 파일 내용.
 * @return False if the file cannot be opened or read, or is larger.
 *         열거나 읽을 수 없거나, 더 크면 false.
 */
bool readWholeFile(const std::string& path, std::size_t maxSize,
                   std::string& content);

#endif
