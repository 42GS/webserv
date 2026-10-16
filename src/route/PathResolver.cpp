#include "route/PathResolver.hpp"

#include <vector>

#include "http/Hex.hpp"

namespace {

// Decodes %XX once. Fails on a malformed escape, NUL, or "/".
bool decodeSegment(const std::string& segment, std::string& decoded) {
  decoded.clear();
  for (std::string::size_type i = 0; i < segment.size(); ++i) {
    if (segment[i] != '%') {
      decoded += segment[i];
      continue;
    }
    if (i + path_resolver::kEscapeDigits >= segment.size()) return false;
    const int high = hexDigitValue(segment[i + 1]);
    const int low = hexDigitValue(segment[i + path_resolver::kEscapeDigits]);
    if (high == hex::kNotHex || low == hex::kNotHex) return false;
    const char c = static_cast<char>(high * hex::kBase + low);
    if (c == '\0' || c == '/') return false;
    decoded += c;
    i += path_resolver::kEscapeDigits;
  }
  return true;
}

}  // namespace

bool normalizePath(const std::string& encodedPath, std::string& normalized) {
  std::vector<std::string> kept;
  bool endsAsDirectory = false;
  std::string::size_type start = 0;
  while (start <= encodedPath.size()) {
    std::string::size_type slash = encodedPath.find('/', start);
    if (slash == std::string::npos) slash = encodedPath.size();
    std::string segment;
    if (!decodeSegment(encodedPath.substr(start, slash - start), segment))
      return false;
    endsAsDirectory = segment.empty() || segment == "." || segment == "..";
    if (segment == "..") {
      if (kept.empty()) return false;
      kept.pop_back();
    } else if (!segment.empty() && segment != ".") {
      kept.push_back(segment);
    }
    start = slash + 1;
  }
  normalized = "/";
  for (std::size_t i = 0; i < kept.size(); ++i) {
    normalized += kept[i];
    if (i + 1 < kept.size() || endsAsDirectory) normalized += '/';
  }
  return true;
}

std::string encodePath(const std::string& decodedPath) {
  std::string encoded;
  std::string::size_type start = 0;
  while (start < decodedPath.size()) {
    // Copy the run of characters that need no escape, then escape one.
    const std::string::size_type special =
        decodedPath.find_first_not_of(path_resolver::kPathChars, start);
    encoded.append(decodedPath, start,
                   special == std::string::npos ? special : special - start);
    if (special == std::string::npos) break;
    const unsigned char c = static_cast<unsigned char>(decodedPath[special]);
    encoded += '%';
    encoded += hex::kUpperDigits[c / hex::kBase];
    encoded += hex::kUpperDigits[c % hex::kBase];
    start = special + 1;
  }
  return encoded;
}
