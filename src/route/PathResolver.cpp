/**
 * @file  PathResolver.cpp
 * @brief Path decoding, normalization and encoding.
 *        경로 디코딩, 정규화, 인코딩.
 */
#include "route/PathResolver.hpp"

#include <vector>

#include "http/Ascii.hpp"
#include "http/Hex.hpp"

namespace {

/**
 * @brief Characters encodePath() leaves as they are.
 *        encodePath()가 그대로 두는 문자.
 *
 * Letters, digits, the rest of unreserved and sub-delims, ":", "@"
 * (pchar) and "/" (RFC 3986 section 3.3).
 * 글자, 숫자, 나머지 unreserved와 sub-delims, ":", "@" (pchar), "/"
 * (RFC 3986 3.3절).
 */
const std::string kPathChars = ascii::kAlphanumerics + "-._~!$&'()*+,;=:@/";

/**
 * @brief Decodes %XX in one path segment, once.
 *        경로 조각 하나의 %XX를 한 번 디코딩한다.
 * @param[in]  segment Encoded segment. / 인코딩된 조각.
 * @param[out] decoded Decoded segment. / 디코딩한 조각.
 * @return False for a malformed escape or one that decodes to NUL or "/".
 *         escape가 잘못됐거나 NUL이나 "/"로 디코딩되면 false.
 */
bool decodeSegment(const std::string& segment, std::string& decoded) {
  decoded.clear();
  for (std::string::size_type i = 0; i < segment.size(); ++i) {
    if (segment[i] != '%') {
      decoded += segment[i];
      continue;
    }
    // "%" and two hex digits. / "%"와 16진 숫자 두 개.
    if (i + 2 >= segment.size()) return false;
    const int high = hexDigitValue(segment[i + 1]);
    const int low = hexDigitValue(segment[i + 2]);
    if (high == hex::kNotHex || low == hex::kNotHex) return false;
    const char c = static_cast<char>(high * hex::kBase + low);
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
  std::string encoded;
  std::string::size_type start = 0;
  while (start < decodedPath.size()) {
    // Copy the run of characters that need no escape, then escape one.
    // 인코딩이 필요 없는 문자들은 한 번에 복사하고, 그다음 한 글자를
    // 인코딩한다.
    const std::string::size_type special =
        decodedPath.find_first_not_of(kPathChars, start);
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
