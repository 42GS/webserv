#ifndef HTTP_MIMETYPES_HPP
#define HTTP_MIMETYPES_HPP

#include <cstddef>
#include <string>

// One row of the extension table.
struct MimeEntry {
  const char* extension;
  const char* type;
};

namespace mime_types {

const MimeEntry kMimeTypes[] = {
    {"html", "text/html"},        {"htm", "text/html"},
    {"css", "text/css"},          {"js", "text/javascript"},
    {"json", "application/json"}, {"txt", "text/plain"},
    {"png", "image/png"},         {"jpg", "image/jpeg"},
    {"jpeg", "image/jpeg"},       {"gif", "image/gif"},
    {"svg", "image/svg+xml"},     {"ico", "image/x-icon"},
    {"pdf", "application/pdf"},
};

const char* const kDefaultType = "application/octet-stream";

const std::size_t kMimeTypeCount = sizeof(kMimeTypes) / sizeof(kMimeTypes[0]);

}  // namespace mime_types

// Media type for a file name by its extension (case-insensitive, as NGINX
// "types"). Unknown extensions get application/octet-stream (design 7).
std::string mimeTypeFor(const std::string& fileName);

#endif
