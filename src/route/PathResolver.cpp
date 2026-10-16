#include "route/PathResolver.hpp"

#include <vector>

#include "http/Ascii.hpp"
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
  for (std::string::size_type i = 0; i < decodedPath.size(); ++i) {
    const unsigned char c = static_cast<unsigned char>(decodedPath[i]);
    if (isAsciiAlnum(static_cast<char>(c)) ||
        isOneOf(static_cast<char>(c), path_resolver::kPathSymbols)) {
      encoded += static_cast<char>(c);
    } else {
      encoded += '%';
      encoded += hex::kUpperDigits[c / hex::kBase];
      encoded += hex::kUpperDigits[c % hex::kBase];
    }
  }
  return encoded;
}
