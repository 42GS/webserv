#include "http/ResponseStream.hpp"

#include <fcntl.h>
#include <unistd.h>

#include <sstream>

#include "core/Limits.hpp"
#include "http/HeaderNames.hpp"
#include "http/StatusCodes.hpp"
#include "http/StatusText.hpp"

namespace {

// RFC 9112 section 11.1: no CR or LF may reach the header section from
// anywhere but this function, so a field value holding one is dropped.
bool isSafeFieldValue(const std::string& value) {
  return value.find_first_of("\r\n") == std::string::npos;
}

std::string serializeHead(const Response& response) {
  std::ostringstream head;
  // The SP after the code stays even without a reason (RFC 9112 4).
  head << "HTTP/1.1 " << response.status << " " << reasonPhrase(response.status)
       << "\r\n";
  for (std::size_t i = 0; i < response.headers.size(); ++i) {
    const HeaderMap::Field& field = response.headers.at(i);
    if (isSafeFieldValue(field.second))
      head << field.first << ": " << field.second << "\r\n";
  }
  // Never for 204 (RFC 9110 8.6); for HEAD it is the GET value.
  if (response.status != http_status::kNoContent) {
    const std::size_t length = response.bodyFile.empty()
                                   ? response.body.size()
                                   : response.bodyFileSize;
    head << header_name::kContentLength << ": " << length << "\r\n";
  }
  head << "\r\n";
  return head.str();
}

}  // namespace

ResponseStream::ResponseStream()
    : offset_(0), fileFd_(response_stream::kNoFile), fileRemaining_(0) {}

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
void ResponseStream::closeFile() {
  if (fileFd_ != response_stream::kNoFile) close(fileFd_);
  fileFd_ = response_stream::kNoFile;
}
