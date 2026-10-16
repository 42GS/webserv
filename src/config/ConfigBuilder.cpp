#include "config/ConfigBuilder.hpp"

#include "config/ConfigError.hpp"
#include "config/ConfigLimits.hpp"
#include "config/ConfigValues.hpp"

#include <algorithm>
#include <sstream>

namespace {

const unsigned long kMinErrorPageCode = 300;
const unsigned long kMaxErrorPageCode = 599;
const unsigned long kMaxStatusCode = 999;
const char* const kDefaultIndexFile = "index.html";   // NGINX "index" default

// Settings that may appear in both server and location blocks.
struct SharedSettings {
    std::string              root;
    std::vector<std::string> indexFiles;
    bool                     autoindex;
    std::size_t              maxBodySize;
};

struct ListenEntry {
    ListenAddr addr;
    int        line;
};

std::string describe(const ListenAddr& addr)
{
    std::ostringstream out;
    out << addr.host << ":" << addr.port;
    return out.str();
}

bool isRedirectCode(unsigned long code)
{
    return code == 301 || code == 302 || code == 303 || code == 307 || code == 308;
}

bool isKnownMethod(const std::string& method)
{
    return method == "GET" || method == "HEAD" || method == "POST" || method == "DELETE";
}

bool longerPathFirst(const LocationConfig& a, const LocationConfig& b)
{
    return a.path.size() > b.path.size();
}

class ConfigBuilder {
public:
    ConfigBuilder(const DirectiveTree& tree, const std::string& source)
        : tree_(tree), source_(source)
    {
    }

    std::vector<ServerConfig> build()
    {
        std::vector<ServerConfig> servers;
        for (std::size_t i = 0; i < tree_.topLevel.size(); ++i) {
            const Directive& directive = node(tree_.topLevel[i]);
            if (directive.name != "server")
                fail(directive, "unknown top-level directive \"" + directive.name + "\" (only \"server\" is allowed)");
            servers.push_back(buildServer(directive));
        }
        if (servers.empty())
            throw ConfigError(source_, 1, "no \"server\" block");
        checkListenConflicts();
        return servers;
    }

private:
    const Directive& node(std::size_t index) const { return tree_.nodes[index]; }

    void fail(const Directive& directive, const std::string& message) const
    {
        throw ConfigError(source_, directive.line, message);
    }

    void requireArgCount(const Directive& directive, std::size_t count) const
    {
        if (directive.args.size() != count)
            fail(directive, "invalid number of arguments in \"" + directive.name + "\"");
    }

    void requireNoBlock(const Directive& directive) const
    {
        if (directive.isBlock)
            fail(directive, "\"" + directive.name + "\" does not take a block");
    }

    void requireBlock(const Directive& directive) const
    {
        if (!directive.isBlock)
            fail(directive, "\"" + directive.name + "\" requires a block");
    }

    // Rejects a directive that may appear only once per block.
    void requireFirst(const Directive& directive, std::set<std::string>& seen) const
    {
        if (!seen.insert(directive.name).second)
            fail(directive, "duplicate \"" + directive.name + "\"");
    }

    ServerConfig buildServer(const Directive& block)
    {
        requireBlock(block);
        if (!block.args.empty())
            fail(block, "\"server\" takes no arguments");
        ServerConfig server;
        SharedSettings shared;
        shared.indexFiles.push_back(kDefaultIndexFile);
        shared.autoindex = false;
        shared.maxBodySize = config_limits::kDefaultMaxBodySize;
        std::set<std::string> seen;
        std::vector<std::size_t> locationBlocks;
        for (std::size_t i = 0; i < block.children.size(); ++i) {
            const Directive& directive = node(block.children[i]);
            if (directive.name == "location")
                locationBlocks.push_back(block.children[i]);
            else if (directive.name == "listen")
                applyListen(directive, server);
            else if (directive.name == "error_page")
                applyErrorPage(directive, server);
            else if (!applyShared(directive, shared, seen))
                fail(directive, "unknown directive \"" + directive.name + "\" in \"server\"");
        }
        if (server.listens.empty())
            fail(block, "\"server\" has no \"listen\"");
        // Locations inherit server values regardless of where they appear in the block.
        std::set<std::string> paths;
        for (std::size_t i = 0; i < locationBlocks.size(); ++i) {
            const Directive& directive = node(locationBlocks[i]);
            LocationConfig location = buildLocation(directive, shared);
            if (!paths.insert(location.path).second)
                fail(directive, "duplicate location \"" + location.path + "\"");
            server.locations.push_back(location);
        }
        if (paths.count("/") == 0 && !shared.root.empty())
            server.locations.push_back(makeImplicitRootLocation(shared));
        std::stable_sort(server.locations.begin(), server.locations.end(), longerPathFirst);
        return server;
    }

    // Handles directives allowed in both server and location blocks.
    // Returns false if the directive is not one of them.
    bool applyShared(const Directive& directive, SharedSettings& shared, std::set<std::string>& seen) const
    {
        if (directive.name == "root") {
            requireNoBlock(directive);
            requireFirst(directive, seen);
            requireArgCount(directive, 1);
            shared.root = directive.args[0];
        } else if (directive.name == "index") {
            requireNoBlock(directive);
            requireFirst(directive, seen);
            if (directive.args.empty())
                fail(directive, "invalid number of arguments in \"index\"");
            shared.indexFiles = directive.args;
        } else if (directive.name == "autoindex") {
            requireNoBlock(directive);
            requireFirst(directive, seen);
            requireArgCount(directive, 1);
            if (directive.args[0] != "on" && directive.args[0] != "off")
                fail(directive, "\"autoindex\" must be \"on\" or \"off\"");
            shared.autoindex = (directive.args[0] == "on");
        } else if (directive.name == "client_max_body_size") {
            requireNoBlock(directive);
            requireFirst(directive, seen);
            requireArgCount(directive, 1);
            shared.maxBodySize = parseBodySize(directive);
        } else {
            return false;
        }
        return true;
    }

    std::size_t parseBodySize(const Directive& directive) const
    {
        std::size_t size;
        if (!parseSize(directive.args[0], size))
            fail(directive, "invalid size \"" + directive.args[0] + "\"");
        if (size == 0 || size > config_limits::kMaxBodySizeCeiling)
            fail(directive, "\"client_max_body_size\" must be between 1 and 100m");
        return size;
    }

    void applyListen(const Directive& directive, ServerConfig& server)
    {
        requireNoBlock(directive);
        requireArgCount(directive, 1);
        ListenEntry entry;
        if (!parseListenAddr(directive.args[0], entry.addr))
            fail(directive, "invalid listen address \"" + directive.args[0] + "\"");
        entry.line = directive.line;
        listens_.push_back(entry);
        server.listens.push_back(entry.addr);
    }

    void applyErrorPage(const Directive& directive, ServerConfig& server) const
    {
        requireNoBlock(directive);
        if (directive.args.size() < 2)
            fail(directive, "invalid number of arguments in \"error_page\"");
        const std::string& uri = directive.args.back();
        if (uri.empty() || uri[0] != '/')
            fail(directive, "\"error_page\" URI must start with \"/\"");
        for (std::size_t i = 0; i + 1 < directive.args.size(); ++i) {
            unsigned long code;
            if (!parseUnsigned(directive.args[i], kMaxStatusCode, code)
                || code < kMinErrorPageCode || code > kMaxErrorPageCode)
                fail(directive, "invalid error_page code \"" + directive.args[i] + "\"");
            if (server.errorPages.count(static_cast<int>(code)) != 0)
                fail(directive, "duplicate error_page code \"" + directive.args[i] + "\"");
            server.errorPages[static_cast<int>(code)] = uri;
        }
    }

    LocationConfig buildLocation(const Directive& block, const SharedSettings& inherited) const
    {
        requireBlock(block);
        if (block.args.size() != 1)
            fail(block, "\"location\" takes exactly one path (modifiers are not supported)");
        if (block.args[0].empty() || block.args[0][0] != '/')
            fail(block, "location path must start with \"/\"");
        LocationConfig location = makeLocation(block.args[0]);
        SharedSettings shared = inherited;
        shared.root.clear();
        std::set<std::string> seen;
        for (std::size_t i = 0; i < block.children.size(); ++i) {
            const Directive& directive = node(block.children[i]);
            if (!applyShared(directive, shared, seen) && !applyLocationOnly(directive, location, seen))
                fail(directive, "unknown directive \"" + directive.name + "\" in \"location\"");
        }
        if (!shared.root.empty() && !location.alias.empty())
            fail(block, "\"root\" and \"alias\" in the same location");
        if (location.alias.empty())
            location.root = shared.root.empty() ? inherited.root : shared.root;
        location.indexFiles = shared.indexFiles;
        location.autoindex = shared.autoindex;
        location.maxBodySize = shared.maxBodySize;
        if (location.returnCode == 0 && location.root.empty() && location.alias.empty())
            fail(block, "location \"" + location.path + "\" has no \"root\" or \"alias\"");
        return location;
    }

    // Handles directives allowed only in location blocks.
    // Returns false if the directive is not one of them.
    bool applyLocationOnly(const Directive& directive, LocationConfig& location, std::set<std::string>& seen) const
    {
        if (directive.name == "alias") {
            requireNoBlock(directive);
            requireFirst(directive, seen);
            requireArgCount(directive, 1);
            location.alias = directive.args[0];
        } else if (directive.name == "return") {
            requireNoBlock(directive);
            requireFirst(directive, seen);
            applyReturn(directive, location);
        } else if (directive.name == "limit_except") {
            requireFirst(directive, seen);
            applyLimitExcept(directive, location);
        } else if (directive.name == "upload_store") {
            requireNoBlock(directive);
            requireFirst(directive, seen);
            requireArgCount(directive, 1);
            location.uploadStore = directive.args[0];
        } else if (directive.name == "cgi") {
            requireNoBlock(directive);
            applyCgi(directive, location);
        } else if (directive.name == "location") {
            fail(directive, "nested \"location\" is not supported");
        } else {
            return false;
        }
        return true;
    }

    void applyReturn(const Directive& directive, LocationConfig& location) const
    {
        requireArgCount(directive, 2);
        unsigned long code;
        if (!parseUnsigned(directive.args[0], kMaxStatusCode, code) || !isRedirectCode(code))
            fail(directive, "\"return\" code must be 301, 302, 303, 307 or 308");
        location.returnCode = static_cast<int>(code);
        location.returnUrl = directive.args[1];
    }

    // Accepts only the form "limit_except METHOD ... { deny all; }".
    void applyLimitExcept(const Directive& directive, LocationConfig& location) const
    {
        requireBlock(directive);
        if (directive.args.empty())
            fail(directive, "invalid number of arguments in \"limit_except\"");
        for (std::size_t i = 0; i < directive.args.size(); ++i) {
            if (!isKnownMethod(directive.args[i]))
                fail(directive, "unsupported method \"" + directive.args[i] + "\" in \"limit_except\"");
            location.allowedMethods.insert(directive.args[i]);
        }
        // NGINX: allowing GET also allows HEAD.
        if (location.allowedMethods.count("GET") != 0)
            location.allowedMethods.insert("HEAD");
        if (directive.children.size() != 1)
            fail(directive, "\"limit_except\" block must contain only \"deny all;\"");
        const Directive& deny = node(directive.children[0]);
        if (deny.name != "deny" || deny.isBlock || deny.args.size() != 1 || deny.args[0] != "all")
            fail(deny, "\"limit_except\" block must contain only \"deny all;\"");
    }

    void applyCgi(const Directive& directive, LocationConfig& location) const
    {
        requireArgCount(directive, 2);
        const std::string& extension = directive.args[0];
        if (extension.size() < 2 || extension[0] != '.')
            fail(directive, "\"cgi\" extension must look like \".php\"");
        if (location.cgiByExtension.count(extension) != 0)
            fail(directive, "duplicate \"cgi\" extension \"" + extension + "\"");
        location.cgiByExtension[extension] = directive.args[1];
    }

    LocationConfig makeLocation(const std::string& path) const
    {
        LocationConfig location;
        location.path = path;
        location.autoindex = false;
        location.returnCode = 0;
        location.maxBodySize = config_limits::kDefaultMaxBodySize;
        return location;
    }

    LocationConfig makeImplicitRootLocation(const SharedSettings& shared) const
    {
        LocationConfig location = makeLocation("/");
        location.root = shared.root;
        location.indexFiles = shared.indexFiles;
        location.autoindex = shared.autoindex;
        location.maxBodySize = shared.maxBodySize;
        return location;
    }

    // Same interface:port twice, or 0.0.0.0 and a specific address on the
    // same port, cannot both be bound (design decision 4).
    void checkListenConflicts() const
    {
        for (std::size_t i = 0; i < listens_.size(); ++i) {
            for (std::size_t j = 0; j < i; ++j) {
                const ListenAddr& a = listens_[i].addr;
                const ListenAddr& b = listens_[j].addr;
                if (a.port != b.port)
                    continue;
                if (a.host == b.host)
                    throw ConfigError(source_, listens_[i].line, "duplicate listen " + describe(a));
                if (a.host == "0.0.0.0" || b.host == "0.0.0.0")
                    throw ConfigError(source_, listens_[i].line,
                                      "listen " + describe(a) + " conflicts with " + describe(b));
            }
        }
    }

    const DirectiveTree&     tree_;
    const std::string&       source_;
    std::vector<ListenEntry> listens_;
};

}  // namespace

std::vector<ServerConfig> buildServerConfigs(const DirectiveTree& tree, const std::string& source)
{
    ConfigBuilder builder(tree, source);
    return builder.build();
}
