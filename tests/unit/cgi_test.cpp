#include <algorithm>
#include <string>
#include <vector>

#include "cgi/CgiEnv.hpp"
#include "cgi/CgiOutput.hpp"
#include "core/SocketAddress.hpp"
#include "fs/FilePath.hpp"
#include "unit/TestFramework.hpp"

// Test IDs (G-xx) refer to the test plan in the wiki (analysis/test-plan).

namespace {

// parseCgiOutput() takes the body out of its argument, so tests parse a
// copy.
CgiOutput parse(std::string output) { return parseCgiOutput(output); }

int statusOf(const std::string& output) {
  const CgiOutput parsed = parse(output);
  return parsed.kind == CgiOutput::RESPONSE ? parsed.response.status : -1;
}

bool hasVariable(const std::vector<std::string>& env,
                 const std::string& entry) {
  return std::find(env.begin(), env.end(), entry) != env.end();
}

bool hasName(const std::vector<std::string>& env, const std::string& name) {
  for (std::size_t i = 0; i < env.size(); ++i) {
    if (env[i].compare(0, name.size() + 1, name + "=") == 0) return true;
  }
  return false;
}

}  // namespace

TEST_CASE(g07_documentResponse) {
  const CgiOutput parsed = parse(
      "Content-Type: text/html\nStatus: 404 Not Found\nSet-Cookie: a=1\n"
      "Set-Cookie: b=2\nContent-Length: 999\nDate: x\n\n<p>gone</p>\n");
  CHECK_EQ(static_cast<int>(CgiOutput::RESPONSE),
           static_cast<int>(parsed.kind));
  CHECK_EQ(404, parsed.response.status);
  CHECK_EQ(std::string("<p>gone</p>\n"), parsed.response.body);
  CHECK_EQ(2u, parsed.response.headers.count("Set-Cookie"));
  CHECK(!parsed.response.headers.has("Content-Length"));
  CHECK(!parsed.response.headers.has("Date"));
  CHECK(!parsed.response.headers.has("Status"));
  CHECK_EQ(200, statusOf("content-type: text/plain\r\n\r\nx"));
  CHECK_EQ(204, statusOf("Content-Type: text/plain\nStatus: 204\n\nx"));
  CHECK_EQ(std::string(""),
           parse("Content-Type: a/b\nStatus: 204\n\nx").response.body);
}

TEST_CASE(g06_invalidOutputs) {
  CHECK_EQ(-1, statusOf(""));
  CHECK_EQ(-1, statusOf("no header at all"));
  CHECK_EQ(-1, statusOf("Content-Type: text/plain\nno blank line"));
  CHECK_EQ(-1, statusOf("\nbody without header"));
  CHECK_EQ(-1, statusOf("Status: 200\n\nbody"));  // no Content-Type
  CHECK_EQ(-1, statusOf("Content-Type: a/b\nContent-Type: a/b\n\n"));
  CHECK_EQ(-1, statusOf("Content-Type: a/b\nStatus: 20\n\n"));
  CHECK_EQ(-1, statusOf("Content-Type: a/b\nStatus: 100\n\n"));
  CHECK_EQ(-1, statusOf("Content-Type: a/b\nStatus: 600\n\n"));
  CHECK_EQ(-1, statusOf("Content-Type : a/b\n\n"));
  CHECK_EQ(-1, statusOf("Content-Type: a/b\n folded\n\n"));
  CHECK_EQ(-1, statusOf(std::string("Content-Type: a/b\nX: a\rb\n\n")));
}

TEST_CASE(g08_redirects) {
  CgiOutput parsed = parse("Location: /other.py?x=1\n\n");
  CHECK_EQ(static_cast<int>(CgiOutput::LOCAL_REDIRECT),
           static_cast<int>(parsed.kind));
  CHECK_EQ(std::string("/other.py?x=1"), parsed.localTarget);
  parsed = parse("Location: http://example.com/\n\n");
  CHECK_EQ(302, parsed.response.status);
  CHECK_EQ(std::string("http://example.com/"),
           *parsed.response.headers.first("Location"));
  CHECK_EQ(301, statusOf("Location: http://e.com/\nStatus: 301 Moved\n"
                         "Content-Type: text/html\n\nmoved"));
  // A local redirect allows nothing else; a document redirect needs 3xx.
  CHECK_EQ(-1, statusOf("Location: /x\nContent-Type: a/b\n\n"));
  CHECK_EQ(-1, statusOf("Location: /x\n\nbody"));
  CHECK_EQ(-1, statusOf("Location: http://e.com/\nStatus: 200\n"
                        "Content-Type: a/b\n\nx"));
  CHECK_EQ(-1, statusOf("Location: relative\n\n"));
}

TEST_CASE(g01_metaVariables) {
  Request request;
  request.method = "POST";
  request.query = "a=1&b=%20";
  request.version = "HTTP/1.1";
  request.body = "abc";
  request.headers.add("Host", "x");
  request.headers.add("Content-Length", "3");
  request.headers.add("Content-Type", "text/plain");
  request.headers.add("Authorization", "secret");
  request.headers.add("X-Multi", "1");
  request.headers.add("x-multi", "2");
  RouteResult script;
  script.fsPath = "www/cgi/a.py";
  script.scriptName = "/cgi/a.py";
  script.pathInfo = "/p q";
  script.pathTranslated = "www/p q";
  ListenContext where;
  where.remoteAddress = "10.0.0.2";
  where.localAddress = "127.0.0.1";
  where.localPort = 8080;
  const std::vector<std::string> env =
      buildCgiEnvironment(request, script, where);
  CHECK(hasVariable(env, "GATEWAY_INTERFACE=CGI/1.1"));
  CHECK(hasVariable(env, "REQUEST_METHOD=POST"));
  CHECK(hasVariable(env, "QUERY_STRING=a=1&b=%20"));
  CHECK(hasVariable(env, "SCRIPT_NAME=/cgi/a.py"));
  CHECK(hasVariable(env, "PATH_INFO=/p q"));
  CHECK(!hasName(env, "PATH_TRANSLATED"));  // relative: not usable there
  CHECK(hasVariable(env, "SCRIPT_FILENAME=a.py"));
  CHECK(hasVariable(env, "SERVER_PORT=8080"));
  CHECK(hasVariable(env, "SERVER_NAME=127.0.0.1"));
  CHECK(hasVariable(env, "REMOTE_ADDR=10.0.0.2"));
  CHECK(hasVariable(env, "CONTENT_LENGTH=3"));
  CHECK(hasVariable(env, "CONTENT_TYPE=text/plain"));
  CHECK(hasVariable(env, "REDIRECT_STATUS=200"));
  CHECK(hasVariable(env, "HTTP_HOST=x"));
  CHECK(hasVariable(env, "HTTP_X_MULTI=1, 2"));
  CHECK(!hasName(env, "HTTP_AUTHORIZATION"));
  CHECK(!hasName(env, "HTTP_CONTENT_LENGTH"));
  script.fsPath = "/srv/cgi/a.py";
  script.pathTranslated = "/srv/p q";
  const std::vector<std::string> absolute =
      buildCgiEnvironment(request, script, where);
  CHECK(hasVariable(absolute, "SCRIPT_FILENAME=/srv/cgi/a.py"));
  CHECK(hasVariable(absolute, "PATH_TRANSLATED=/srv/p q"));
  request.headers = HeaderMap();
  CHECK(
      !hasName(buildCgiEnvironment(request, script, where), "CONTENT_LENGTH"));
}

TEST_CASE(g01_addressAndPathText) {
  CHECK_EQ(std::string("127.0.0.1"), formatIpv4(htonl(0x7f000001)));
  CHECK_EQ(std::string("255.0.10.1"), formatIpv4(htonl(0xff000a01)));
  CHECK_EQ(std::string("www/cgi"), parentDirectory("www/cgi/a.py"));
  CHECK_EQ(std::string("."), parentDirectory("a.py"));
  CHECK_EQ(std::string("/"), parentDirectory("/a.py"));
  CHECK_EQ(std::string("a.py"), baseName("www/cgi/a.py"));
  CHECK_EQ(std::string("a.py"), baseName("a.py"));
}
