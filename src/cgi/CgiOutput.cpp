/**
 * @file  CgiOutput.cpp
 * @brief The CGI response parser.
 *        CGI 응답 파서.
 *
 * Section numbers refer to RFC 3875; wiki concepts/cgi section 3 has the
 * rules with line references.
 * 절 번호는 RFC 3875의 것이다. 줄 번호가 있는 규칙은 위키 concepts/cgi
 * 3절에 있다.
 */
#include "cgi/CgiOutput.hpp"

#include <cstddef>

#include "http/Ascii.hpp"
#include "http/FieldSyntax.hpp"
#include "http/HeaderNames.hpp"
#include "http/StatusCodes.hpp"

namespace {

/** The CGI field for the status (section 6.3.3). / 상태 CGI 필드. */
const char* const kStatus = "status";
/**
 * @brief Fields the server sets or that change the framing.
 *        서버가 정하거나 framing을 바꾸는 필드.
 */
const char* const kDroppedFields[] = {"connection", "content-length", "date",
                                      "transfer-encoding"};
/** Number of entries in kDroppedFields. / kDroppedFields의 항목 수. */
const std::size_t kDroppedFieldCount =
    sizeof(kDroppedFields) / sizeof(kDroppedFields[0]);
/** DEL, the control character after "~". / "~" 다음의 제어 문자 DEL. */
const unsigned char kDeleteChar = 0x7f;
/**
 * @brief Lowest final status; 1xx are not final.
 *        최종 상태의 최솟값. 1xx는 최종이 아니다.
 */
const int kMinFinalStatus = 200;
/** Highest status a script may give. / 스크립트가 줄 수 있는 최대 상태. */
const int kMaxStatus = 599;

/**
 * @brief The CGI fields of a response, before classification.
 *        분류하기 전의 응답 CGI 필드.
 */
struct CgiFields {
  /** @brief No field seen. / 본 필드 없음. */
  CgiFields()
      : fieldCount(0),
        hasStatus(false),
        hasType(false),
        hasLocation(false),
        status(0) {}

  /** All header fields, CGI or not. / CGI 여부와 관계없는 모든 헤더 필드 수. */
  std::size_t fieldCount;
  /** A Status field was given. / Status 필드가 있었다. */
  bool hasStatus;
  /** A Content-Type field was given. / Content-Type 필드가 있었다. */
  bool hasType;
  /** A Location field was given. / Location 필드가 있었다. */
  bool hasLocation;
  /** Status code. / 상태 코드. */
  int status;
  /** Location value. / Location 값. */
  std::string location;
};

/**
 * @brief True for a field value without control characters but HTAB.
 *        HTAB 말고는 제어 문자가 없는 필드 값이면 true.
 *
 * A CR inside a value would split the response when sent over HTTP
 * (wiki concepts/security section 4).
 * 값 안의 CR은 HTTP로 보낼 때 응답을 나눌 수 있다 (위키 concepts/security
 * 4절).
 *
 * @param[in] value Field value. / 필드 값.
 * @return Whether it is safe. / 안전한지.
 */
bool isSafeValue(const std::string& value) {
  for (std::string::size_type i = 0; i < value.size(); ++i) {
    const unsigned char c = static_cast<unsigned char>(value[i]);
    if ((c < ' ' && c != '\t') || c == kDeleteChar) return false;
  }
  return true;
}

/**
 * @brief True for a field the server drops.
 *        서버가 버리는 필드이면 true.
 * @param[in] lowered Field name in lower case. / 소문자 필드 이름.
 * @return Whether it is dropped. / 버리는지.
 */
bool isDropped(const std::string& lowered) {
  for (std::size_t i = 0; i < kDroppedFieldCount; ++i) {
    if (lowered == kDroppedFields[i]) return true;
  }
  return false;
}

/**
 * @brief Parses "Status: 3digit [SP reason-phrase]" (section 6.3.3).
 *        "Status: 3digit [SP reason-phrase]"를 해석한다 (6.3.3절).
 *
 * The reason phrase is not used: the server sends its own.
 * reason phrase는 쓰지 않는다. 서버가 자기 것을 보낸다.
 *
 * @param[in]  value  Field value. / 필드 값.
 * @param[out] status Status code. / 상태 코드.
 * @return False unless it is a final status (200 to 599).
 *         최종 상태(200~599)가 아니면 false.
 */
bool parseStatus(const std::string& value, int& status) {
  unsigned long code = 0;
  if (value.size() > 3 && value[3] != ' ') return false;
  if (!parseDecimal(value.substr(0, 3), kMaxStatus, code) ||
      static_cast<int>(code) < kMinFinalStatus)
    return false;
  status = static_cast<int>(code);
  return true;
}

/**
 * @brief Applies one header line.
 *        헤더 줄 하나를 적용한다.
 *
 * Each CGI field may appear once (section 6.3); the others are kept for
 * the response unless dropped.
 * CGI 필드는 한 번씩만 올 수 있다 (6.3절). 다른 필드는 버리지 않으면
 * 응답에 옮긴다.
 *
 * @param[in]     line     The line, without its line end. / 줄 끝을 뺀 줄.
 * @param[in,out] fields   CGI fields so far. / 지금까지의 CGI 필드.
 * @param[in,out] response Response that gets the other fields.
 *                         다른 필드를 받는 응답.
 * @return False for a line that is not "name: value", a repeated CGI
 *         field or a bad Status.
 *         "name: value"가 아니거나, CGI 필드가 반복되거나, Status가
 *         잘못됐으면 false.
 */
bool applyLine(const std::string& line, CgiFields& fields, Response& response) {
  const std::string::size_type colon = line.find(':');
  if (colon == std::string::npos) return false;
  const std::string name = line.substr(0, colon);
  const std::string value = trimWhitespace(line.substr(colon + 1));
  if (!isToken(name) || !isSafeValue(value)) return false;

  ++fields.fieldCount;
  const std::string lowered = toAsciiLower(name);
  if (lowered == kStatus) {
    if (fields.hasStatus || !parseStatus(value, fields.status)) return false;
    fields.hasStatus = true;
    return true;
  }

  if (lowered == toAsciiLower(header_name::kLocation)) {
    if (fields.hasLocation) return false;
    fields.hasLocation = true;
    fields.location = value;
    return true;
  }

  if (lowered == toAsciiLower(header_name::kContentType)) {
    if (fields.hasType) return false;
    fields.hasType = true;
  }

  if (!isDropped(lowered)) response.headers.add(name, value);
  return true;
}

/**
 * @brief Reads the header lines up to the blank line.
 *        빈 줄까지 헤더 줄을 읽는다.
 * @param[in]  output   Script output. / 스크립트 출력.
 * @param[out] fields   CGI fields. / CGI 필드.
 * @param[out] response Gets the other fields. / 다른 필드를 받는다.
 * @return Offset of the body, or npos if the header is malformed or has
 *         no blank line.
 *         본문의 시작 위치. 헤더가 잘못됐거나 빈 줄이 없으면 npos.
 */
std::string::size_type readHeader(const std::string& output, CgiFields& fields,
                                  Response& response) {
  std::string::size_type start = 0;
  for (;;) {
    const std::string::size_type newline = output.find('\n', start);
    if (newline == std::string::npos) return std::string::npos;
    std::string line = output.substr(start, newline - start);
    if (!line.empty() && line[line.size() - 1] == '\r')
      line.erase(line.size() - 1);

    start = newline + 1;
    if (line.empty()) return start;
    if (!applyLine(line, fields, response)) return std::string::npos;
  }
}

/**
 * @brief True for an absolute URI: a scheme, then ":" (section 6.3.2).
 *        절대 URI이면 true: scheme 뒤에 ":" (6.3.2절).
 * @param[in] location Location value. / Location 값.
 * @return Whether it has a scheme. / scheme이 있는지.
 */
bool isAbsoluteUri(const std::string& location) {
  const std::string::size_type colon = location.find(':');
  if (colon == 0 || colon == std::string::npos) return false;
  return consistsOf(location.substr(0, colon), ascii::kAlphanumerics + "+-.");
}

/**
 * @brief True for a 3xx status: client redirection (RFC 9110 section
 *        15.4).
 *        3xx 상태이면 true: 클라이언트 리다이렉트 (RFC 9110 15.4절).
 * @param[in] status Status code. / 상태 코드.
 * @return Whether it redirects. / 리다이렉트인지.
 */
bool isRedirection(int status) { return status / 100 == 3; }

/**
 * @brief The output for Location fields (sections 6.2.2 to 6.2.4).
 *        Location 필드가 있는 출력 (6.2.2~6.2.4절).
 * @param[in] fields   CGI fields. / CGI 필드.
 * @param[in] response Other fields and body. / 다른 필드와 본문.
 * @return A local or client redirect, or INVALID.
 *         로컬이나 클라이언트 리다이렉트, 또는 INVALID.
 */
CgiOutput redirectOutput(const CgiFields& fields, const Response& response) {
  CgiOutput result;
  const bool alone = fields.fieldCount == 1 && response.body.empty();

  if (!fields.location.empty() && fields.location[0] == '/') {
    if (!alone) return result;
    result.kind = CgiOutput::LOCAL_REDIRECT;
    result.localTarget = fields.location;
    return result;
  }

  if (!isAbsoluteUri(fields.location)) return result;
  result.response = response;
  if (alone) {
    result.response.status = http_status::kFound;
  } else {
    // With a document: Status MUST be a redirect and Content-Type is
    // required (section 6.2.4).
    // 문서가 있으면 Status는 리다이렉트여야 하고 Content-Type이 필요하다
    // (6.2.4절).
    if (!fields.hasStatus || !fields.hasType || !isRedirection(fields.status))
      return result;
    result.response.status = fields.status;
  }

  result.response.headers.add(header_name::kLocation, fields.location);
  result.kind = CgiOutput::RESPONSE;
  return result;
}

/**
 * @brief Drops the body of a status that cannot have one.
 *        본문을 가질 수 없는 상태의 본문을 버린다.
 *
 * 204 and 304 responses end at the header section (RFC 9110 sections
 * 15.3.5 and 15.4.5); the server MUST adjust the output to the protocol
 * (section 6.2.1).
 * 204와 304 응답은 헤더 영역에서 끝난다 (RFC 9110 15.3.5절, 15.4.5절).
 * 서버는 출력을 프로토콜에 맞게 고쳐야 한다 (6.2.1절).
 *
 * @param[in,out] response The response. / 응답.
 */
void dropForbiddenBody(Response& response) {
  if (response.status == http_status::kNoContent ||
      response.status == http_status::kNotModified)
    response.body.clear();
}

}  // namespace

// Every path returns the same object, so that the body (up to
// limits::kMaxCgiOutputBytes) is not copied on return; an INVALID result
// may still hold a body, which the caller ignores.
// 모든 경로가 같은 객체를 돌려줘서, 본문(최대 limits::kMaxCgiOutputBytes)이
// 돌려줄 때 복사되지 않게 한다. INVALID 결과에 본문이 남아 있을 수
// 있지만 호출자는 무시한다.
CgiOutput parseCgiOutput(std::string& output) {
  CgiFields fields;
  CgiOutput result;
  const std::string::size_type body =
      readHeader(output, fields, result.response);
  if (body == std::string::npos || fields.fieldCount == 0) return result;

  output.erase(0, body);
  result.response.body.swap(output);

  if (fields.hasLocation) {
    result = redirectOutput(fields, result.response);
    return result;
  }

  // A document MUST have Content-Type (section 6.2.1).
  // 문서에는 Content-Type이 있어야 한다 (6.2.1절).
  if (!fields.hasType) return result;
  result.kind = CgiOutput::RESPONSE;
  result.response.status = fields.hasStatus ? fields.status : http_status::kOk;
  dropForbiddenBody(result.response);
  return result;
}
