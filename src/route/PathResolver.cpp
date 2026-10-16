#include "route/PathResolver.hpp"

#include <cctype>
#include <cstring>
#include <vector>

namespace {

const int kHexBase = 16;
const int kHexLetterValue = 10;  // value of 'a' / 'A'
const int kNotHex = -1;

int hexValue(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + kHexLetterValue;
  if (c >= 'A' && c <= 'F') return c - 'A' + kHexLetterValue;
  return kNotHex;
}

// Decodes %XX once. Fails on a malformed escape, NUL, or "/".
bool decodeSegment(const std::string& segment, std::string& decoded) {
  decoded.clear();
  for (std::string::size_type i = 0; i < segment.size(); ++i) {
    if (segment[i] != '%') {
      decoded += segment[i];
      continue;
    }
    if (i + 2 >= segment.size()) return false;  // needs two hex digits
    const int high = hexValue(segment[i + 1]);
    const int low = hexValue(segment[i + 2]);
    if (high == kNotHex || low == kNotHex) return false;
    const char c = static_cast<char>(high * kHexBase + low);
    if (c == '\0' || c == '/') return false;
    decoded += c;
    i += 2;
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
  static const char kHexDigits[] = "0123456789ABCDEF";
  std::string encoded;
  for (std::string::size_type i = 0; i < decodedPath.size(); ++i) {
    const unsigned char c = static_cast<unsigned char>(decodedPath[i]);
    // strchr() would also find the terminating NUL, so c == 0 is excluded.
    if (std::isalnum(c) ||
        (c != 0 && std::strchr("-._~!$&'()*+,;=:@/", c) != 0)) {
      encoded += static_cast<char>(c);
    } else {
      encoded += '%';
      encoded += kHexDigits[c / kHexBase];
      encoded += kHexDigits[c % kHexBase];
    }
  }
  return encoded;
}
