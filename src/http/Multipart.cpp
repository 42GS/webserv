/**
 * @file  Multipart.cpp
 * @brief The multipart/form-data parser.
 *        multipart/form-data 파서.
 *
 * Grammar names in comments are those of RFC 2046 section 5.1.1.
 * 주석의 문법 이름은 RFC 2046 5.1.1절의 것이다.
 */
#include "http/Multipart.hpp"

#include <algorithm>
#include <map>

#include "http/Ascii.hpp"
#include "http/FieldSyntax.hpp"

namespace {

/** The media type handled. / 처리하는 미디어 타입. */
const char* const kFormDataType = "multipart/form-data";
/**
 * @brief Disposition type of every part (RFC 7578 section 4.2).
 *        모든 part의 disposition 타입 (RFC 7578 4.2절).
 */
const char* const kFormDataDisposition = "form-data";
/** The part header that names the field. / 필드 이름을 담는 part 헤더. */
const char* const kContentDisposition = "content-disposition";
/** bcharsnospace. */
const std::string kBoundaryChars = ascii::kAlphanumerics + "'()+_,-./:=?";
/** Longest boundary. / boundary의 최대 길이. */
const std::string::size_type kMaxBoundaryLength = 70;
/** Line end. / 줄 끝. */
const char* const kCrlf = "\r\n";
/** Header end: a line end and an empty line. / 헤더 끝: 줄 끝과 빈 줄. */
const char* const kBlankLine = "\r\n\r\n";
/**
 * @brief Starts the dash-boundary and ends the close-delimiter.
 *        dash-boundary의 시작이자 close-delimiter의 끝.
 */
const char* const kDashes = "--";

/**
 * @brief True if @p text has @p part at @p pos.
 *        @p text의 @p pos에 @p part가 있으면 true.
 * @param[in] text Text. / 텍스트.
 * @param[in] pos  Position, at most text.size(). / 위치. text.size() 이하.
 * @param[in] part Expected text. / 기대하는 텍스트.
 * @return Whether it is there. / 있는지.
 */
bool hasAt(const std::string& text, std::string::size_type pos,
           const std::string& part) {
  return text.compare(pos, part.size(), part) == 0;
}

/**
 * @brief Applies the Content-Disposition of a part.
 *        part의 Content-Disposition을 적용한다.
 * @param[in]     value Field value. / 필드 값.
 * @param[in,out] part  Gets name and filename. / name과 filename을 받는다.
 * @return False if it is not form-data with a name. / name이 있는
 * form-data가 아니면 false.
 */
bool applyDisposition(const std::string& value, FormPart& part) {
  std::map<std::string, std::string> parameters;
  if (valueBeforeParameters(value) != kFormDataDisposition ||
      !parseParameters(value, parameters) || parameters.count("name") == 0)
    return false;
  part.name = parameters["name"];
  part.hasFilename = parameters.count("filename") != 0;
  if (part.hasFilename) part.filename = parameters["filename"];
  return true;
}

/**
 * @brief Reads the header lines of a part.
 *        part의 헤더 줄을 읽는다.
 *
 * Headers other than Content-Disposition are ignored: RFC 7578 section
 * 4.8 says to ignore all but three, and of those Content-Type only
 * describes the content, which is stored as sent.
 * Content-Disposition 말고는 무시한다. RFC 7578 4.8절은 세 가지 말고는
 * 무시하라고 하고, 그중 Content-Type은 내용을 설명할 뿐이며 내용은 보낸
 * 그대로 저장한다.
 *
 * @param[in]  headers Lines separated by CRLF. / CRLF로 구분한 줄.
 * @param[out] part    The part. / part.
 * @return False for a line without ":", or no or two dispositions.
 *         ":"이 없는 줄이 있거나, disposition이 없거나 두 개이면 false.
 */
bool readPartHeaders(const std::string& headers, FormPart& part) {
  bool hasDisposition = false;
  std::string::size_type start = 0;
  while (start < headers.size()) {
    std::string::size_type end = headers.find(kCrlf, start);
    if (end == std::string::npos) end = headers.size();
    const std::string line = headers.substr(start, end - start);
    start = end + 2;
    const std::string::size_type colon = line.find(':');
    if (colon == std::string::npos) return false;
    if (toAsciiLower(line.substr(0, colon)) != kContentDisposition) continue;
    if (hasDisposition || !applyDisposition(line.substr(colon + 1), part))
      return false;
    hasDisposition = true;
  }
  return hasDisposition;
}

/**
 * @brief Reads the part between two delimiters.
 *        두 구분자 사이의 part를 읽는다.
 *
 * body-part := MIME-part-headers [CRLF *OCTET]: the headers end at an
 * empty line; without one there is no content. A part without headers
 * has no Content-Disposition, so it fails either way.
 * body-part := MIME-part-headers [CRLF *OCTET]: 헤더는 빈 줄에서 끝나고,
 * 빈 줄이 없으면 내용이 없다. 헤더가 없는 part는 Content-Disposition이
 * 없으므로 어느 쪽이든 실패한다.
 *
 * @param[in]  body  Request body. / 요청 본문.
 * @param[in]  start First octet after the delimiter line. / 구분자 줄 다음 첫
 * 옥텟.
 * @param[in]  end   Start of the next delimiter. / 다음 구분자의 시작.
 * @param[out] part  The part. / part.
 * @return False as for readPartHeaders(). / readPartHeaders()와 같은 경우에
 * false.
 */
bool readPart(const std::string& body, std::string::size_type start,
              std::string::size_type end, FormPart& part) {
  // Searched within the part only: a body of many parts without an empty
  // line would otherwise be scanned to its end once per part.
  // part 안에서만 찾는다. 그렇지 않으면 빈 줄 없는 part가 많은 본문을
  // part마다 끝까지 훑는다.
  const std::string blankLine(kBlankLine);
  const std::string::const_iterator partEnd = body.begin() + end;
  const std::string::const_iterator blank = std::search(
      body.begin() + start, partEnd, blankLine.begin(), blankLine.end());
  std::string::size_type headerEnd = end;
  std::string::size_type contentStart = end;
  if (blank != partEnd) {
    headerEnd = blank - body.begin();
    contentStart = headerEnd + blankLine.size();
  }
  part.contentStart = contentStart;
  part.contentLength = end - contentStart;
  return readPartHeaders(body.substr(start, headerEnd - start), part);
}

/**
 * @brief Moves past transport-padding and the CRLF ending a delimiter
 *        line.
 *        구분자 줄 끝의 transport-padding과 CRLF를 지나간다.
 *
 * Receivers MUST handle padding (RFC 2046 section 5.1.1).
 * 받는 쪽은 padding을 처리해야 한다 (RFC 2046 5.1.1절).
 *
 * @param[in]     body Request body. / 요청 본문.
 * @param[in,out] pos  Just after the boundary. / boundary 바로 뒤.
 * @return False if anything else follows the boundary.
 *         boundary 뒤에 다른 것이 있으면 false.
 */
bool skipDelimiterLineEnd(const std::string& body,
                          std::string::size_type& pos) {
  while (pos < body.size() && isWhitespace(body[pos])) ++pos;
  if (!hasAt(body, pos, kCrlf)) return false;
  pos += 2;
  return true;
}

}  // namespace

bool isFormData(const std::string& contentType) {
  return valueBeforeParameters(contentType) == kFormDataType;
}

bool formDataBoundary(const std::string& contentType, std::string& boundary) {
  std::map<std::string, std::string> parameters;
  if (!parseParameters(contentType, parameters) ||
      parameters.count("boundary") == 0)
    return false;
  const std::string& value = parameters["boundary"];
  if (value.empty() || value.size() > kMaxBoundaryLength ||
      value[value.size() - 1] == ' ' ||
      !consistsOf(value, kBoundaryChars + " "))
    return false;
  boundary = value;
  return true;
}

// multipart-body := [preamble CRLF] dash-boundary transport-padding CRLF
// body-part *encapsulation close-delimiter transport-padding
// [CRLF epilogue]. pos is just after a boundary at the top of each round.
// 매 회 시작에서 pos는 boundary 바로 뒤에 있다.
bool parseFormData(const std::string& body, const std::string& boundary,
                   std::vector<FormPart>& parts) {
  const std::string dashBoundary = kDashes + boundary;
  const std::string delimiter = kCrlf + dashBoundary;
  std::string::size_type pos = 0;
  if (hasAt(body, 0, dashBoundary)) {
    pos = dashBoundary.size();
  } else {
    pos = body.find(delimiter);
    if (pos == std::string::npos) return false;
    pos += delimiter.size();
  }
  for (;;) {
    if (hasAt(body, pos, kDashes)) return true;
    if (!skipDelimiterLineEnd(body, pos)) return false;
    const std::string::size_type next = body.find(delimiter, pos);
    if (next == std::string::npos) return false;
    FormPart part;
    if (!readPart(body, pos, next, part)) return false;
    parts.push_back(part);
    pos = next + delimiter.size();
  }
}
