#include "http/MimeTypes.hpp"

#include <cstddef>

#include "http/Ascii.hpp"

namespace {

// One row of the extension table.
struct MimeEntry {
  const char* extension;
  const char* type;
};

const MimeEntry kMimeTypes[] = {
    {"html", "text/html"},        {"htm", "text/html"},
    {"css", "text/css"},          {"js", "text/javascript"},
    {"json", "application/json"}, {"txt", "text/plain"},
    {"png", "image/png"},         {"jpg", "image/jpeg"},
    {"jpeg", "image/jpeg"},       {"gif", "image/gif"},
    {"svg", "image/svg+xml"},     {"ico", "image/x-icon"},
    {"pdf", "application/pdf"},
};

const std::size_t kMimeTypeCount = sizeof(kMimeTypes) / sizeof(kMimeTypes[0]);

const char* const kDefaultType = "application/octet-stream";

std::string lowerExtension(const std::string& fileName) {
  const std::string::size_type slash = fileName.rfind('/');
  const std::string::size_type dot = fileName.rfind('.');
  if (dot == std::string::npos || (slash != std::string::npos && dot < slash))
    return "";
  std::string extension = fileName.substr(dot + 1);
  for (std::string::size_type i = 0; i < extension.size(); ++i)
    extension[i] = toAsciiLower(extension[i]);
  return extension;
}

}  // namespace

std::string mimeTypeFor(const std::string& fileName) {
  const std::string extension = lowerExtension(fileName);
  for (std::size_t i = 0; i < kMimeTypeCount; ++i) {
    if (extension == kMimeTypes[i].extension) return kMimeTypes[i].type;
  }
  return kDefaultType;
}
