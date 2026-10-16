#include "http/MimeTypes.hpp"

#include <cstddef>

#include "http/Ascii.hpp"

namespace {

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
  for (std::size_t i = 0; i < mime_types::kMimeTypeCount; ++i) {
    if (extension == mime_types::kMimeTypes[i].extension)
      return mime_types::kMimeTypes[i].type;
  }
  return mime_types::kDefaultType;
}
