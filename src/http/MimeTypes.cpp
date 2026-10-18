/**
 * @file  MimeTypes.cpp
 * @brief The extension to media type table.
 *        확장자 → 미디어 타입 표.
 */
#include "http/MimeTypes.hpp"

#include <cstddef>

#include "http/Ascii.hpp"

namespace {

/**
 * @brief One row of the extension table.
 *        확장자 표의 한 행.
 */
struct MimeEntry {
  /** Lowercase extension without ".". / "."을 뺀 소문자 확장자. */
  const char* extension;
  /** Media type. / 미디어 타입. */
  const char* type;
};

/** Known extensions. / 아는 확장자. */
const MimeEntry kMimeTypes[] = {
    {"html", "text/html"},        {"htm", "text/html"},
    {"css", "text/css"},          {"js", "text/javascript"},
    {"json", "application/json"}, {"txt", mime_types::kTextType},
    {"png", "image/png"},         {"jpg", "image/jpeg"},
    {"jpeg", "image/jpeg"},       {"gif", "image/gif"},
    {"svg", "image/svg+xml"},     {"ico", "image/x-icon"},
    {"pdf", "application/pdf"},
};

/** Number of rows in kMimeTypes. / kMimeTypes의 행 수. */
const std::size_t kMimeTypeCount = sizeof(kMimeTypes) / sizeof(kMimeTypes[0]);

/** Type of unknown extensions (design 7). / 모르는 확장자의 타입 (설계 7절). */
const char* const kDefaultType = "application/octet-stream";

/**
 * @brief Lowercased extension of the last path segment.
 *        마지막 경로 조각의 확장자를 소문자로 돌려준다.
 * @param[in] fileName File name or path. / 파일 이름이나 경로.
 * @return Text after the last ".", or "" if the last segment has none.
 *         마지막 "." 뒤의 텍스트. 마지막 조각에 "."이 없으면 "".
 */
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
