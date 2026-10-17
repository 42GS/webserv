/**
 * @file  ResponseStream.cpp
 * @brief Response serialization and file streaming.
 *        응답 직렬화와 파일 나눠 보내기.
 */
#include "http/ResponseStream.hpp"

#include <fcntl.h>
#include <unistd.h>

#include <sstream>

#include "core/Limits.hpp"
#include "http/HeaderNames.hpp"
#include "http/StatusCodes.hpp"
#include "http/StatusText.hpp"

namespace {

/**
 * @brief True if a field value holds no CR or LF.
 *        필드 값에 CR이나 LF가 없으면 true.
 *
 * RFC 9112 section 11.1: no CR or LF may reach the header section from
 * anywhere but serializeHead(), so a field value holding one is dropped.
 * RFC 9112 11.1절: serializeHead() 말고는 어디서도 CR이나 LF가 헤더
 * 영역에 들어가면 안 되므로, 그런 값을 가진 필드는 버린다.
 *
 * @param[in] value Field value. / 필드 값.
 * @return Whether it is safe to write. / 써도 안전한지.
 */
bool isSafeFieldValue(const std::string& value) {
  return value.find_first_of("\r\n") == std::string::npos;
}

/**
 * @brief Writes the status line, the fields and Content-Length.
 *        상태 줄, 필드, Content-Length를 쓴다.
 * @param[in] response The response. / 응답.
 * @return The head, ending with an empty line. / 빈 줄로 끝나는 머리.
 */
std::string serializeHead(const Response& response) {
  std::ostringstream head;
  // The SP after the code stays even without a reason (RFC 9112 4).
  // 이유 문구가 없어도 코드 뒤의 SP는 남긴다 (RFC 9112 4절).
  head << "HTTP/1.1 " << response.status << " " << reasonPhrase(response.status)
       << "\r\n";
  for (std::size_t i = 0; i < response.headers.size(); ++i) {
    const HeaderMap::Field& field = response.headers.at(i);
    if (isSafeFieldValue(field.second))
      head << field.first << ": " << field.second << "\r\n";
  }
  // Never for 204; for 304 only the length a 200 would have, which is not
  // known (RFC 9110 8.6). For HEAD it is the GET value.
  // 204에는 절대 보내지 않는다. 304에는 200의 길이만 보낼 수 있는데 그
  // 길이를 모른다 (RFC 9110 8.6절). HEAD에는 GET의 값을 쓴다.
  if (response.status != http_status::kNoContent &&
      response.status != http_status::kNotModified) {
    const std::size_t length = response.bodyFile.empty()
                                   ? response.body.size()
                                   : response.bodyFileSize;
    head << header_name::kContentLength << ": " << length << "\r\n";
  }
  head << "\r\n";
  return head.str();
}

}  // namespace

ResponseStream::ResponseStream() : offset_(0), fileFd_(-1), fileRemaining_(0) {}

ResponseStream::~ResponseStream() { closeFile(); }

bool ResponseStream::reset(const Response& response) {
  closeFile();
  out_ = serializeHead(response);
  offset_ = 0;
  fileRemaining_ = 0;
  if (response.omitBody ||
      (!response.bodyFile.empty() && response.bodyFileSize == 0))
    return true;
  if (response.bodyFile.empty()) {
    out_ += response.body;
    return true;
  }
  fileFd_ = open(response.bodyFile.c_str(), O_RDONLY);
  if (fileFd_ < 0) return false;
  fileRemaining_ = response.bodyFileSize;
  return true;
}

bool ResponseStream::hasPending() const {
  return offset_ < out_.size() || fileRemaining_ > 0;
}

const char* ResponseStream::data() const { return out_.data() + offset_; }

std::size_t ResponseStream::size() const { return out_.size() - offset_; }

bool ResponseStream::consume(std::size_t n) {
  offset_ += n;
  if (offset_ < out_.size() || fileRemaining_ == 0) return true;
  return readFilePiece();
}

// A regular file: read() needs no readiness (raw/subject.md:134). A short
// file (changed since stat) cannot match the Content-Length already sent.
// 일반 파일이라 read()에 준비 확인이 필요 없다 (raw/subject.md:134).
// stat 뒤에 바뀌어 짧아진 파일은 이미 보낸 Content-Length와 맞출 수 없다.
bool ResponseStream::readFilePiece() {
  char buffer[limits::kFileChunkSize];
  const std::size_t wanted = fileRemaining_ < limits::kFileChunkSize
                                 ? fileRemaining_
                                 : limits::kFileChunkSize;
  const ssize_t got = read(fileFd_, buffer, wanted);
  if (got <= 0) {
    closeFile();
    fileRemaining_ = 0;
    return false;
  }
  out_.assign(buffer, static_cast<std::size_t>(got));
  offset_ = 0;
  fileRemaining_ -= static_cast<std::size_t>(got);
  if (fileRemaining_ == 0) closeFile();
  return true;
}

// The descriptor is given up either way; a close() error changes nothing.
// 어느 쪽이든 디스크립터는 버린다. close()가 실패해도 달라지는 것이 없다.
void ResponseStream::closeFile() {
  if (fileFd_ >= 0) close(fileFd_);
  fileFd_ = -1;
}
