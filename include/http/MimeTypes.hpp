#ifndef HTTP_MIMETYPES_HPP
#define HTTP_MIMETYPES_HPP

#include <string>

// Media type for a file name by its extension (case-insensitive, as NGINX
// "types"). Unknown extensions get application/octet-stream (design 7).
std::string mimeTypeFor(const std::string& fileName);

#endif
