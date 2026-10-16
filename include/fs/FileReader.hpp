#ifndef FS_FILEREADER_HPP
#define FS_FILEREADER_HPP

#include <cstddef>
#include <string>

// Reads a whole regular file of at most maxSize bytes. False if it cannot
// be opened or read, or is larger. Regular files need no poll()
// (raw/subject.md:134).
bool readWholeFile(const std::string& path, std::size_t maxSize,
                   std::string& content);

#endif
