#include "http/RequestParser.hpp"

#include <limits>
#include <vector>

#include "core/Limits.hpp"
#include "http/Ascii.hpp"
#include "http/HeaderNames.hpp"
#include "http/Hex.hpp"
#include "http/Methods.hpp"
#include "http/StatusCodes.hpp"

// Section numbers in comments refer to RFC 9112 unless noted otherwise.
// The choices where the RFC allows several behaviours are listed in
// analysis/design section 6 of the wiki.

namespace {

// tchar (RFC 9110 section 5.6.2).
const std::string kTokenChars = ascii::kAlphanumerics + "!#$%&'*+-.^_`|~";
// Host characters: unreserved, pct-encoded, sub-delims, ":" and IP-literal
// brackets (RFC 3986 section 3.2.2).
const std::string kHostChars = ascii::kAlphanumerics + "-._~%!$&'()*+,;=:[]";
const char* const kAbsoluteFormPrefix = "http://";
const char* const kChunked = "chunked";
const unsigned char kDeleteChar = 0x7f;  // DEL

const std::size_t kMaxSize = std::numeric_limits<std::size_t>::max();
// The largest Content-Length that fits both parseDecimal() and size_t.
const unsigned long kMaxContentLength =
    std::numeric_limits<unsigned long>::max() < kMaxSize
        ? std::numeric_limits<unsigned long>::max()
        : static_cast<unsigned long>(kMaxSize);

bool isToken(const std::string& text) {
  return !text.empty() && consistsOf(text, kTokenChars);
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
    lowered[i] = toAsciiLower(lowered[i]);
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

// HTTP-version = "HTTP/" DIGIT "." DIGIT, case-sensitive (section 2.3).
bool isHttpVersion(const std::string& version) {
  return version.size() == 8 && version.compare(0, 5, "HTTP/") == 0 &&
         isAsciiDigit(version[5]) && version[6] == '.' &&
         isAsciiDigit(version[7]);
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
  return consistsOf(value, kHostChars);
}

// Adds one digit to value, or returns false if the result would not fit.
bool appendDigit(std::size_t& value, int base, int digit) {
  const std::size_t factor = static_cast<std::size_t>(base);
  const std::size_t add = static_cast<std::size_t>(digit);
  if (value > (kMaxSize - add) / factor) return false;
  value = value * factor + add;
  return true;
}

// A request line that does not end within the limit: a method longer than
// any known one gets 501 (section 3), anything else 414.
int overlongRequestLineStatus(const std::string& text) {
  const std::string::size_type space = text.find(' ');
  const std::string::size_type methodLength =
      (space == std::string::npos) ? text.size() : space;
  return methodLength > longestImplementedMethod()
             ? http_status::kNotImplemented
             : http_status::kUriTooLong;
}

}  // namespace

ParserLimits defaultParserLimits() {
  ParserLimits parserLimits;
  parserLimits.maxRequestLine = limits::kMaxRequestLine;
  parserLimits.maxHeaderBytes = limits::kMaxHeaderBytes;
  parserLimits.maxChunkLine = limits::kMaxChunkLine;
  return parserLimits;
}

RequestParser::RequestParser(const ParserLimits& limits)
    : limits_(limits), consumed_(0) {
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
    case BODY_START:  // the head is complete; the caller sets the body limit
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

// Takes one line ending in LF (section 2.2: a lone LF is accepted, a
// preceding CR is dropped, any other CR makes the line invalid).
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

// A line of the header section or of the trailer. Both count against the
// same limit, so a long trailer cannot bypass it.
bool RequestParser::takeHeaderLine(const std::string& buffer,
                                   std::string& line) {
  const LineStatus status = takeLine(buffer, line);
  if (status == LINE_INCOMPLETE) {
    if (headerBytes_ + unread(buffer) > limits_.maxHeaderBytes)
      return fail(http_status::kRequestHeaderFieldsTooLarge);
    return false;
  }
  if (status == LINE_INVALID) return fail(http_status::kBadRequest);
  headerBytes_ += line.size() + 1;
  if (headerBytes_ > limits_.maxHeaderBytes)
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
    if (unread(buffer) > limits_.maxRequestLine)
      return fail(overlongRequestLineStatus(buffer.substr(consumed_)));
    return false;
  }
  if (status == LINE_INVALID) return fail(http_status::kBadRequest);
  if (line.empty()) {
    // Empty lines before the request line are ignored (section 2.2), but
    // only as many as would fit in a request line.
    preambleBytes_ += 1;
    if (preambleBytes_ > limits_.maxRequestLine)
      return fail(http_status::kBadRequest);
    return true;
  }
  if (line.size() > limits_.maxRequestLine)
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
    if (unread(buffer) > limits_.maxChunkLine)
      return fail(http_status::kBadRequest);
    return false;
  }
  if (status == LINE_INVALID || line.size() > limits_.maxChunkLine)
    return fail(http_status::kBadRequest);
  return readChunkSize(line);
}

// The CRLF after chunk-data.
bool RequestParser::readChunkEnd(std::string& buffer) {
  std::string line;
  const LineStatus status = takeLine(buffer, line);
  if (status == LINE_INCOMPLETE) {
    // Only a CR may still be waiting for its LF.
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

// Trailer fields are checked and discarded (section 7.1.2).
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

// request-line = method SP request-target SP HTTP-version (section 3),
// with exactly one SP between the parts. Extra spaces leave an empty part
// or a space inside the version, which the checks below reject.
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
  const char major = version[5];  // "HTTP/x.y"
  const char minor = version[7];
  if (major != '1') return fail(http_status::kHttpVersionNotSupported);
  // A higher 1.x minor version is handled as 1.1 (RFC 9110 section 6.2).
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
bool RequestParser::parseTarget(const std::string& target) {
  std::string pathAndQuery;
  const std::string::size_type prefixLength =
      std::string(kAbsoluteFormPrefix).size();
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

// field-line = field-name ":" OWS field-value OWS (section 5). A line that
// starts with whitespace (after the request line, or an obs-fold) is
// rejected (sections 2.2 and 5.2) because its name is not a token.
bool RequestParser::parseFieldLine(const std::string& line, std::string& name,
                                   std::string& value) {
  const std::string::size_type colon = line.find(':');
  if (colon == std::string::npos) return fail(http_status::kBadRequest);
  name = line.substr(0, colon);
  value = trimWhitespace(line.substr(colon + 1));
  // A space before the colon makes the name an invalid token (section 5.1).
  if (!isToken(name) || !isFieldValue(value))
    return fail(http_status::kBadRequest);
  return true;
}

bool RequestParser::finishHead() {
  const HeaderMap& headers = request_.headers;
  const std::size_t hosts = headers.count(header_name::kHost);
  // Section 3.2: Host is required in HTTP/1.1, never repeated, and valid.
  if ((hosts == 0 && request_.version == request_parser::kHttp11) ||
      hosts > 1 ||
      (hosts == 1 && !isHostValue(*headers.first(header_name::kHost))))
    return fail(http_status::kBadRequest);
  if (!decideBodyLength()) return false;
  // Stop here: feed() reports HEAD_DONE so that the caller can set the
  // body limit of the chosen location before the body is read.
  state_ = BODY_START;
  return false;
}

// Section 6.3 for requests.
bool RequestParser::decideBodyLength() {
  const std::vector<std::string> codings =
      splitList(request_.headers.all(header_name::kTransferEncoding));
  const std::vector<std::string> lengths =
      splitList(request_.headers.all(header_name::kContentLength));
  if (request_.headers.has(header_name::kTransferEncoding)) {
    // HTTP/1.0 with Transfer-Encoding is faulty framing (section 6.1);
    // with Content-Length as well it may be smuggling (section 6.3).
    if (request_.version == request_parser::kHttp10 ||
        request_.headers.has(header_name::kContentLength))
      return fail(http_status::kBadRequest);
    if (codings.empty() || toLower(codings.back()) != kChunked)
      return fail(http_status::kBadRequest);
    // chunked may be applied only once; no other coding is implemented.
    for (std::size_t i = 0; i + 1 < codings.size(); ++i) {
      if (toLower(codings[i]) == kChunked)
        return fail(http_status::kBadRequest);
    }
    if (codings.size() > 1) return fail(http_status::kNotImplemented);
    chunked_ = true;
    return true;
  }
  if (request_.headers.has(header_name::kContentLength)) {
    // A list is allowed only if every value is the same (section 6.3).
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
  if (request_.version == request_parser::kHttp10 &&
      request_.method == methods::kPost)
    return fail(http_status::kBadRequest);
  return true;
}

// chunk-size [ chunk-ext ] (section 7.1). Extensions are ignored.
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
