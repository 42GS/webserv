#include "route/Router.hpp"

#include <sys/stat.h>

#include <fstream>

#include "config/ConfigLoader.hpp"
#include "unit/TestConstants.hpp"
#include "unit/TestFramework.hpp"

// Test IDs (R-xx) refer to the test plan in the wiki (analysis/test-plan).

namespace {

// Relative to the repository root, where "make test" runs the tests.

std::string matchedPath(const ServerConfig& server, const std::string& path) {
  const LocationConfig* location = Router(server).matchLocation(path);
  return location == 0 ? "(none)" : location->path;
}

ServerConfig serverFrom(const std::string& text) {
  return loadConfigText(text, "test.conf").at(0);
}

Request requestFor(const std::string& method, const std::string& path,
                   const std::string& query = "") {
  Request request;
  request.method = method;
  request.path = path;
  request.query = query;
  return request;
}

void writeFile(const std::string& path, const std::string& content) {
  std::ofstream out(path.c_str());
  out << content;
}

// obj/router-www/{a.txt, dir/, idx/index.html, list/x.txt}. mkdir() results
// are not checked: the directories may exist from an earlier run, and a
// real failure shows up as failing checks.
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
}

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
         "  location /dir/ { limit_except GET POST DELETE { deny all; }"
         "    upload_store " +
         root + "/dir; } }";
}

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
  CHECK_EQ(static_cast<int>(RouteResult::DIRECTORY_LISTING),
           kindOf(router.route(requestFor("GET", "/old/../list/"))));
}

TEST_CASE(r03_rootAndAliasMapping) {
  makeTree();
  const ServerConfig server = serverFrom(treeConfig());
  const Router router(server);
  CHECK_EQ(std::string(router_test::kTree) + "/a.txt",
           router.route(requestFor("GET", "/ro/a.txt")).fsPath);
  std::string fsPath;
  CHECK(router.mapToFile("/idx/index.html", fsPath));
  CHECK_EQ(std::string(router_test::kTree) + "/idx/index.html", fsPath);
  CHECK(!router.mapToFile("/../x", fsPath));
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
  // "dir/" + "/index.html": the doubled slash names the same file.
  CHECK_EQ(std::string(router_test::kTree) + "/idx//index.html",
           router.route(requestFor("GET", "/k/index.html")).fsPath);
  CHECK_EQ(404, router.route(requestFor("GET", "/k../a.txt")).status);
  CHECK_EQ(404, router.route(requestFor("GET", "/kx")).status);
  std::string fsPath;
  CHECK(!router.mapToFile("/k../a.txt", fsPath));
  CHECK(router.locationFor("/k../a.txt") == 0);
  CHECK_EQ(std::string("(none)"), matchedPath(server, "/k../a.txt"));
}

TEST_CASE(f09_f12_d04_methodsOfALocation) {
  makeTree();
  const ServerConfig server = serverFrom(treeConfig());
  const Router router(server);
  // No limit_except: GET and HEAD only (decision 16).
  RouteResult result = router.route(requestFor("DELETE", "/a.txt"));
  CHECK_EQ(405, result.status);
  CHECK_EQ(std::string("GET, HEAD"), result.allow);
  // POST allowed but no upload_store: not answered, not in Allow.
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
  CHECK_EQ(std::string(router_test::kTree) + "/dir", result.fsPath);
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
