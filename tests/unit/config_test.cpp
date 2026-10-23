/**
 * @file  config_test.cpp
 * @brief Unit tests of the configuration loader.
 *        설정 로더의 단위 테스트.
 *
 * Test IDs (C-xx, G-xx) refer to the test plan in the wiki
 * (analysis/test-plan).
 * 테스트 ID(C-xx, G-xx)는 위키의 테스트 계획(analysis/test-plan)을 가리킨다.
 */
#include <map>
#include <stdexcept>

#include "config/ConfigError.hpp"
#include "config/ConfigLimits.hpp"
#include "config/ConfigLoader.hpp"
#include "unit/TestConstants.hpp"
#include "unit/TestFramework.hpp"

namespace {

/**
 * @brief loadConfigText() with the name "test.conf".
 *        이름을 "test.conf"로 한 loadConfigText().
 * @param[in] text Configuration. / 설정.
 * @return The servers. / server 목록.
 * @throws ConfigError If the text is invalid. / 텍스트가 잘못됐으면.
 */
std::vector<ServerConfig> load(const std::string& text) {
  return loadConfigText(text, "test.conf");
}

/**
 * @brief The error of loading a text.
 *        텍스트를 읽을 때의 오류.
 * @param[in] text Configuration. / 설정.
 * @return The ConfigError message, or "" if loading succeeded.
 *         ConfigError 메시지. 읽기에 성공하면 "".
 */
std::string loadError(const std::string& text) {
  try {
    load(text);
  } catch (const ConfigError& error) {
    return error.what();
  }
  return "";
}

/**
 * @brief The value of a key.
 *        키의 값.
 *
 * Throws instead of dereferencing end(), so a missing key fails only the
 * current test.
 * end()를 역참조하는 대신 예외를 던져서, 없는 키는 지금 테스트만 실패시킨다.
 *
 * @param[in] values Map. / map.
 * @param[in] key    Key. / 키.
 * @return The value. / 값.
 * @throws std::out_of_range If the key is missing. / 키가 없으면.
 */
template <typename Key>
std::string valueAt(const std::map<Key, std::string>& values, const Key& key) {
  const typename std::map<Key, std::string>::const_iterator it =
      values.find(key);
  if (it == values.end()) throw std::out_of_range("key not found");
  return it->second;
}

/**
 * @brief True if part occurs in text.
 *        text 안에 part가 있으면 true.
 * @param[in] text Text. / 텍스트.
 * @param[in] part Part to find. / 찾을 부분.
 * @return Whether it occurs. / 있는지.
 */
bool contains(const std::string& text, const std::string& part) {
  return text.find(part) != std::string::npos;
}

/**
 * @brief The location with exactly this path.
 *        경로가 정확히 같은 location.
 * @param[in] server Server. / server.
 * @param[in] path   Location path. / location 경로.
 * @return The location, or 0. / location, 또는 0.
 */
const LocationConfig* findLocation(const ServerConfig& server,
                                   const std::string& path) {
  for (std::size_t i = 0; i < server.locations.size(); ++i) {
    if (server.locations[i].path == path) return &server.locations[i];
  }
  return 0;
}

}  // namespace

TEST_CASE(parsesDesignExample) {
  const std::vector<ServerConfig> servers = load(config_test::kDesignExample);
  CHECK_EQ(1u, servers.size());
  const ServerConfig& server = servers.at(0);
  CHECK_EQ(1u, server.listens.size());
  CHECK_EQ(std::string("127.0.0.1"), server.listens.at(0).host);
  CHECK_EQ(8080, server.listens.at(0).port);
  CHECK_EQ(std::string("/errors/404.html"), valueAt(server.errorPages, 404));
  CHECK_EQ(5u, server.locations.size());

  const LocationConfig* root = findLocation(server, "/");
  CHECK(root != 0);
  if (root != 0) {
    CHECK_EQ(2u, root->allowedMethods.size());  // GET and the implied HEAD
    CHECK(root->allowedMethods.count("HEAD") == 1);
    CHECK_EQ(std::string("/var/www/site"), root->root);
  }
  const LocationConfig* upload = findLocation(server, "/upload/");
  CHECK(upload != 0);
  if (upload != 0) {
    CHECK_EQ(std::string("/var/www/site/upload"), upload->uploadStore);
    CHECK_EQ(static_cast<std::size_t>(10 * 1024 * 1024), upload->maxBodySize);
  }
  const LocationConfig* cgi = findLocation(server, "/cgi-bin/");
  CHECK(cgi != 0);
  if (cgi != 0) {
    CHECK_EQ(2u, cgi->cgiByExtension.size());
    CHECK_EQ(std::string("/usr/bin/php-cgi"),
             valueAt(cgi->cgiByExtension, std::string(".php")));
    // No limit_except: GET and HEAD only (decision 16).
    // limit_except가 없으면 GET과 HEAD만 받는다 (결정 16).
    CHECK_EQ(2u, cgi->allowedMethods.size());
    CHECK(cgi->allowedMethods.count("POST") == 0);
  }
  const LocationConfig* kapouet = findLocation(server, "/kapouet/");
  CHECK(kapouet != 0);
  if (kapouet != 0) {
    CHECK_EQ(std::string("/tmp/www/"), kapouet->alias);
    CHECK(kapouet->root.empty());
  }
  const LocationConfig* old = findLocation(server, "/old/");
  CHECK(old != 0);
  if (old != 0) {
    CHECK_EQ(301, old->returnCode);
    CHECK_EQ(std::string("/new/"), old->returnUrl);
  }
}

TEST_CASE(locationsAreSortedLongestFirst) {
  const ServerConfig server = load(config_test::kDesignExample).at(0);
  for (std::size_t i = 1; i < server.locations.size(); ++i)
    CHECK(server.locations[i - 1].path.size() >=
          server.locations[i].path.size());
}

TEST_CASE(locationInheritsServerSettings) {
  const ServerConfig server =
      load(
          "server { listen 8080; root /srv; index a.html b.html; autoindex on; "
          "client_max_body_size 2k;"
          "  location /x/ { } }")
          .at(0);
  const LocationConfig* x = findLocation(server, "/x/");
  CHECK(x != 0);
  if (x != 0) {
    CHECK_EQ(std::string("/srv"), x->root);
    CHECK_EQ(2u, x->indexFiles.size());
    CHECK(x->autoindex);
    CHECK_EQ(static_cast<std::size_t>(2048), x->maxBodySize);
  }
}

TEST_CASE(serverRootCreatesImplicitRootLocation) {
  const ServerConfig server =
      load("server { listen 8080; root /srv; location /x/ { } }").at(0);
  const LocationConfig* root = findLocation(server, "/");
  CHECK(root != 0);
  if (root != 0) CHECK_EQ(std::string("/srv"), root->root);
}

TEST_CASE(defaultsWhenNotConfigured) {
  const ServerConfig server = load("server { listen 8080; root /srv; }").at(0);
  CHECK_EQ(std::string("0.0.0.0"), server.listens.at(0).host);
  const LocationConfig& root = server.locations.at(0);
  CHECK_EQ(1u, root.indexFiles.size());
  CHECK_EQ(std::string("index.html"), root.indexFiles.at(0));
  CHECK(!root.autoindex);
  CHECK_EQ(config_limits::kDefaultMaxBodySize, root.maxBodySize);
}

TEST_CASE(acceptsSeparateServersOnDifferentPorts) {
  const std::vector<ServerConfig> servers = load(
      "server { listen 127.0.0.1:8080; root /a; }\n"
      "server { listen 127.0.0.1:8081; root /b; }\n"
      "server { listen 127.0.0.2:8080; root /c; }\n");
  CHECK_EQ(3u, servers.size());
}

TEST_CASE(rejectsSyntaxErrors) {
  CHECK(contains(loadError("server { listen 8080 }"), "not terminated"));
  CHECK(contains(loadError("server { listen 8080;"), "not closed"));
  CHECK(contains(loadError("server { listen 8080; } }"), "unexpected \"}\""));
  CHECK(contains(loadError("server { root \"/a b\"; }"), "quoted strings"));
  CHECK(contains(loadError(""), "no \"server\" block"));
}

TEST_CASE(rejectsBlocksNestedTooDeeply) {
  CHECK(loadError("server { listen 8080; root /a; location /x/ { "
                  "limit_except GET { deny all; } } }")
            .empty());
  CHECK(contains(loadError("server { location /x/ { limit_except GET { "
                           "deny { } } } }"),
                 "nested too deeply"));
}

TEST_CASE(reportsLineNumbers) {
  CHECK(contains(loadError("server {\n  listen 8080;\n  bogus 1;\n}\n"),
                 "test.conf:3:"));
}

TEST_CASE(rejectsUnknownOrMisplacedDirectives) {
  CHECK(contains(loadError("http { }"), "unknown top-level directive"));
  CHECK(contains(loadError("server { listen 8080; root /a; bogus 1; }"),
                 "unknown directive \"bogus\""));
  CHECK(contains(loadError("server { listen 8080; alias /a; }"),
                 "\"alias\" is not allowed in \"server\""));
  CHECK(contains(
      loadError(
          "server { listen 8080; root /a; location /x/ { location /y/ { } } }"),
      "\"location\" is not allowed in \"location\""));
  CHECK(contains(
      loadError("server { listen 8080; root /a; location /x/ { listen 80; } }"),
      "\"listen\" is not allowed in \"location\""));
  CHECK(contains(loadError("server { listen 8080; root /a { } }"),
                 "does not take a block"));
  CHECK(contains(loadError("server { listen 8080; root /a /b; }"),
                 "invalid number of arguments in \"root\""));
  CHECK(contains(loadError("server;"), "\"server\" requires a block"));
  CHECK(
      contains(loadError("server { listen 8080; root /a; location = /x { } }"),
               "modifiers"));
  CHECK(contains(loadError("server { listen 8080; root /a; location x { } }"),
                 "must start with"));
}

TEST_CASE(rejectsListenProblems) {
  CHECK(contains(loadError("server { root /a; }"), "no \"listen\""));
  CHECK(contains(loadError("server { listen 0; root /a; }"), "invalid listen"));
  CHECK(contains(loadError("server { listen 70000; root /a; }"),
                 "invalid listen"));
  CHECK(contains(loadError("server { listen 256.0.0.1:80; root /a; }"),
                 "invalid listen"));
  CHECK(contains(loadError("server { listen localhost:80; root /a; }"),
                 "invalid listen"));
  CHECK(contains(
      loadError(
          "server { listen 8080; root /a; }\nserver { listen 8080; root /b; }"),
      "duplicate listen 0.0.0.0:8080"));
  CHECK(contains(loadError("server { listen 8080; root /a; }\nserver { listen "
                           "127.0.0.1:8080; root /b; }"),
                 "conflicts with"));
}

TEST_CASE(rejectsBodySizeOutsideRange) {
  CHECK(contains(
      loadError("server { listen 8080; root /a; client_max_body_size 0; }"),
      "between 1 and 100m"));
  CHECK(contains(
      loadError("server { listen 8080; root /a; client_max_body_size 101m; }"),
      "between 1 and 100m"));
  CHECK(contains(
      loadError("server { listen 8080; root /a; client_max_body_size 1g; }"),
      "invalid size"));
  CHECK(contains(loadError("server { listen 8080; root /a; "
                           "client_max_body_size 99999999999999999999999; }"),
                 "invalid size"));
  CHECK(loadError("server { listen 8080; root /a; client_max_body_size 100m; }")
            .empty());
}

TEST_CASE(rejectsLocationProblems) {
  CHECK(contains(
      loadError("server { listen 8080; location /x/ { root /a; alias /b/; } }"),
      "\"root\" and \"alias\""));
  CHECK(contains(loadError("server { listen 8080; location /x/ { } }"),
                 "has no \"root\" or \"alias\""));
  CHECK(loadError("server { listen 8080; location /x/ { return 301 /y/; } }")
            .empty());
  CHECK(contains(
      loadError(
          "server { listen 8080; root /a; location /x/ { } location /x/ { } }"),
      "duplicate location"));
  CHECK(contains(loadError("server { listen 8080; root /a; root /b; }"),
                 "duplicate \"root\""));
}

TEST_CASE(rejectsBadLocationDirectives) {
  CHECK(contains(
      loadError(
          "server { listen 8080; root /a; location /x/ { return 200 /y; } }"),
      "\"return\" code"));
  CHECK(contains(loadError("server { listen 8080; root /a; location /x/ { "
                           "limit_except GET; } }"),
                 "requires a block"));
  CHECK(contains(loadError("server { listen 8080; root /a; location /x/ { "
                           "limit_except PUT { deny all; } } }"),
                 "unsupported method"));
  CHECK(contains(loadError("server { listen 8080; root /a; location /x/ { "
                           "limit_except GET { allow all; } } }"),
                 "only \"deny all;\""));
  CHECK(contains(loadError("server { listen 8080; root /a; location /x/ { cgi "
                           "php /usr/bin/php-cgi; } }"),
                 "extension"));
  CHECK(contains(loadError("server { listen 8080; root /a; location /x/ { cgi "
                           ".py /a; cgi .py /b; } }"),
                 "duplicate \"cgi\""));
  // G-18: the script runs in its own directory.
  // G-18: 스크립트는 자기 디렉토리에서 실행된다.
  CHECK(contains(loadError("server { listen 8080; root /a; location /x/ { cgi "
                           ".py python3; } }"),
                 "absolute path"));
  CHECK(contains(
      loadError(
          "server { listen 8080; root /a; location /x/ { autoindex yes; } }"),
      "\"on\" or \"off\""));
}

TEST_CASE(c29_rejectsIndexNamesWithSlash) {
  CHECK(contains(
      loadError("server { listen 8080; root /a; index ../secret.txt; }"),
      "must not contain"));
  CHECK(contains(loadError("server { listen 8080; root /a; location /x/ { "
                           "index a.html sub/b.html; } }"),
                 "must not contain"));
  CHECK(loadError("server { listen 8080; root /a; index a.html .b; }").empty());
}

TEST_CASE(rejectsBadErrorPages) {
  CHECK(contains(
      loadError(
          "server { listen 8080; root /a; error_page 404 errors/404.html; }"),
      "must start with"));
  CHECK(contains(
      loadError("server { listen 8080; root /a; error_page 200 /e.html; }"),
      "invalid error_page code"));
  CHECK(contains(loadError("server { listen 8080; root /a; error_page 404 "
                           "/a.html; error_page 404 /b.html; }"),
                 "duplicate error_page"));
  const ServerConfig server =
      load("server { listen 8080; root /a; error_page 500 502 /50x.html; }")
          .at(0);
  CHECK_EQ(2u, server.errorPages.size());
}

TEST_CASE(reportsMissingFile) {
  try {
    loadConfigFile("/nonexistent/webserv.conf");
    CHECK(false);
  } catch (const ConfigError& error) {
    CHECK(contains(error.what(), "cannot open"));
  }
}

TEST_CASE(rejectsDirectoryAsFile) {
  try {
    loadConfigFile("tests");
    CHECK(false);
  } catch (const ConfigError& error) {
    CHECK(contains(error.what(), "not a regular file"));
  }
}

TEST_CASE(acceptsSeveralListensInOneServer) {
  const ServerConfig server =
      load("server { listen 127.0.0.1:8080; listen 8081; root /a; }").at(0);
  CHECK_EQ(2u, server.listens.size());
  if (server.listens.size() == 2) {
    CHECK_EQ(8080, server.listens.at(0).port);
    CHECK_EQ(std::string("0.0.0.0"), server.listens[1].host);
    CHECK_EQ(8081, server.listens[1].port);
  }
}

TEST_CASE(acceptsPortBoundaries) {
  CHECK(loadError("server { listen 1; root /a; }").empty());
  CHECK(loadError("server { listen 65535; root /a; }").empty());
  CHECK(contains(loadError("server { listen 65536; root /a; }"),
                 "invalid listen"));
}

// NGINX also accepts these; this server requires an explicit port and a
// dotted IPv4 address (design decision 13).
// NGINX는 이것도 받지만, 이 서버는 명시한 포트와 점으로 구분한 IPv4 주소를
// 요구한다 (설계 결정 13).
TEST_CASE(rejectsListenWithoutPortOrWithWildcard) {
  CHECK(contains(loadError("server { listen 127.0.0.1; root /a; }"),
                 "expected IPv4:port or port"));
  CHECK(contains(loadError("server { listen *:8000; root /a; }"),
                 "expected IPv4:port or port"));
}

TEST_CASE(rejectsDuplicateListenInOneServer) {
  CHECK(contains(loadError("server { listen 8080; listen 8080; root /a; }"),
                 "duplicate listen 0.0.0.0:8080"));
}

TEST_CASE(rejectsWildcardAfterSpecificAddress) {
  CHECK(contains(loadError("server { listen 127.0.0.1:8080; root /a; }\n"
                           "server { listen 8080; root /b; }"),
                 "conflicts with"));
}

TEST_CASE(acceptsBodySizeWithoutUnitAndLowerBound) {
  const ServerConfig server =
      load(
          "server { listen 8080; root /a; client_max_body_size 1; "
          "location /x/ { client_max_body_size 4096; } }")
          .at(0);
  const LocationConfig* root = findLocation(server, "/");
  const LocationConfig* x = findLocation(server, "/x/");
  CHECK(root != 0 && x != 0);
  if (root != 0) CHECK_EQ(static_cast<std::size_t>(1), root->maxBodySize);
  if (x != 0) CHECK_EQ(static_cast<std::size_t>(4096), x->maxBodySize);
}

TEST_CASE(locationOverridesServerSettings) {
  const ServerConfig server =
      load(
          "server { listen 8080; root /srv; index a.html; autoindex on;"
          "  location /x/ { root /other; index b.html; autoindex off; } }")
          .at(0);
  const LocationConfig* x = findLocation(server, "/x/");
  CHECK(x != 0);
  if (x != 0) {
    CHECK_EQ(std::string("/other"), x->root);
    CHECK_EQ(1u, x->indexFiles.size());
    if (!x->indexFiles.empty())
      CHECK_EQ(std::string("b.html"), x->indexFiles.at(0));
    CHECK(!x->autoindex);
  }
}

TEST_CASE(noImplicitRootLocationWithoutServerRoot) {
  const ServerConfig server =
      load("server { listen 8080; location /x/ { return 301 /y/; } }").at(0);
  CHECK(findLocation(server, "/") == 0);
  CHECK_EQ(1u, server.locations.size());
}
