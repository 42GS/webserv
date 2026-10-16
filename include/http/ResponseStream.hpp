#ifndef HTTP_RESPONSESTREAM_HPP
#define HTTP_RESPONSESTREAM_HPP

#include <cstddef>
#include <string>

#include "http/Response.hpp"

namespace response_stream {

const int kNoFile = -1;  // fileFd_ when no file is open

}  // namespace response_stream

// Serialized response handed to send() a piece at a time. A file body is
// read in kFileChunkSize pieces as the previous piece is sent, so a large
// file never stalls the loop (raw/subject.md:134 exempts regular files
// from poll(), design 8). Owns the file descriptor while reading.
class ResponseStream {
 public:
  ResponseStream();
  ~ResponseStream();

  // Starts a response. False if the body file cannot be opened.
  bool reset(const Response& response);
  bool hasPending() const;
  const char* data() const;
  std::size_t size() const;
  // Marks n bytes as sent and reads the next file piece when needed.
  // False if the file could not be read: the response cannot be finished
  // and the connection must close.
  bool consume(std::size_t n);

 private:
  ResponseStream(const ResponseStream&);
  ResponseStream& operator=(const ResponseStream&);

  bool readFilePiece();
  void closeFile();

  std::string out_;
  std::size_t offset_;
  int fileFd_;
  std::size_t fileRemaining_;
};

#endif
