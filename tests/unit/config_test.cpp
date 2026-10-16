#include "unit/TestFramework.hpp"

#include "config/ConfigError.hpp"
#include "config/ConfigLimits.hpp"
#include "config/ConfigLoader.hpp"

namespace {

std::vector<ServerConfig> load(const std::string& text)
{
    return loadConfigText(text, "test.conf");
}

// Returns the ConfigError message, or "" if loading succeeded.
std::string loadError(const std::string& text)
{
    try {
        load(text);
    } catch (const ConfigError& error) {
        return error.what();
    }
    return "";
}

bool contains(const std::string& text, const std::string& part)
{
    return text.find(part) != std::string::npos;
}

const LocationConfig* findLocation(const ServerConfig& server, const std::string& path)
{
    for (std::size_t i = 0; i < server.locations.size(); ++i) {
        if (server.locations[i].path == path)
            return &server.locations[i];
    }
    return 0;
}

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

}  // namespace

TEST_CASE(parsesDesignExample)
{
    const std::vector<ServerConfig> servers = load(kDesignExample);
    CHECK_EQ(1u, servers.size());
    const ServerConfig& server = servers[0];
    CHECK_EQ(1u, server.listens.size());
    CHECK_EQ(std::string("127.0.0.1"), server.listens[0].host);
    CHECK_EQ(8080, server.listens[0].port);
    CHECK_EQ(std::string("/errors/404.html"), server.errorPages.find(404)->second);
    CHECK_EQ(5u, server.locations.size());

    const LocationConfig* root = findLocation(server, "/");
    CHECK(root != 0);
    if (root != 0) {
        CHECK_EQ(2u, root->allowedMethods.size());   // GET and the implied HEAD
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
        CHECK_EQ(std::string("/usr/bin/php-cgi"), cgi->cgiByExtension.find(".php")->second);
        CHECK(cgi->allowedMethods.empty());
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

TEST_CASE(locationsAreSortedLongestFirst)
{
    const ServerConfig server = load(kDesignExample)[0];
    for (std::size_t i = 1; i < server.locations.size(); ++i)
        CHECK(server.locations[i - 1].path.size() >= server.locations[i].path.size());
}

TEST_CASE(locationInheritsServerSettings)
{
    const ServerConfig server = load(
        "server { listen 8080; root /srv; index a.html b.html; autoindex on; client_max_body_size 2k;"
        "  location /x/ { } }")[0];
    const LocationConfig* x = findLocation(server, "/x/");
    CHECK(x != 0);
    if (x != 0) {
        CHECK_EQ(std::string("/srv"), x->root);
        CHECK_EQ(2u, x->indexFiles.size());
        CHECK(x->autoindex);
        CHECK_EQ(static_cast<std::size_t>(2048), x->maxBodySize);
    }
}

TEST_CASE(serverRootCreatesImplicitRootLocation)
{
    const ServerConfig server = load("server { listen 8080; root /srv; location /x/ { } }")[0];
    const LocationConfig* root = findLocation(server, "/");
    CHECK(root != 0);
    if (root != 0)
        CHECK_EQ(std::string("/srv"), root->root);
}

TEST_CASE(defaultsWhenNotConfigured)
{
    const ServerConfig server = load("server { listen 8080; root /srv; }")[0];
    CHECK_EQ(std::string("0.0.0.0"), server.listens[0].host);
    const LocationConfig& root = server.locations[0];
    CHECK_EQ(1u, root.indexFiles.size());
    CHECK_EQ(std::string("index.html"), root.indexFiles[0]);
    CHECK(!root.autoindex);
    CHECK_EQ(config_limits::kDefaultMaxBodySize, root.maxBodySize);
}

TEST_CASE(acceptsSeparateServersOnDifferentPorts)
{
    const std::vector<ServerConfig> servers = load(
        "server { listen 127.0.0.1:8080; root /a; }\n"
        "server { listen 127.0.0.1:8081; root /b; }\n"
        "server { listen 127.0.0.2:8080; root /c; }\n");
    CHECK_EQ(3u, servers.size());
}

TEST_CASE(rejectsSyntaxErrors)
{
    CHECK(contains(loadError("server { listen 8080 }"), "not terminated"));
    CHECK(contains(loadError("server { listen 8080;"), "not closed"));
    CHECK(contains(loadError("server { listen 8080; } }"), "unexpected \"}\""));
    CHECK(contains(loadError("server { root \"/a b\"; }"), "quoted strings"));
    CHECK(contains(loadError(""), "no \"server\" block"));
}

TEST_CASE(reportsLineNumbers)
{
    CHECK(contains(loadError("server {\n  listen 8080;\n  bogus 1;\n}\n"), "test.conf:3:"));
}

TEST_CASE(rejectsUnknownOrMisplacedDirectives)
{
    CHECK(contains(loadError("http { }"), "unknown top-level directive"));
    CHECK(contains(loadError("server { listen 8080; root /a; bogus 1; }"), "unknown directive \"bogus\""));
    CHECK(contains(loadError("server { listen 8080; alias /a; }"), "unknown directive \"alias\""));
    CHECK(contains(loadError("server { listen 8080; root /a; location /x/ { location /y/ { } } }"), "nested"));
    CHECK(contains(loadError("server { listen 8080; root /a; location = /x { } }"), "modifiers"));
    CHECK(contains(loadError("server { listen 8080; root /a; location x { } }"), "must start with"));
}

TEST_CASE(rejectsListenProblems)
{
    CHECK(contains(loadError("server { root /a; }"), "no \"listen\""));
    CHECK(contains(loadError("server { listen 0; root /a; }"), "invalid listen"));
    CHECK(contains(loadError("server { listen 70000; root /a; }"), "invalid listen"));
    CHECK(contains(loadError("server { listen 256.0.0.1:80; root /a; }"), "invalid listen"));
    CHECK(contains(loadError("server { listen localhost:80; root /a; }"), "invalid listen"));
    CHECK(contains(loadError("server { listen 8080; root /a; }\nserver { listen 8080; root /b; }"),
                   "duplicate listen 0.0.0.0:8080"));
    CHECK(contains(loadError("server { listen 8080; root /a; }\nserver { listen 127.0.0.1:8080; root /b; }"),
                   "conflicts with"));
}

TEST_CASE(rejectsBodySizeOutsideRange)
{
    CHECK(contains(loadError("server { listen 8080; root /a; client_max_body_size 0; }"), "between 1 and 100m"));
    CHECK(contains(loadError("server { listen 8080; root /a; client_max_body_size 101m; }"), "between 1 and 100m"));
    CHECK(contains(loadError("server { listen 8080; root /a; client_max_body_size 1g; }"), "invalid size"));
    CHECK(contains(loadError("server { listen 8080; root /a; client_max_body_size 99999999999999999999999; }"),
                   "invalid size"));
    CHECK(loadError("server { listen 8080; root /a; client_max_body_size 100m; }").empty());
}

TEST_CASE(rejectsLocationProblems)
{
    CHECK(contains(loadError("server { listen 8080; location /x/ { root /a; alias /b/; } }"), "\"root\" and \"alias\""));
    CHECK(contains(loadError("server { listen 8080; location /x/ { } }"), "has no \"root\" or \"alias\""));
    CHECK(loadError("server { listen 8080; location /x/ { return 301 /y/; } }").empty());
    CHECK(contains(loadError("server { listen 8080; root /a; location /x/ { } location /x/ { } }"),
                   "duplicate location"));
    CHECK(contains(loadError("server { listen 8080; root /a; root /b; }"), "duplicate \"root\""));
}

TEST_CASE(rejectsBadLocationDirectives)
{
    CHECK(contains(loadError("server { listen 8080; root /a; location /x/ { return 200 /y; } }"), "\"return\" code"));
    CHECK(contains(loadError("server { listen 8080; root /a; location /x/ { limit_except GET; } }"), "requires a block"));
    CHECK(contains(loadError("server { listen 8080; root /a; location /x/ { limit_except PUT { deny all; } } }"),
                   "unsupported method"));
    CHECK(contains(loadError("server { listen 8080; root /a; location /x/ { limit_except GET { allow all; } } }"),
                   "only \"deny all;\""));
    CHECK(contains(loadError("server { listen 8080; root /a; location /x/ { cgi php /usr/bin/php-cgi; } }"),
                   "extension"));
    CHECK(contains(loadError("server { listen 8080; root /a; location /x/ { cgi .py /a; cgi .py /b; } }"),
                   "duplicate \"cgi\""));
    CHECK(contains(loadError("server { listen 8080; root /a; location /x/ { autoindex yes; } }"), "\"on\" or \"off\""));
}

TEST_CASE(rejectsBadErrorPages)
{
    CHECK(contains(loadError("server { listen 8080; root /a; error_page 404 errors/404.html; }"), "must start with"));
    CHECK(contains(loadError("server { listen 8080; root /a; error_page 200 /e.html; }"), "invalid error_page code"));
    CHECK(contains(loadError("server { listen 8080; root /a; error_page 404 /a.html; error_page 404 /b.html; }"),
                   "duplicate error_page"));
    const ServerConfig server = load("server { listen 8080; root /a; error_page 500 502 /50x.html; }")[0];
    CHECK_EQ(2u, server.errorPages.size());
}

TEST_CASE(reportsMissingFile)
{
    try {
        loadConfigFile("/nonexistent/webserv.conf");
        CHECK(false);
    } catch (const ConfigError& error) {
        CHECK(contains(error.what(), "cannot open"));
    }
}

TEST_CASE(rejectsDirectoryAsFile)
{
    try {
        loadConfigFile("tests");
        CHECK(false);
    } catch (const ConfigError& error) {
        CHECK(contains(error.what(), "not a regular file"));
    }
}
