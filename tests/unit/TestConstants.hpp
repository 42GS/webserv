#ifndef TESTS_UNIT_TESTCONSTANTS_HPP
#define TESTS_UNIT_TESTCONSTANTS_HPP

#include <sys/types.h>

#include <cstddef>

// Constants of the unit tests, one namespace per test file. Paths are
// relative to the repository root, where "make test" runs the tests.

namespace test_paths {

// Written by Clock in clock_test.cpp and event_loop_test.cpp.
const char* const kClockFile = "obj/.test_clock";

}  // namespace test_paths

namespace clock_test {

const char* const kUnwritableClockPath = "/nonexistent/dir/clock";

}  // namespace clock_test

namespace config_test {

// The example configuration of design section 3.
const char* const kDesignExample =
    "server {\n"
    "    listen 127.0.0.1:8080;\n"
    "    root /var/www/site;\n"
    "    index index.html;\n"
    "    error_page 404 /errors/404.html;\n"
    "    client_max_body_size 1m;\n"
    "\n"
    "    location / {\n"
    "        limit_except GET { deny all; }\n"
    "    }\n"
    "    location /upload/ {\n"
    "        limit_except GET POST DELETE { deny all; }\n"
    "        upload_store /var/www/site/upload;\n"
    "        client_max_body_size 10m;\n"
    "    }\n"
    "    location /cgi-bin/ {\n"
    "        cgi .php /usr/bin/php-cgi;\n"
    "        cgi .py /usr/bin/python3;\n"
    "    }\n"
    "    location /kapouet/ {\n"
    "        alias /tmp/www/;   # subject example\n"
    "    }\n"
    "    location /old/ {\n"
    "        return 301 /new/;\n"
    "    }\n"
    "}\n";

}  // namespace config_test

namespace fs_test {

const char* const kFile = "obj/fs-test.txt";
const char* const kContent = "0123456789";
const std::size_t kContentSize = 10;

}  // namespace fs_test

namespace path_resolver_test {

const char* const kRejected = "(400)";

}  // namespace path_resolver_test

namespace request_parser_test {

const std::size_t kBodyLimit = 100;
const int kIncomplete = -1;  // statusOf(): the parser wants more bytes
const int kParsed = 0;       // statusOf(): a complete request

}  // namespace request_parser_test

namespace response_stream_test {

const char* const kBigFile = "obj/stream-test.bin";
const std::size_t kBigFileSize = 200000;  // several 64 KiB pieces
const std::size_t kSendSize = 1000;       // a short send() each time

}  // namespace response_stream_test

namespace router_test {

const char* const kTree = "obj/router-www";
const mode_t kDirectoryMode = 0755;

}  // namespace router_test

#endif
