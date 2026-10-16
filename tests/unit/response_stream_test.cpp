#include <fstream>
#include <string>

#include "http/ResponseStream.hpp"
#include "unit/TestFramework.hpp"

namespace {

// Relative to the repository root, where "make test" runs the tests.
const char* const kBigFile = "obj/stream-test.bin";
const std::size_t kBigFileSize = 200000;  // several 64 KiB pieces
const std::size_t kSendSize = 1000;       // a short send() each time

// Sends everything the way Connection does: a short write each time.
std::string drain(ResponseStream& stream) {
  std::string sent;
  while (stream.hasPending()) {
    const std::size_t n = stream.size() < kSendSize ? stream.size() : kSendSize;
    sent.append(stream.data(), n);
    if (!stream.consume(n)) return sent + "(read failed)";
  }
  return sent;
}

Response textResponse(int status, const std::string& body) {
  Response response;
  response.status = status;
  response.headers.add("Content-Type", "text/plain");
  response.body = body;
  return response;
}

}  // namespace

TEST_CASE(responseHeadAndMemoryBody) {
  ResponseStream stream;
  CHECK(stream.reset(textResponse(200, "hello")));
  CHECK_EQ(std::string("HTTP/1.1 200 OK\r\n"
                       "Content-Type: text/plain\r\n"
                       "Content-Length: 5\r\n"
                       "\r\n"
                       "hello"),
           drain(stream));
}

TEST_CASE(r07_headKeepsContentLengthWithoutBody) {
  Response response = textResponse(200, "hello");
  response.omitBody = true;
  ResponseStream stream;
  CHECK(stream.reset(response));
  CHECK_EQ(std::string("HTTP/1.1 200 OK\r\n"
                       "Content-Type: text/plain\r\n"
                       "Content-Length: 5\r\n"
                       "\r\n"),
           drain(stream));
}

TEST_CASE(responseNoContentHasNoContentLength) {
  Response response;
  response.status = 204;
  ResponseStream stream;
  CHECK(stream.reset(response));
  CHECK_EQ(std::string("HTTP/1.1 204 No Content\r\n\r\n"), drain(stream));
}

TEST_CASE(responseDropsFieldsWithLineBreaks) {
  Response response = textResponse(302, "");
  response.headers.add("Location", "/a\r\nSet-Cookie: x=1");
  ResponseStream stream;
  CHECK(stream.reset(response));
  CHECK(drain(stream).find("Set-Cookie") == std::string::npos);
}

TEST_CASE(r16_streamsLargeFileInPieces) {
  std::string content;
  for (std::size_t i = 0; i < kBigFileSize; ++i)
    content += static_cast<char>('a' + i % 26);
  {
    std::ofstream out(kBigFile, std::ios::binary);
    out << content;
  }
  Response response;
  response.status = 200;
  response.bodyFile = kBigFile;
  response.bodyFileSize = kBigFileSize;
  ResponseStream stream;
  CHECK(stream.reset(response));
  const std::string sent = drain(stream);
  const std::string::size_type bodyStart = sent.find("\r\n\r\n") + 4;
  CHECK_EQ(std::string("HTTP/1.1 200 OK\r\nContent-Length: 200000\r\n\r\n"),
           sent.substr(0, bodyStart));
  CHECK(sent.substr(bodyStart) == content);
}

TEST_CASE(responseReportsMissingAndShrunkFiles) {
  Response response;
  response.status = 200;
  response.bodyFile = "obj/no-such-file";
  response.bodyFileSize = 10;
  ResponseStream stream;
  CHECK(!stream.reset(response));
  {
    std::ofstream out(kBigFile, std::ios::binary);
    out << "short";
  }
  response.bodyFile = kBigFile;
  CHECK(stream.reset(response));
  const std::string sent = drain(stream);
  CHECK(sent.find("(read failed)") != std::string::npos);
}
