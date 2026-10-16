#ifndef FS_FILEINFO_HPP
#define FS_FILEINFO_HPP

#include <cstddef>
#include <string>

// What stat() says about a path. Used by the configuration loader, the
// router and the handlers so that the checks are written once.
enum FileKind { FILE_MISSING, FILE_REGULAR, FILE_DIRECTORY, FILE_OTHER };

// FILE_MISSING also covers paths that cannot be reached; errno is not read.
FileKind fileKind(const std::string& path);

// access(R_OK): the server may open the file for reading.
bool isReadable(const std::string& path);

// Size of a regular file; false if the path is not one.
bool regularFileSize(const std::string& path, std::size_t& size);

#endif
