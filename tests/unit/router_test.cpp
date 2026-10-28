/**
 * @file  router_test.cpp
 * @brief Unit tests of Router.
 *        Router의 단위 테스트.
 *
 * Test IDs (R-xx) refer to the test plan in the wiki
 * (analysis/test-plan).
 * 테스트 ID(R-xx)는 위키의 테스트 계획(analysis/test-plan)을 가리킨다.
 */
#include "route/Router.hpp"

#include <sys/stat.h>

#include <fstream>

#include "config/ConfigLoader.hpp"
#include "unit/TestConstants.hpp"
#include "unit/TestFramework.hpp"

namespace {

/**
 * @brief The location a path matches.
 *        경로가 고르는 location.
 * @param[in] server Server configuration. / server 설정.
 * @param[in] path   Normalized path. / 정규화한 경로.
 * @return Its path, or "(none)". / 그 경로, 또는 "(none)".
 */
std::string matchedPath(const ServerConfig& server, const std::string& path) {
  const LocationConfig* location = Router(server).matchLocation(path);
  return location == 0 ? "(none)" : location->path;
}

/**
 * @brief The first server of a configuration text.
 *        설정 텍스트의 첫 server.
 * @param[in] text Configuration. / 설정.
 * @return The server. / server.
 * @throws ConfigError If the text is invalid. / 텍스트가 잘못됐으면.
 */
ServerConfig serverFrom(const std::string& text) {
  return loadConfigText(text, "test.conf").at(0);
}

/**
 * @brief A parsed request with only the given parts.
 *        주어진 부분만 있는 해석된 요청.
 * @param[in] method Method. / 메서드.
 * @param[in] path   Encoded path. / 인코딩된 경로.
 * @param[in] query  Query. / query.
 * @return The request. / 요청.
 */
Request requestFor(const std::string& method, const std::string& path,
                   const std::string& query = "") {
  Request request;
  request.method = method;
  request.path = path;
  request.query = query;
  return request;
}

/**
 * @brief Writes a file, replacing it.
 *        파일을 덮어쓴다.
 * @param[in] path    File. / 파일.
 * @param[in] content Content. / 내용.
 */
void writeFile(const std::string& path, const std::string& content) {
  std::ofstream out(path.c_str());
  out << content;
}

/**
 * @brief Creates obj/router-www/{a.txt, dir/, idx/index.html, list/x.txt,
 *        cgi/run.py, cgi/index.py, cgi/sub.py/}.
 *        obj/router-www/{a.txt, dir/, idx/index.html, list/x.txt,
 *        cgi/run.py, cgi/index.py, cgi/sub.py/}를 만든다.
 *
 * mkdir() results are not checked: the directories may exist from an
 * earlier run, and a real failure shows up as failing checks.
 * mkdir() 결과는 검사하지 않는다. 디렉토리가 이전 실행에서 남아 있을 수
 * 있고, 진짜 실패는 검사 실패로 드러난다.
 */
void makeTree() {
  const std::string root(router_test::kTree);
  static_cast<void>(mkdir(root.c_str(), router_test::kDirectoryMode));
  static_cast<void>(
      mkdir((root + "/dir").c_str(), router_test::kDirectoryMode));
  static_cast<void>(
      mkdir((root + "/idx").c_str(), router_test::kDirectoryMode));
  static_cast<void>(
      mkdir((root + "/list").c_str(), router_test::kDirectoryMode));
  writeFile(root + "/a.txt", "a");
  writeFile(root + "/idx/index.html", "index");
  writeFile(root + "/list/x.txt", "x");
  static_cast<void>(
      mkdir((root + "/cgi").c_str(), router_test::kDirectoryMode));
  static_cast<void>(
      mkdir((root + "/cgi/sub.py").c_str(), router_test::kDirectoryMode));
  writeFile(root + "/cgi/run.py", "print()");
  writeFile(root + "/cgi/index.py", "print()");
}

/**
 * @brief A server over the tree of makeTree().
 *        makeTree()의 트리를 쓰는 server.
 * @return Configuration text. / 설정 텍스트.
 */
std::string treeConfig() {
  const std::string root(router_test::kTree);
  return "server { listen 8080; root " + root +
         "; index index.html;"
         "  location /list/ { autoindex on; }"
         "  location /old/ { return 301 /new/; }"
         "  location /ro/ { alias " +
         root +
         "/; limit_except GET { deny all; } }"
         "  location /post-only/ { limit_except POST { deny all; } }"
         "  location /cgi/ { limit_except GET POST { deny all; }"
         "    index index.py; cgi .py /usr/bin/python3; }"
         "  location /dir/ { limit_except GET POST DELETE { deny all; }"
         "    upload_store " +
         root + "/dir; } }";
}

/**
 * @brief The kind as an int, for CHECK_EQ.
 *        CHECK_EQ에 쓰도록 int로 바꾼 종류.
 * @param[in] result Decision. / 결정.
 * @return The kind. / 종류.
 */
int kindOf(const RouteResult& result) { return static_cast<int>(result.kind); }

}  // namespace

TEST_CASE(routerPicksTheLongestPrefix) {
  const ServerConfig server = serverFrom(
      "server { listen 8080; root /r; location /a/ { } "
      "location /a/b/ { } location /ab { } }");
  CHECK_EQ(std::string("/a/b/"), matchedPath(server, "/a/b/c"));
  CHECK_EQ(std::string("/a/"), matchedPath(server, "/a/x"));
  CHECK_EQ(std::string("/ab"), matchedPath(server, "/abc"));
  CHECK_EQ(std::string("/"), matchedPath(server, "/z"));
  CHECK_EQ(std::string("/"), matchedPath(server, "/a"));
}

TEST_CASE(routerFindsNothingWithoutRootLocation) {
  const ServerConfig server =
      serverFrom("server { listen 8080; location /a/ { return 301 /; } }");
  CHECK_EQ(std::string("(none)"), matchedPath(server, "/b"));
  CHECK_EQ(404, Router(server).route(requestFor("GET", "/b")).status);
}

TEST_CASE(r01_r02_routesTheNormalizedPath) {
  makeTree();
  const ServerConfig server = serverFrom(treeConfig());
  const Router router(server);
  CHECK_EQ(400, router.route(requestFor("GET", "/../a.txt")).status);
  CHECK_EQ(400, router.route(requestFor("GET", "/a%2Fb")).status);
  const RouteResult file = router.route(requestFor("GET", "/x/../a%2etxt"));
  CHECK_EQ(static_cast<int>(RouteResult::STATIC_FILE), kindOf(file));
  CHECK_EQ(std::string(router_test::kTree) + "/a.txt", file.fsPath);
  // "/old/../list/" is "/list/" once normalized, not the /old/ location.
  // "/old/../list/"는 정규화하면 "/list/"이고, /old/ location이 아니다.
  CHECK_EQ(static_cast<int>(RouteResult::DIRECTORY_LISTING),
           kindOf(router.route(requestFor("GET", "/old/../list/"))));
}

TEST_CASE(r03_rootAndAliasMapping) {
  makeTree();
  const ServerConfig server = serverFrom(treeConfig());
  const Router router(server);
  CHECK_EQ(std::string(router_test::kTree) + "/a.txt",
           router.route(requestFor("GET", "/ro/a.txt")).fsPath);
  CHECK_EQ(std::string(router_test::kTree) + "/idx/index.html",
           router.mapToFile("/idx/index.html"));
  CHECK_EQ(std::string(""), router.mapToFile("/../x"));
}

TEST_CASE(r04_returnRedirects) {
  const ServerConfig server = serverFrom(treeConfig());
  const RouteResult result = Router(server).route(requestFor("GET", "/old/x"));
  CHECK_EQ(static_cast<int>(RouteResult::REDIRECT), kindOf(result));
  CHECK_EQ(301, result.status);
  CHECK_EQ(std::string("/new/"), result.redirectTo);
}

TEST_CASE(r05_r19_methodNotAllowedListsAllow) {
  makeTree();
  const ServerConfig server = serverFrom(treeConfig());
  const Router router(server);
  RouteResult result = router.route(requestFor("DELETE", "/ro/a.txt"));
  CHECK_EQ(405, result.status);
  CHECK_EQ(std::string("GET, HEAD"), result.allow);
  result = router.route(requestFor("POST", "/a.txt"));
  CHECK_EQ(405, result.status);
  CHECK_EQ(std::string("GET, HEAD"), result.allow);
  result = router.route(requestFor("GET", "/post-only/a.txt"));
  CHECK_EQ(405, result.status);
  CHECK_EQ(std::string(""), result.allow);
  CHECK_EQ(static_cast<int>(RouteResult::STATIC_FILE),
           kindOf(router.route(requestFor("HEAD", "/ro/a.txt"))));
}

TEST_CASE(r08_missingFileIs404) {
  makeTree();
  const ServerConfig server = serverFrom(treeConfig());
  CHECK_EQ(404, Router(server).route(requestFor("GET", "/nope.txt")).status);
}

TEST_CASE(r10_r11_r12_r13_directories) {
  makeTree();
  const ServerConfig server = serverFrom(treeConfig());
  const Router router(server);
  RouteResult result = router.route(requestFor("GET", "/list", "a=1"));
  CHECK_EQ(301, result.status);
  CHECK_EQ(std::string("/list/?a=1"), result.redirectTo);
  result = router.route(requestFor("GET", "/idx/"));
  CHECK_EQ(static_cast<int>(RouteResult::STATIC_FILE), kindOf(result));
  CHECK_EQ(std::string(router_test::kTree) + "/idx/index.html", result.fsPath);
  result = router.route(requestFor("GET", "/list/"));
  CHECK_EQ(static_cast<int>(RouteResult::DIRECTORY_LISTING), kindOf(result));
  CHECK_EQ(std::string("/list/"), result.urlPath);
  CHECK_EQ(403, router.route(requestFor("GET", "/dir/")).status);
}

TEST_CASE(r02_locationForNormalizesFirst) {
  const ServerConfig server = serverFrom(treeConfig());
  const Router router(server);
  const LocationConfig* location = router.locationFor("/old/../list/x");
  CHECK(location != 0);
  if (location != 0) CHECK_EQ(std::string("/list/"), location->path);
  CHECK(router.locationFor("/../x") == 0);
}

TEST_CASE(r20_aliasCoversOnlyWholeSegments) {
  makeTree();
  const ServerConfig server = serverFrom(
      std::string("server { listen 8080; root ") + router_test::kTree +
      "; location /k { alias " + router_test::kTree + "/idx/; } }");
  const Router router(server);
  // The alias ends in "/" and the rest of the path starts with one; the
  // doubled slash names the same file.
  // alias가 "/"로 끝나고 경로의 나머지도 "/"로 시작한다. 겹친 슬래시도 같은
  // 파일을 가리킨다.
  CHECK_EQ(std::string(router_test::kTree) + "/idx//index.html",
           router.route(requestFor("GET", "/k/index.html")).fsPath);
  CHECK_EQ(404, router.route(requestFor("GET", "/k../a.txt")).status);
  CHECK_EQ(404, router.route(requestFor("GET", "/kx")).status);
  CHECK_EQ(std::string(""), router.mapToFile("/k../a.txt"));
  CHECK(router.locationFor("/k../a.txt") == 0);
  CHECK_EQ(std::string("(none)"), matchedPath(server, "/k../a.txt"));
}

TEST_CASE(f09_f12_d04_methodsOfALocation) {
  makeTree();
  const ServerConfig server = serverFrom(treeConfig());
  const Router router(server);
  // No limit_except: GET and HEAD only (decision 16).
  // limit_except가 없으면 GET과 HEAD만 받는다 (결정 16).
  RouteResult result = router.route(requestFor("DELETE", "/a.txt"));
  CHECK_EQ(405, result.status);
  CHECK_EQ(std::string("GET, HEAD"), result.allow);
  // POST allowed but no upload_store: not answered, not in Allow.
  // POST를 허용했지만 upload_store가 없으면 답하지 않고 Allow에도 없다.
  result = router.route(requestFor("POST", "/post-only/a.txt"));
  CHECK_EQ(405, result.status);
  CHECK_EQ(std::string(""), result.allow);
  result = router.route(requestFor("PUT", "/dir/x"));
  CHECK_EQ(std::string("GET, HEAD, POST, DELETE"), result.allow);
}

TEST_CASE(f01_postToAnUploadLocation) {
  makeTree();
  const ServerConfig server = serverFrom(treeConfig());
  const RouteResult result =
      Router(server).route(requestFor("POST", "/dir/sub/../"));
  CHECK_EQ(static_cast<int>(RouteResult::UPLOAD), kindOf(result));
  CHECK_EQ(std::string(router_test::kTree) + "/dir", result.uploadStore);
  CHECK_EQ(std::string("/dir/"), result.urlPath);
}

TEST_CASE(d02_d03_deleteDecisions) {
  makeTree();
  writeFile(std::string(router_test::kTree) + "/dir/gone.txt", "g");
  const ServerConfig server = serverFrom(treeConfig());
  const Router router(server);
  const RouteResult result =
      router.route(requestFor("DELETE", "/dir/gone.txt"));
  CHECK_EQ(static_cast<int>(RouteResult::DELETE), kindOf(result));
  CHECK_EQ(std::string(router_test::kTree) + "/dir/gone.txt", result.fsPath);
  CHECK_EQ(404, router.route(requestFor("DELETE", "/dir/none.txt")).status);
  CHECK_EQ(403, router.route(requestFor("DELETE", "/dir/")).status);
  CHECK_EQ(400, router.route(requestFor("DELETE", "/dir/../../x")).status);
}

TEST_CASE(g01_g09_g10_scripts) {
  makeTree();
  const ServerConfig server = serverFrom(treeConfig());
  const Router router(server);
  RouteResult result = router.route(requestFor("GET", "/cgi/run.py/a/b%20c"));
  CHECK_EQ(static_cast<int>(RouteResult::CGI), kindOf(result));
  CHECK_EQ(std::string(router_test::kTree) + "/cgi/run.py", result.fsPath);
  CHECK_EQ(std::string("/usr/bin/python3"), result.interpreter);
  CHECK_EQ(std::string("/cgi/run.py"), result.scriptName);
  CHECK_EQ(std::string("/a/b c"), result.pathInfo);
  CHECK_EQ(std::string(router_test::kTree) + "/a/b c", result.pathTranslated);
  // POST to a script needs no upload_store.
  // 스크립트로 가는 POST에는 upload_store가 필요 없다.
  CHECK_EQ(static_cast<int>(RouteResult::CGI),
           kindOf(router.route(requestFor("POST", "/cgi/run.py"))));
  // A directory named like a script is not one; the index is (decision 6).
  // 스크립트 이름을 가진 디렉토리는 스크립트가 아니다. index는 스크립트다
  // (결정 6).
  result = router.route(requestFor("GET", "/cgi/sub.py/run.py"));
  CHECK_EQ(404, result.status);
  result = router.route(requestFor("GET", "/cgi/"));
  CHECK_EQ(static_cast<int>(RouteResult::CGI), kindOf(result));
  CHECK_EQ(std::string("/cgi/index.py"), result.scriptName);
  result = router.route(requestFor("DELETE", "/cgi/run.py"));
  CHECK_EQ(405, result.status);
  CHECK_EQ(std::string("GET, HEAD, POST"), result.allow);
  CHECK_EQ(404, router.route(requestFor("GET", "/cgi/none.py")).status);
}
