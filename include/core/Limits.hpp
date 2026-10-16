#ifndef CORE_LIMITS_HPP
#define CORE_LIMITS_HPP

#include <cstddef>
#include <ctime>

// Values to tune while testing (wiki analysis/design section 12). Body size
// limits of the configuration are in config/ConfigLimits.hpp.
namespace limits {

// Program
const char* const kDefaultConfigPath = "conf/default.conf";
const char* const kClockFile = ".webserv_clock";  // relative to the run dir
const int kMaxArgc = 2;  // program name and an optional configuration file

// Event loop
const int kPollTimeoutMs = 1000;  // how often timeouts are checked

// Request
const std::size_t kRecvBufferSize = 65536;     // one recv()
const std::size_t kMaxRequestLine = 8192;      // RFC 9112 3: at least 8000
const std::size_t kMaxHeaderBytes = 16384;     // more is 431
const std::size_t kMaxChunkLine = 4096;        // chunk-size line, more is 400
const std::time_t kHeaderTimeoutSeconds = 30;  // whole head
const std::time_t kBodyTimeoutSeconds = 30;    // without input
const std::time_t kSendTimeoutSeconds = 30;    // without progress

// Routing
const int kMaxInternalRedirects = 10;  // as NGINX (design decision 6)

// Files
const std::size_t kInMemoryFileLimit = 1024 * 1024;  // larger is streamed
const std::size_t kFileChunkSize = 65536;  // one read() of a streamed file
const std::size_t kFileReadSize = 65536;   // one read() of a whole file
const std::size_t kMaxErrorPageSize = 1024 * 1024;

}  // namespace limits

#endif
