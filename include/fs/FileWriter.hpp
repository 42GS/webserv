/**
 * @file  FileWriter.hpp
 * @brief Writing a new file in one go.
 *        새 파일을 한 번에 쓰기.
 */
#ifndef FS_FILEWRITER_HPP
#define FS_FILEWRITER_HPP

#include <cstddef>
#include <string>

/**
 * @brief Creates a file that must not exist yet and writes @p size bytes.
 *        아직 없어야 하는 파일을 만들고 @p size 바이트를 쓴다.
 *
 * O_EXCL makes the check for an existing file and the creation one step,
 * so a file another process made after the caller looked is not
 * overwritten. Regular files need no poll() (raw/subject.md:134).
 * O_EXCL로 기존 파일 검사와 생성을 한 단계로 해서, 호출자가 확인한 뒤에
 * 다른 프로세스가 만든 파일을 덮어쓰지 않는다. 일반 파일은 poll()이
 * 필요 없다 (raw/subject.md:134).
 *
 * @param[in] path Path. / 경로.
 * @param[in] data Bytes to write. / 쓸 바이트.
 * @param[in] size Number of bytes. / 바이트 수.
 * @return False if the file exists or cannot be created, written or
 *         closed. A file this call created is removed again, and an
 *         existing one is left alone.
 *         파일이 이미 있거나, 만들거나 쓰거나 닫을 수 없으면 false. 이
 *         호출이 만든 파일은 다시 지우고, 원래 있던 파일은 건드리지 않는다.
 */
bool writeNewFile(const std::string& path, const char* data, std::size_t size);

#endif
