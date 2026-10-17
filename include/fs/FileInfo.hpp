/**
 * @file  FileInfo.hpp
 * @brief What stat() and access() say about a path.
 *        stat()과 access()가 알려 주는 경로 정보.
 */
#ifndef FS_FILEINFO_HPP
#define FS_FILEINFO_HPP

#include <cstddef>
#include <string>

/**
 * @brief What stat() says about a path.
 *        stat()이 알려 주는 경로의 종류.
 *
 * Used by the configuration loader, the router and the handlers so that
 * the checks are written once.
 * 설정 loader, router, handler가 함께 써서 검사를 한 곳에만 둔다.
 */
enum FileKind {
  /** Missing or unreachable. / 없거나 닿을 수 없음. */
  FILE_MISSING,
  /** Regular file. / 일반 파일. */
  FILE_REGULAR,
  /** Directory. / 디렉토리. */
  FILE_DIRECTORY,
  /** Anything else (device, FIFO, ...). / 그 밖의 것 (장치, FIFO 등). */
  FILE_OTHER
};

/**
 * @brief Kind of the file at @p path.
 *        @p path에 있는 파일의 종류.
 * @param[in] path Path. / 경로.
 * @return The kind; FILE_MISSING also covers paths that cannot be reached
 *         (errno is not read).
 *         종류. 닿을 수 없는 경로도 FILE_MISSING이다 (errno는 읽지 않는다).
 */
FileKind fileKind(const std::string& path);

/**
 * @brief access(R_OK): the server may open the file for reading.
 *        access(R_OK): 서버가 이 파일을 읽으려고 열 수 있는지.
 * @param[in] path Path. / 경로.
 * @return Whether it is readable. / 읽을 수 있는지.
 */
bool isReadable(const std::string& path);

/**
 * @brief access(W_OK | X_OK): the server may add and remove entries in
 *        the directory.
 *        access(W_OK | X_OK): 서버가 이 디렉토리에 항목을 만들고 지울 수
 *        있는지.
 * @param[in] path Directory path. / 디렉토리 경로.
 * @return Whether entries can be changed. / 항목을 바꿀 수 있는지.
 */
bool canChangeEntries(const std::string& path);

/**
 * @brief Size of a regular file.
 *        일반 파일의 크기.
 * @param[in]  path Path. / 경로.
 * @param[out] size Size in bytes; set only on success.
 *                  바이트 단위 크기. 성공했을 때만 쓴다.
 * @return False if the path is not a regular file.
 *         일반 파일이 아니면 false.
 */
bool regularFileSize(const std::string& path, std::size_t& size);

#endif
