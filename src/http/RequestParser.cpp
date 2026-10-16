#include "http/RequestParser.hpp"

#include <cctype>
#include <cstring>
#include <limits>
#include <vector>

// Section numbers in comments refer to RFC 9112 unless noted otherwise.
// The choices where the RFC allows several behaviours are listed in
// analysis/design section 6 of the wiki.

namespace {

const std::size_t kMaxRequestLine = 8192;  // at least 8000 (section 3)
const std::size_t kMaxHeaderBytes = 16384;
const std::size_t kMaxChunkLine = 4096;

const int kBadRequest = 400;
const int kContentTooLarge = 413;
const int kUriTooLong = 414;
const int kHeaderFieldsTooLarge = 431;
const int kNotImplemented = 501;
const int kVersionNotSupported = 505;

const char* const kKnownMethods[] = {"GET", "HEAD", "POST", "DELETE"};
const std::size_t kLongestMethod = 6;  // "DELETE"

const char* const kHttp10 = "HTTP/1.0";
const char* const kHttp11 = "HTTP/1.1";
const char* const kVersionPrefix = "HTTP/";
const std::string::size_type kVersionLength = 8;  // "HTTP/x.y"
const std::string::size_type kMajorIndex = 5;
const std::string::size_type kDotIndex = 6;
const std::string::size_type kMinorIndex = 7;
const unsigned char kDeleteChar = 0x7f;
const char* const kAbsoluteFormPrefix = "http://";
const char* const kChunked = "chunked";

const int kDecimalBase = 10;
const int kHexBase = 16;
const std::size_t kMaxSize = std::numeric_limits<std::size_t>::max();

bool isTokenChar(char c) {
  return std::isalnum(static_cast<unsigned char>(c)) ||
         (c != '\0' && std::strchr("!#$%&'*+-.^_`|~", c) != 0);
}

bool isToken(const std::string& text) {
  if (text.empty()) return false;
  for (std::string::size_type i = 0; i < text.size(); ++i) {
    if (!isTokenChar(text[i])) return false;
  }
  return true;
}

bool isWhitespace(char c) { return c == ' ' || c == '\t'; }

std::string trimWhitespace(const std::string& text) {
  std::string::size_type begin = 0;
  std::string::size_type end = text.size();
  while (begin < end && isWhitespace(text[begin])) ++begin;
  while (end > begin && isWhitespace(text[end - 1])) --end;
  return text.substr(begin, end - begin);
}

std::string toLower(const std::string& text) {
  std::string lowered(text);
  for (std::string::size_type i = 0; i < lowered.size(); ++i)
    lowered[i] =
        static_cast<char>(std::tolower(static_cast<unsigned char>(lowered[i])));
  return lowered;
}

// Splits a comma-separated list (RFC 9110 section 5.6.1), trimming
// whitespace and dropping empty elements.
std::vector<std::string> splitList(const std::vector<std::string>& lines) {
  std::vector<std::string> items;
  for (std::size_t i = 0; i < lines.size(); ++i) {
    std::string::size_type start = 0;
    while (start <= lines[i].size()) {
      std::string::size_type comma = lines[i].find(',', start);
      if (comma == std::string::npos) comma = lines[i].size();
      const std::string item =
          trimWhitespace(lines[i].substr(start, comma - start));
      if (!item.empty()) items.push_back(item);
      start = comma + 1;
    }
  }
  return items;
}

bool isKnownMethod(const std::string& method) {
  for (std::size_t i = 0; i < sizeof(kKnownMethods) / sizeof(kKnownMethods[0]);
       ++i) {
    if (method == kKnownMethods[i]) return true;
  }
  return false;
}

// Visible ASCII only: no whitespace, control or non-ASCII octet, and no
// fragment, which is not part of a request-target (section 3.2).
bool isTargetText(const std::string& target) {
  for (std::string::size_type i = 0; i < target.size(); ++i) {
    const unsigned char c = static_cast<unsigned char>(target[i]);
    if (c <= ' ' || c >= kDeleteChar || c == '#') return false;
  }
  return !target.empty();
}

// Field values may hold any visible octet, SP and HTAB (RFC 9110 5.5).
bool isFieldValue(const std::string& value) {
  for (std::string::size_type i = 0; i < value.size(); ++i) {
    const unsigned char c = static_cast<unsigned char>(value[i]);
    if ((c < ' ' && c != '\t') || c == kDeleteChar) return false;
  }
  return true;
}

// Host = uri-host [ ":" port ] (RFC 9110 7.2). Only the character set is
// checked; an empty value is allowed by the grammar.
bool isHostValue(const std::string& value) {
  for (std::string::size_type i = 0; i < value.size(); ++i) {
    const char c = value[i];
    if (!std::isalnum(static_cast<unsigned char>(c)) &&
        std::strchr("-._~%!$&'()*+,;=:[]", c) == 0)
      return false;
  }
  return true;
}

// Adds one digit to value, or returns false if the result would not fit.
bool appendDigit(std::size_t& value, int base, int digit) {
  const std::size_t factor = static_cast<std::size_t>(base);
  const std::size_t add = static_cast<std::size_t>(digit);
  if (value > (kMaxSize - add) / factor) return false;
  value = value * factor + add;
  return true;
}

bool parseDecimal(const std::string& text, std::size_t& value) {
  if (text.empty()) return false;
  value = 0;
  for (std::string::size_type i = 0; i < text.size(); ++i) {
    if (!std::isdigit(static_cast<unsigned char>(text[i]))) return false;
    if (!appendDigit(value, kDecimalBase, text[i] - '0')) return false;
  }
  return true;
}

int hexDigitValue(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  const char lower =
      static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  if (lower >= 'a' && lower <= 'f') return lower - 'a' + kDecimalBase;
  return -1;
}

// A request line that does not end within the limit: a method longer than
// any known one gets 501 (section 3), anything else 414.
int overlongRequestLineStatus(const std::string& text) {
  const std::string::size_type space = text.find(' ');
  const std::string::size_type methodLength =
      (space == std::string::npos) ? text.size() : space;
  return methodLength > kLongestMethod ? kNotImplemented : kUriTooLong;
}

}  // namespace

ParserLimits defaultParserLimits() {
  ParserLimits limits;
  limits.maxRequestLine = kMaxRequestLine;
  limits.maxHeaderBytes = kMaxHeaderBytes;
  limits.maxChunkLine = kMaxChunkLine;
  return limits;
}

RequestParser::RequestParser(const ParserLimits& limits) : limits_(limits) {
  reset();
}

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
  Result result = NEED_MORE;
  while (step(buffer, result)) {
  }
  return result;
}

void RequestParser::setMaxBodySize(std::size_t maxBodySize) {
  maxBodySize_ = maxBodySize;
}

int RequestParser::errorStatus() const { return errorStatus_; }

bool RequestParser::hasStarted() const { return started_; }

const Request& RequestParser::request() const { return request_; }

// One parsing step. Returns true to keep going, false to return result.
bool RequestParser::step(std::string& buffer, Result& result) {
  switch (state_) {
    case REQUEST_LINE:
      return readRequestLine(buffer, result);
    case HEADERS:
      return readHeaderLine(buffer, result);
    case BODY_START:
      return startBody(result);
    case BODY_LENGTH:
      return readBodyBytes(buffer, COMPLETE, result);
    case CHUNK_SIZE:
      return readChunkLine(buffer, result);
    case CHUNK_DATA:
      return readBodyBytes(buffer, CHUNK_DATA_END, result);
    case CHUNK_DATA_END:
      return readChunkEnd(buffer, result);
    case TRAILERS:
      return readTrailerLine(buffer, result);
    case COMPLETE:
      result = DONE;
      return false;
    case FAILED:
      break;
  }
  result = ERROR;
  return false;
}

bool RequestParser::fail(int status, Result& result) {
  state_ = FAILED;
  errorStatus_ = status;
  result = ERROR;
  return false;
}

// Takes one line ending in LF (section 2.2: a lone LF is accepted, a
// preceding CR is dropped, any other CR makes the line invalid).
RequestParser::LineStatus RequestParser::takeLine(std::string& buffer,
                                                  std::string& line) {
  const std::string::size_type newline = buffer.find('\n');
  if (newline == std::string::npos) return LINE_INCOMPLETE;
  line.assign(buffer, 0, newline);
  buffer.erase(0, newline + 1);
  if (!line.empty() && line[line.size() - 1] == '\r')
    line.erase(line.size() - 1);
  return line.find('\r') == std::string::npos ? LINE_READY : LINE_INVALID;
}

bool RequestParser::readRequestLine(std::string& buffer, Result& result) {
  std::string line;
  const LineStatus status = takeLine(buffer, line);
  if (status == LINE_INCOMPLETE) {
    if (buffer.size() > limits_.maxRequestLine)
      return fail(overlongRequestLineStatus(buffer), result);
    return false;
  }
  if (status == LINE_INVALID) return fail(kBadRequest, result);
  if (line.empty()) {
    // Empty lines before the request line are ignored (section 2.2), but
    // only as many as would fit in a request line.
    preambleBytes_ += 1;
    if (preambleBytes_ > limits_.maxRequestLine)
      return fail(kBadRequest, result);
    return true;
  }
  if (line.size() > limits_.maxRequestLine)
    return fail(overlongRequestLineStatus(line), result);
  if (!parseRequestLine(line, result)) return false;
  state_ = HEADERS;
  return true;
}

// request-line = method SP request-target SP HTTP-version (section 3),
// with exactly one SP between the parts. Extra spaces leave an empty part
// or a space inside the version, which the checks below reject.
bool RequestParser::parseRequestLine(const std::string& line, Result& result) {
  const std::string::size_type first = line.find(' ');
  const std::string::size_type second =
      (first == std::string::npos) ? first : line.find(' ', first + 1);
  if (second == std::string::npos) return fail(kBadRequest, result);
  const std::string method = line.substr(0, first);
  const std::string target = line.substr(first + 1, second - first - 1);
  const std::string version = line.substr(second + 1);
  if (!isToken(method) || !isTargetText(target) ||
      version.size() != kVersionLength ||
      version.compare(0, std::strlen(kVersionPrefix), kVersionPrefix) != 0 ||
      !std::isdigit(static_cast<unsigned char>(version[kMajorIndex])) ||
      version[kDotIndex] != '.' ||
      !std::isdigit(static_cast<unsigned char>(version[kMinorIndex])))
    return fail(kBadRequest, result);
  if (version[kMajorIndex] != '1') return fail(kVersionNotSupported, result);
  // A higher 1.x minor version is handled as 1.1 (RFC 9110 2.5).
  request_.version = (version[kMinorIndex] == '0') ? kHttp10 : kHttp11;
  if (!isKnownMethod(method)) return fail(kNotImplemented, result);
  request_.method = method;
  if (!parseTarget(target)) return fail(kBadRequest, result);
  return true;
}

// origin-form "/path?query" or absolute-form "http://host/path?query"
// (section 3.2). The host of the absolute-form is not used: there are no
// virtual hosts (design decision 4).
bool RequestParser::parseTarget(const std::string& target) {
  std::string pathAndQuery;
  const std::string::size_type prefixLength = std::strlen(kAbsoluteFormPrefix);
  if (target[0] == '/') {
    pathAndQuery = target;
  } else if (toLower(target.substr(0, prefixLength)) == kAbsoluteFormPrefix) {
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

bool RequestParser::readHeaderLine(std::string& buffer, Result& result) {
  std::string line;
  const LineStatus status = takeLine(buffer, line);
  if (status == LINE_INCOMPLETE) {
    if (headerBytes_ + buffer.size() > limits_.maxHeaderBytes)
      return fail(kHeaderFieldsTooLarge, result);
    return false;
  }
  if (status == LINE_INVALID) return fail(kBadRequest, result);
  headerBytes_ += line.size() + 1;
  if (headerBytes_ > limits_.maxHeaderBytes)
    return fail(kHeaderFieldsTooLarge, result);
  if (line.empty()) return finishHead(result);
  return parseHeaderLine(line, true, result);
}

// field-line = field-name ":" OWS field-value OWS (section 5). A line that
// starts with whitespace (after the request line, or an obs-fold) is
// rejected (sections 2.2 and 5.2) because its name is not a token.
bool RequestParser::parseHeaderLine(const std::string& line, bool store,
                                    Result& result) {
  const std::string::size_type colon = line.find(':');
  if (colon == std::string::npos) return fail(kBadRequest, result);
  const std::string name = line.substr(0, colon);
  const std::string value = trimWhitespace(line.substr(colon + 1));
  // A space before the colon makes the name an invalid token (section 5.1).
  if (!isToken(name) || !isFieldValue(value)) return fail(kBadRequest, result);
  if (store) request_.headers.add(name, value);
  return true;
}

bool RequestParser::finishHead(Result& result) {
  const HeaderMap& headers = request_.headers;
  const std::size_t hosts = headers.count("Host");
  // Section 3.2: Host is required in HTTP/1.1, never repeated, and valid.
  if ((hosts == 0 && request_.version == kHttp11) || hosts > 1 ||
      (hosts == 1 && !isHostValue(*headers.first("Host"))))
    return fail(kBadRequest, result);
  if (!decideBodyLength(result)) return false;
  state_ = BODY_START;
  result = HEAD_DONE;
  return false;
}

// Section 6.3 for requests.
bool RequestParser::decideBodyLength(Result& result) {
  const std::vector<std::string> codings =
      splitList(request_.headers.all("Transfer-Encoding"));
  const std::vector<std::string> lengths =
      splitList(request_.headers.all("Content-Length"));
  if (request_.headers.has("Transfer-Encoding")) {
    // HTTP/1.0 with Transfer-Encoding is faulty framing (section 6.1);
    // with Content-Length as well it may be smuggling (section 6.3).
    if (request_.version == kHttp10 || request_.headers.has("Content-Length"))
      return fail(kBadRequest, result);
    if (codings.empty() || toLower(codings.back()) != kChunked)
      return fail(kBadRequest, result);
    // chunked may be applied only once; no other coding is implemented.
    for (std::size_t i = 0; i + 1 < codings.size(); ++i) {
      if (toLower(codings[i]) == kChunked) return fail(kBadRequest, result);
    }
    if (codings.size() > 1) return fail(kNotImplemented, result);
    chunked_ = true;
    return true;
  }
  if (request_.headers.has("Content-Length")) {
    // A list is allowed only if every value is the same (section 6.3).
    if (lengths.empty()) return fail(kBadRequest, result);
    for (std::size_t i = 0; i < lengths.size(); ++i) {
      std::size_t value = 0;
      if (!parseDecimal(lengths[i], value) ||
          (i > 0 && value != contentLength_))
        return fail(kBadRequest, result);
      contentLength_ = value;
    }
    return true;
  }
  // HTTP/1.0 POST must carry Content-Length (RFC 1945 section 8.3).
  if (request_.version == kHttp10 && request_.method == "POST")
    return fail(kBadRequest, result);
  return true;
}

bool RequestParser::startBody(Result& result) {
  if (chunked_) {
    state_ = CHUNK_SIZE;
    return true;
  }
  if (contentLength_ > maxBodySize_) return fail(kContentTooLarge, result);
  if (contentLength_ == 0) {
    state_ = COMPLETE;
    return true;
  }
  // Known size: reserve once instead of growing (design decision 11).
  request_.body.reserve(contentLength_);
  remaining_ = contentLength_;
  state_ = BODY_LENGTH;
  return true;
}

bool RequestParser::readBodyBytes(std::string& buffer, State whenDone,
                                  Result& result) {
  if (buffer.empty()) {
    result = NEED_MORE;
    return false;
  }
  const std::size_t taken =
      (buffer.size() < remaining_) ? buffer.size() : remaining_;
  request_.body.append(buffer, 0, taken);
  buffer.erase(0, taken);
  remaining_ -= taken;
  if (remaining_ == 0) state_ = whenDone;
  return true;
}

bool RequestParser::readChunkLine(std::string& buffer, Result& result) {
  std::string line;
  const LineStatus status = takeLine(buffer, line);
  if (status == LINE_INCOMPLETE) {
    if (buffer.size() > limits_.maxChunkLine) return fail(kBadRequest, result);
    return false;
  }
  if (status == LINE_INVALID || line.size() > limits_.maxChunkLine)
    return fail(kBadRequest, result);
  return readChunkSize(line, result);
}

// chunk-size [ chunk-ext ] (section 7.1). Extensions are ignored.
bool RequestParser::readChunkSize(const std::string& line, Result& result) {
  std::size_t size = 0;
  std::string::size_type i = 0;
  for (; i < line.size() && hexDigitValue(line[i]) >= 0; ++i) {
    if (!appendDigit(size, kHexBase, hexDigitValue(line[i])))
      return fail(kBadRequest, result);
  }
  if (i == 0) return fail(kBadRequest, result);
  while (i < line.size() && isWhitespace(line[i])) ++i;
  if (i < line.size() && line[i] != ';') return fail(kBadRequest, result);
  if (size == 0) {
    state_ = TRAILERS;
    return true;
  }
  if (size > maxBodySize_ - request_.body.size())
    return fail(kContentTooLarge, result);
  remaining_ = size;
  state_ = CHUNK_DATA;
  return true;
}

// The CRLF after chunk-data.
bool RequestParser::readChunkEnd(std::string& buffer, Result& result) {
  std::string line;
  const LineStatus status = takeLine(buffer, line);
  if (status == LINE_INCOMPLETE) {
    // Only a CR may still be waiting for its LF.
    if (buffer.size() > 1 || (buffer.size() == 1 && buffer[0] != '\r'))
      return fail(kBadRequest, result);
    return false;
  }
  if (status == LINE_INVALID || !line.empty()) return fail(kBadRequest, result);
  state_ = CHUNK_SIZE;
  return true;
}

// Trailer fields are checked and discarded (section 7.1.2), counted
// against the same limit as the header section.
bool RequestParser::readTrailerLine(std::string& buffer, Result& result) {
  std::string line;
  const LineStatus status = takeLine(buffer, line);
  if (status == LINE_INCOMPLETE) {
    if (headerBytes_ + buffer.size() > limits_.maxHeaderBytes)
      return fail(kHeaderFieldsTooLarge, result);
    return false;
  }
  if (status == LINE_INVALID) return fail(kBadRequest, result);
  headerBytes_ += line.size() + 1;
  if (headerBytes_ > limits_.maxHeaderBytes)
    return fail(kHeaderFieldsTooLarge, result);
  if (line.empty()) {
    state_ = COMPLETE;
    return true;
  }
  return parseHeaderLine(line, false, result);
}
