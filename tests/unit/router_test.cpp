#include "http/Router.hpp"

#include "config/ConfigLoader.hpp"
#include "unit/TestFramework.hpp"

namespace {

std::string matchedPath(const ServerConfig& server, const std::string& path) {
  const LocationConfig* location = Router(server).matchLocation(path);
  return location == 0 ? "(none)" : location->path;
}

}  // namespace

TEST_CASE(routerPicksTheLongestPrefix) {
  const ServerConfig server =
      loadConfigText(
          "server { listen 8080; root /r; location /a/ { } "
          "location /a/b/ { } location /ab { } }",
          "test.conf")
          .at(0);
  CHECK_EQ(std::string("/a/b/"), matchedPath(server, "/a/b/c"));
  CHECK_EQ(std::string("/a/"), matchedPath(server, "/a/x"));
  CHECK_EQ(std::string("/ab"), matchedPath(server, "/abc"));
  CHECK_EQ(std::string("/"), matchedPath(server, "/z"));
  CHECK_EQ(std::string("/"), matchedPath(server, "/a"));
}

TEST_CASE(routerFindsNothingWithoutRootLocation) {
  const ServerConfig server =
      loadConfigText("server { listen 8080; location /a/ { return 301 /; } }",
                     "test.conf")
          .at(0);
  CHECK_EQ(std::string("(none)"), matchedPath(server, "/b"));
}
