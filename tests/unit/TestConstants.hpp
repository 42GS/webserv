/**
 * @file  TestConstants.hpp
 * @brief Constants of the unit tests, one namespace per test file.
 *        단위 테스트의 상수. 테스트 파일마다 namespace 하나.
 *
 * Paths are relative to the repository root, where "make test" runs the
 * tests.
 * 경로는 "make test"가 테스트를 실행하는 저장소 루트 기준이다.
 */
#ifndef TESTS_UNIT_TESTCONSTANTS_HPP
#define TESTS_UNIT_TESTCONSTANTS_HPP

#include <sys/types.h>

#include <cstddef>

/** @brief Paths shared by test files. / 테스트 파일이 함께 쓰는 경로. */
namespace test_paths {

/**
 * @brief Written by Clock in clock_test.cpp and event_loop_test.cpp.
 *        clock_test.cpp와 event_loop_test.cpp에서 Clock이 쓴다.
 */
const char* const kClockFile = "obj/.test_clock";

}  // namespace test_paths

/**
 * @brief Constants of clock_test.cpp.
 *        clock_test.cpp의 상수.
 */
namespace clock_test {

/**
 * @brief A path whose directory does not exist.
 *        디렉토리가 없는 경로.
 */
const char* const kUnwritableClockPath = "/nonexistent/dir/clock";

}  // namespace clock_test

/**
 * @brief Constants of config_test.cpp.
 *        config_test.cpp의 상수.
 */
namespace config_test {

/**
 * @brief The example configuration of design section 3.
 *        설계 3절의 설정 예시.
 */
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

/**
 * @brief Constants of fs_test.cpp.
 *        fs_test.cpp의 상수.
 */
namespace fs_test {

/** File the tests write. / 테스트가 쓰는 파일. */
const char* const kFile = "obj/fs-test.txt";
/** Its content. / 그 내용. */
const char* const kContent = "0123456789";
/** Length of kContent. / kContent의 길이. */
const std::size_t kContentSize = 10;

}  // namespace fs_test

/**
 * @brief Constants of path_resolver_test.cpp.
 *        path_resolver_test.cpp의 상수.
 */
namespace path_resolver_test {

/**
 * @brief Result text of a rejected path.
 *        거부한 경로의 결과 문자열.
 */
const char* const kRejected = "(400)";

}  // namespace path_resolver_test

/**
 * @brief Constants of request_parser_test.cpp.
 *        request_parser_test.cpp의 상수.
 */
namespace request_parser_test {

/** Body size limit given to the parser. / 파서에 주는 본문 크기 상한. */
const std::size_t kBodyLimit = 100;
/**
 * @brief statusOf(): the parser wants more bytes.
 *        statusOf(): 파서가 바이트를 더 기다린다.
 */
const int kIncomplete = -1;
/** statusOf(): a complete request. / statusOf(): 끝난 요청. */
const int kParsed = 0;

}  // namespace request_parser_test

/**
 * @brief Constants of response_stream_test.cpp.
 *        response_stream_test.cpp의 상수.
 */
namespace response_stream_test {

/** File sent in pieces. / 나눠 보내는 파일. */
const char* const kBigFile = "obj/stream-test.bin";
/** Several 64 KiB pieces. / 64 KiB 조각 여러 개. */
const std::size_t kBigFileSize = 200000;
/** A short send() each time. / 매번 짧은 send(). */
const std::size_t kSendSize = 1000;

}  // namespace response_stream_test

/**
 * @brief Constants of router_test.cpp.
 *        router_test.cpp의 상수.
 */
namespace router_test {

/** Root of the test tree. / 테스트 트리의 루트. */
const char* const kTree = "obj/router-www";
/** Mode of its directories. / 그 디렉토리의 권한. */
const mode_t kDirectoryMode = 0755;

}  // namespace router_test

#endif
