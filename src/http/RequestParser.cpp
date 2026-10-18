/**
 * @file  RequestParser.cpp
 * @brief The request parser's states and grammar checks.
 *        요청 파서의 상태와 문법 검사.
 *
 * Section numbers in comments refer to RFC 9112 unless noted otherwise.
 * The choices where the RFC allows several behaviours are listed in
 * analysis/design section 6 of the wiki.
 * 주석의 절 번호는 따로 적지 않으면 RFC 9112의 것이다. RFC가 여러
 * 동작을 허용하는 곳에서 고른 것은 위키 analysis/design 6절에 있다.
 */
#include "http/RequestParser.hpp"

#include <limits>
#include <vector>

#include "core/Limits.hpp"
#include "http/Ascii.hpp"
#include "http/FieldSyntax.hpp"
#include "http/HeaderNames.hpp"
#include "http/Hex.hpp"
#include "http/Methods.hpp"
#include "http/StatusCodes.hpp"

namespace {

/**
 * @brief Host characters: unreserved, pct-encoded, sub-delims, ":" and
 *        IP-literal brackets (RFC 3986 section 3.2.2).
 *        Host 문자: unreserved, pct-encoded, sub-delims, ":", IP-literal
 *        괄호 (RFC 3986 3.2.2절).
 */
const std::string kHostChars = ascii::kAlphanumerics + "-._~%!$&'()*+,;=:[]";
/**
 * @brief Prefix of the absolute-form, any case.
 *        absolute-form 접두어, 대소문자 무관.
 */
const char* const kAbsoluteFormPrefix = "http://";
/** The only transfer coding implemented. / 구현한 유일한 transfer coding. */
const char* const kChunked = "chunked";
/** DEL, the control character after "~". / "~" 다음의 제어 문자 DEL. */
const unsigned char kDeleteChar = 0x7f;

/** Largest size_t. / size_t의 최댓값. */
const std::size_t kMaxSize = std::numeric_limits<std::size_t>::max();
/**
 * @brief The largest Content-Length that fits both parseDecimal() and
 *        size_t.
 *        parseDecimal()과 size_t 모두에 들어가는 가장 큰 Content-Length.
 */
const unsigned long kMaxContentLength =
    std::numeric_limits<unsigned long>::max() < kMaxSize
        ? std::numeric_limits<unsigned long>::max()
        : static_cast<unsigned long>(kMaxSize);

/**
 * @brief True for HTTP-version = "HTTP/" DIGIT "." DIGIT, case-sensitive
 *        (section 2.3).
 *        HTTP-version = "HTTP/" DIGIT "." DIGIT이면 true, 대소문자 구분
 *        (2.3절).
 * @param[in] version Third part of the request line. / 요청 줄의 셋째 부분.
 * @return Whether it matches. / 문법에 맞는지.
 */
bool isHttpVersion(const std::string& version) {
  return version.size() == 8 && version.compare(0, 5, "HTTP/") == 0 &&
         isAsciiDigit(version[5]) && version[6] == '.' &&
         isAsciiDigit(version[7]);
}

/**
 * @brief True for a non-empty request-target of visible ASCII.
 *        보이는 ASCII로 된, 비어 있지 않은 request-target이면 true.
 *
 * No whitespace, control or non-ASCII octet, and no fragment ("#"), which
 * is not part of a request-target (section 3.2).
 * 공백, 제어 문자, 비 ASCII 옥텟이 없어야 하고, request-target에 속하지
 * 않는 fragment("#")도 없어야 한다 (3.2절).
 *
 * @param[in] target Second part of the request line. / 요청 줄의 둘째 부분.
 * @return Whether it is acceptable. / 받을 수 있는지.
 */
bool isTargetText(const std::string& target) {
  for (std::string::size_type i = 0; i < target.size(); ++i) {
    const unsigned char c = static_cast<unsigned char>(target[i]);
    if (c <= ' ' || c >= kDeleteChar || c == '#') return false;
  }
  return !target.empty();
}

/**
 * @brief True if a field value holds only visible octets, SP and HTAB
 *        (RFC 9110 section 5.5).
 *        필드 값이 보이는 옥텟, SP, HTAB만 담고 있으면 true
 *        (RFC 9110 5.5절).
 * @param[in] value Field value. / 필드 값.
 * @return Whether it is acceptable. / 받을 수 있는지.
 */
bool isFieldValue(const std::string& value) {
  for (std::string::size_type i = 0; i < value.size(); ++i) {
    const unsigned char c = static_cast<unsigned char>(value[i]);
    if ((c < ' ' && c != '\t') || c == kDeleteChar) return false;
  }
  return true;
}

/**
 * @brief Checks Host = uri-host [ ":" port ] (RFC 9110 section 7.2).
 *        Host = uri-host [ ":" port ]를 검사한다 (RFC 9110 7.2절).
 *
 * Only the character set is checked; an empty value is allowed by the
 * grammar.
 * 문자 집합만 검사한다. 빈 값은 문법이 허용한다.
 *
 * @param[in] value Host field value. / Host 필드 값.
 * @return Whether every character is allowed. / 모든 문자가 허용되는지.
 */
bool isHostValue(const std::string& value) {
  return consistsOf(value, kHostChars);
}

/**
 * @brief Appends one digit to @p value in base @p base.
 *        @p value 뒤에 @p base 진법 숫자 하나를 붙인다.
 * @param[in,out] value Number so far. / 지금까지의 수.
 * @param[in]     base  Radix. / 기수.
 * @param[in]     digit Digit value, 0 .. base - 1. / 숫자 값, 0 .. base - 1.
 * @return False, leaving value unchanged, if the result would not fit.
 *         결과가 size_t에 들어가지 않으면 false. 이때 value는 그대로다.
 */
bool appendDigit(std::size_t& value, int base, int digit) {
  const std::size_t factor = static_cast<std::size_t>(base);
  const std::size_t add = static_cast<std::size_t>(digit);
  if (value > (kMaxSize - add) / factor) return false;
  value = value * factor + add;
  return true;
}

/**
 * @brief Status for a request line that does not end within the limit.
 *        상한 안에 끝나지 않은 요청 줄의 상태 코드.
 *
 * A method longer than any known one gets 501 (section 3), anything else
 * 414.
 * 아는 어떤 메서드보다 긴 메서드면 501 (3절), 그 밖에는 414.
 *
 * @param[in] text The request line so far. / 지금까지의 요청 줄.
 * @return 501 or 414. / 501 또는 414.
 */
int overlongRequestLineStatus(const std::string& text) {
  const std::string::size_type space = text.find(' ');
  const std::string::size_type methodLength =
      (space == std::string::npos) ? text.size() : space;
  return methodLength > longestImplementedMethod()
             ? http_status::kNotImplemented
             : http_status::kUriTooLong;
}

/**
 * @brief Whether the connection persists after this request (RFC 9112
 *        section 9.3).
 *        이 요청 뒤에 연결을 유지하는지 (RFC 9112 9.3절).
 *
 * HTTP/1.1 persists unless the "close" option is listed; connection
 * options are case-insensitive (RFC 9110 section 7.6.1). HTTP/1.0 always
 * closes: section 9.3 leaves its "keep-alive" to the recipient, and this
 * server does not take it up.
 * HTTP/1.1은 "close" 옵션이 없으면 유지한다. 연결 옵션은 대소문자를 구분하지
 * 않는다 (RFC 9110 7.6.1절). HTTP/1.0은 항상 닫는다. 9.3절은 1.0의
 * "keep-alive"를 받아들일지 수신자에게 맡기고, 이 서버는 받아들이지 않는다.
 *
 * @param[in] request A request with its head complete. / 머리가 끝난 요청.
 * @return True to keep the connection. / 연결을 유지하면 true.
 */
bool keepsConnection(const Request& request) {
  if (request.version != request_parser::kHttp11) return false;

  const std::vector<std::string> options =
      splitList(request.headers.all(header_name::kConnection));
  for (std::size_t i = 0; i < options.size(); ++i) {
    if (toAsciiLower(options[i]) == header_value::kClose) return false;
  }
  return true;
}

}  // namespace

RequestParser::RequestParser() : consumed_(0) { reset(); }

void RequestParser::reset() {
  maxBodySize_ = 0;
  state_ = REQUEST_LINE;
  request_ = Request();
  errorStatus_ = 0;
  started_ = false;
  preambleBytes_ = 0;
  headerBytes_ = 0;
  chunked_ = false;
  contentLength_ = 0;
  remaining_ = 0;
}

RequestParser::Result RequestParser::feed(std::string& buffer) {
  if (!buffer.empty()) started_ = true;
  consumed_ = 0;
  while (step(buffer)) {
  }

  buffer.erase(0, consumed_);
  consumed_ = 0;
  return resultOfState();
}

void RequestParser::setMaxBodySize(std::size_t maxBodySize) {
  maxBodySize_ = maxBodySize;
}

int RequestParser::errorStatus() const { return errorStatus_; }

bool RequestParser::hasStarted() const { return started_; }

const Request& RequestParser::request() const { return request_; }

bool RequestParser::step(std::string& buffer) {
  switch (state_) {
    case REQUEST_LINE:
      return readRequestLine(buffer);
    case HEADERS:
      return readHeaderLine(buffer);
    case BODY_START:
      return startBody();
    case BODY_LENGTH:
      return readBodyBytes(buffer, COMPLETE);
    case CHUNK_SIZE:
      return readChunkLine(buffer);
    case CHUNK_DATA:
      return readBodyBytes(buffer, CHUNK_DATA_END);
    case CHUNK_DATA_END:
      return readChunkEnd(buffer);
    case TRAILERS:
      return readTrailerLine(buffer);
    case COMPLETE:
    case FAILED:
      break;
  }
  return false;
}

RequestParser::Result RequestParser::resultOfState() const {
  switch (state_) {
    // The head is complete; the caller sets the body limit.
    // 머리가 끝났다. 호출자가 본문 상한을 정한다.
    case BODY_START:
      return HEAD_DONE;
    case COMPLETE:
      return DONE;
    case FAILED:
      return ERROR;
    default:
      return NEED_MORE;
  }
}

bool RequestParser::fail(int status) {
  state_ = FAILED;
  errorStatus_ = status;
  return false;
}

RequestParser::LineStatus RequestParser::takeLine(const std::string& buffer,
                                                  std::string& line) {
  const std::string::size_type newline = buffer.find('\n', consumed_);
  if (newline == std::string::npos) return LINE_INCOMPLETE;
  line.assign(buffer, consumed_, newline - consumed_);
  consumed_ = newline + 1;
  if (!line.empty() && line[line.size() - 1] == '\r')
    line.erase(line.size() - 1);
  return line.find('\r') == std::string::npos ? LINE_READY : LINE_INVALID;
}

bool RequestParser::takeHeaderLine(const std::string& buffer,
                                   std::string& line) {
  const LineStatus status = takeLine(buffer, line);
  if (status == LINE_INCOMPLETE) {
    if (headerBytes_ + unread(buffer) > limits::kMaxHeaderBytes)
      return fail(http_status::kRequestHeaderFieldsTooLarge);
    return false;
  }

  if (status == LINE_INVALID) return fail(http_status::kBadRequest);

  headerBytes_ += line.size() + 1;
  if (headerBytes_ > limits::kMaxHeaderBytes)
    return fail(http_status::kRequestHeaderFieldsTooLarge);
  return true;
}

std::size_t RequestParser::unread(const std::string& buffer) const {
  return buffer.size() - consumed_;
}

bool RequestParser::readRequestLine(std::string& buffer) {
  std::string line;
  const LineStatus status = takeLine(buffer, line);

  if (status == LINE_INCOMPLETE) {
    if (unread(buffer) > limits::kMaxRequestLine)
      return fail(overlongRequestLineStatus(buffer.substr(consumed_)));
    return false;
  }

  if (status == LINE_INVALID) return fail(http_status::kBadRequest);

  if (line.empty()) {
    // Empty lines before the request line are ignored (section 2.2), but
    // only as many as would fit in a request line.
    // 요청 줄 앞의 빈 줄은 무시하지만 (2.2절), 요청 줄 하나에 들어갈
    // 만큼만 받는다.
    preambleBytes_ += 1;
    if (preambleBytes_ > limits::kMaxRequestLine)
      return fail(http_status::kBadRequest);
    return true;
  }

  if (line.size() > limits::kMaxRequestLine)
    return fail(overlongRequestLineStatus(line));
  if (!parseRequestLine(line)) return false;
  state_ = HEADERS;
  return true;
}

bool RequestParser::readHeaderLine(std::string& buffer) {
  std::string line;
  if (!takeHeaderLine(buffer, line)) return false;
  if (line.empty()) return finishHead();

  std::string name;
  std::string value;
  if (!parseFieldLine(line, name, value)) return false;
  request_.headers.add(name, value);
  return true;
}

bool RequestParser::startBody() {
  if (chunked_) {
    state_ = CHUNK_SIZE;
    return true;
  }

  if (contentLength_ > maxBodySize_) return fail(http_status::kContentTooLarge);
  if (contentLength_ == 0) {
    state_ = COMPLETE;
    return true;
  }

  // Known size: reserve once instead of growing (design decision 11).
  // 크기를 아니까 늘려 가지 않고 한 번에 확보한다 (설계 결정 11).
  request_.body.reserve(contentLength_);
  remaining_ = contentLength_;
  state_ = BODY_LENGTH;
  return true;
}

bool RequestParser::readBodyBytes(std::string& buffer, State whenDone) {
  const std::size_t available = unread(buffer);
  if (available == 0) return false;
  const std::size_t taken = (available < remaining_) ? available : remaining_;
  request_.body.append(buffer, consumed_, taken);
  consumed_ += taken;
  remaining_ -= taken;
  if (remaining_ == 0) state_ = whenDone;
  return true;
}

bool RequestParser::readChunkLine(std::string& buffer) {
  std::string line;
  const LineStatus status = takeLine(buffer, line);

  if (status == LINE_INCOMPLETE) {
    if (unread(buffer) > limits::kMaxChunkLine)
      return fail(http_status::kBadRequest);
    return false;
  }

  if (status == LINE_INVALID || line.size() > limits::kMaxChunkLine)
    return fail(http_status::kBadRequest);
  return readChunkSize(line);
}

bool RequestParser::readChunkEnd(std::string& buffer) {
  std::string line;
  const LineStatus status = takeLine(buffer, line);

  if (status == LINE_INCOMPLETE) {
    // Only a CR may still be waiting for its LF.
    // LF를 기다리는 CR 하나만 남아 있을 수 있다.
    if (unread(buffer) > 1 ||
        (unread(buffer) == 1 && buffer[consumed_] != '\r'))
      return fail(http_status::kBadRequest);
    return false;
  }

  if (status == LINE_INVALID || !line.empty())
    return fail(http_status::kBadRequest);
  state_ = CHUNK_SIZE;
  return true;
}

bool RequestParser::readTrailerLine(std::string& buffer) {
  std::string line;
  if (!takeHeaderLine(buffer, line)) return false;

  if (line.empty()) {
    state_ = COMPLETE;
    return true;
  }

  std::string name;
  std::string value;
  return parseFieldLine(line, name, value);
}

// Exactly one SP between the parts: extra spaces leave an empty part or a
// space inside the version, which the checks below reject.
// 부분 사이의 SP는 정확히 하나다. 공백이 더 있으면 빈 부분이 생기거나
// 버전 안에 공백이 들어가고, 아래 검사가 그것을 거부한다.
bool RequestParser::parseRequestLine(const std::string& line) {
  const std::string::size_type first = line.find(' ');
  const std::string::size_type second =
      (first == std::string::npos) ? first : line.find(' ', first + 1);
  if (second == std::string::npos) return fail(http_status::kBadRequest);

  const std::string method = line.substr(0, first);
  const std::string target = line.substr(first + 1, second - first - 1);
  const std::string version = line.substr(second + 1);
  if (!isToken(method) || !isTargetText(target) || !isHttpVersion(version))
    return fail(http_status::kBadRequest);

  const char major = version[5];  // "HTTP/x.y": x
  const char minor = version[7];  // "HTTP/x.y": y
  if (major != '1') return fail(http_status::kHttpVersionNotSupported);
  // A higher 1.x minor version is handled as 1.1 (RFC 9110 section 6.2).
  // 1.x의 더 높은 minor 버전은 1.1로 처리한다 (RFC 9110 6.2절).
  request_.version =
      (minor == '0') ? request_parser::kHttp10 : request_parser::kHttp11;

  if (!isImplementedMethod(method)) return fail(http_status::kNotImplemented);
  request_.method = method;
  if (!parseTarget(target)) return fail(http_status::kBadRequest);
  return true;
}

// origin-form "/path?query" or absolute-form "http://host/path?query"
// (section 3.2). The host of the absolute-form is not used: there are no
// virtual hosts (design decision 4).
// origin-form "/path?query" 또는 absolute-form "http://host/path?query"
// (3.2절). 가상 호스트가 없으므로 absolute-form의 host는 쓰지 않는다
// (설계 결정 4).
bool RequestParser::parseTarget(const std::string& target) {
  std::string pathAndQuery;
  const std::string::size_type prefixLength =
      std::string(kAbsoluteFormPrefix).size();
  if (target[0] == '/') {
    pathAndQuery = target;
  } else if (toAsciiLower(target.substr(0, prefixLength)) ==
             kAbsoluteFormPrefix) {
    const std::string rest = target.substr(prefixLength);
    const std::string::size_type authorityEnd = rest.find_first_of("/?");
    if (authorityEnd == 0) return false;
    pathAndQuery =
        (authorityEnd == std::string::npos) ? "/" : rest.substr(authorityEnd);
    if (pathAndQuery[0] == '?') pathAndQuery.insert(0, "/");
  } else {
    return false;
  }

  const std::string::size_type question = pathAndQuery.find('?');
  request_.target = target;
  request_.path = pathAndQuery.substr(0, question);
  if (question != std::string::npos)
    request_.query = pathAndQuery.substr(question + 1);
  return true;
}

// field-line = field-name ":" OWS field-value OWS (section 5). A line that
// starts with whitespace (after the request line, or an obs-fold) is
// rejected (sections 2.2 and 5.2) because its name is not a token.
// 공백으로 시작하는 줄(요청 줄 바로 뒤, 또는 obs-fold)은 이름이 token이
// 아니라서 거부된다 (2.2절, 5.2절).
bool RequestParser::parseFieldLine(const std::string& line, std::string& name,
                                   std::string& value) {
  const std::string::size_type colon = line.find(':');
  if (colon == std::string::npos) return fail(http_status::kBadRequest);

  name = line.substr(0, colon);
  value = trimWhitespace(line.substr(colon + 1));

  // A space before the colon makes the name an invalid token (section 5.1).
  // 콜론 앞에 공백이 있으면 이름이 올바른 token이 아니다 (5.1절).
  if (!isToken(name) || !isFieldValue(value))
    return fail(http_status::kBadRequest);
  return true;
}

bool RequestParser::finishHead() {
  const HeaderMap& headers = request_.headers;
  const std::size_t hosts = headers.count(header_name::kHost);
  // Section 3.2: Host is required in HTTP/1.1, never repeated, and valid.
  // 3.2절: HTTP/1.1에서 Host는 필수이고, 반복되지 않고, 올바라야 한다.
  if ((hosts == 0 && request_.version == request_parser::kHttp11) ||
      hosts > 1 ||
      (hosts == 1 && !isHostValue(*headers.first(header_name::kHost))))
    return fail(http_status::kBadRequest);

  if (!decideBodyLength()) return false;
  request_.persistent = keepsConnection(request_);

  // Stop here: feed() reports HEAD_DONE so that the caller can set the
  // body limit of the chosen location before the body is read.
  // 여기서 멈춘다. feed()가 HEAD_DONE을 알려서, 본문을 읽기 전에 호출자가
  // 고른 location의 본문 상한을 정할 수 있게 한다.
  state_ = BODY_START;
  return false;
}

bool RequestParser::decideBodyLength() {
  const std::vector<std::string> codings =
      splitList(request_.headers.all(header_name::kTransferEncoding));
  const std::vector<std::string> lengths =
      splitList(request_.headers.all(header_name::kContentLength));

  if (request_.headers.has(header_name::kTransferEncoding)) {
    // HTTP/1.0 with Transfer-Encoding is faulty framing (section 6.1);
    // with Content-Length as well it may be smuggling (section 6.3).
    // HTTP/1.0에 Transfer-Encoding이 있으면 길이 표시가 잘못된 것이고
    // (6.1절), Content-Length까지 있으면 smuggling일 수 있다 (6.3절).
    if (request_.version == request_parser::kHttp10 ||
        request_.headers.has(header_name::kContentLength))
      return fail(http_status::kBadRequest);
    if (codings.empty() || toAsciiLower(codings.back()) != kChunked)
      return fail(http_status::kBadRequest);

    // chunked may be applied only once; no other coding is implemented.
    // chunked는 한 번만 적용할 수 있고, 다른 coding은 구현하지 않았다.
    for (std::size_t i = 0; i + 1 < codings.size(); ++i) {
      if (toAsciiLower(codings[i]) == kChunked)
        return fail(http_status::kBadRequest);
    }

    if (codings.size() > 1) return fail(http_status::kNotImplemented);
    chunked_ = true;
    return true;
  }

  if (request_.headers.has(header_name::kContentLength)) {
    // A list is allowed only if every value is the same (section 6.3).
    // 목록은 모든 값이 같을 때만 허용한다 (6.3절).
    if (lengths.empty()) return fail(http_status::kBadRequest);
    for (std::size_t i = 0; i < lengths.size(); ++i) {
      unsigned long value = 0;
      if (!parseDecimal(lengths[i], kMaxContentLength, value) ||
          (i > 0 && value != contentLength_))
        return fail(http_status::kBadRequest);
      contentLength_ = static_cast<std::size_t>(value);
    }
    return true;
  }

  // HTTP/1.0 POST must carry Content-Length (RFC 1945 section 8.3).
  // HTTP/1.0 POST에는 Content-Length가 있어야 한다 (RFC 1945 8.3절).
  if (request_.version == request_parser::kHttp10 &&
      request_.method == methods::kPost)
    return fail(http_status::kBadRequest);
  return true;
}

// Chunk extensions are ignored. / chunk 확장은 무시한다.
bool RequestParser::readChunkSize(const std::string& line) {
  std::size_t size = 0;
  std::string::size_type i = 0;
  for (; i < line.size() && hexDigitValue(line[i]) != hex::kNotHex; ++i) {
    if (!appendDigit(size, hex::kBase, hexDigitValue(line[i])))
      return fail(http_status::kBadRequest);
  }
  if (i == 0) return fail(http_status::kBadRequest);

  while (i < line.size() && isWhitespace(line[i])) ++i;
  if (i < line.size() && line[i] != ';') return fail(http_status::kBadRequest);

  if (size == 0) {
    state_ = TRAILERS;
    return true;
  }

  if (size > maxBodySize_ - request_.body.size())
    return fail(http_status::kContentTooLarge);
  remaining_ = size;
  state_ = CHUNK_DATA;
  return true;
}
