#include "config/ConfigBuilder.hpp"

#include "config/BuildContext.hpp"
#include "config/ConfigError.hpp"
#include "config/ServerBuilder.hpp"

#include <sstream>

namespace {

const char* const kAnyAddress = "0.0.0.0";

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

// The same interface:port twice, or 0.0.0.0 and a specific address on the
// same port, cannot both be bound (design decision 4).
void checkListenConflicts(const std::vector<ListenEntry>& listens, const std::string& source)
{
    for (std::size_t i = 0; i < listens.size(); ++i) {
        for (std::size_t j = 0; j < i; ++j) {
            const ListenAddr& a = listens[i].addr;
            const ListenAddr& b = listens[j].addr;
            if (a.port != b.port)
                continue;
            if (a.host == b.host)
                throw ConfigError(source, listens[i].line, "duplicate listen " + describe(a));
            if (a.host == kAnyAddress || b.host == kAnyAddress)
                throw ConfigError(source, listens[i].line,
                                  "listen " + describe(a) + " conflicts with " + describe(b));
        }
    }
}

void collectListens(const BuiltServer& server, std::vector<ListenEntry>& listens)
{
    for (std::size_t i = 0; i < server.config.listens.size(); ++i) {
        ListenEntry entry;
        entry.addr = server.config.listens[i];
        entry.line = server.listenLines[i];
        listens.push_back(entry);
    }
}

}  // namespace

std::vector<ServerConfig> buildServerConfigs(const DirectiveTree& tree, const std::string& source)
{
    const BuildContext context = { tree, source };
    std::vector<ServerConfig> servers;
    std::vector<ListenEntry> listens;
    for (std::size_t i = 0; i < tree.topLevel.size(); ++i) {
        const Directive& directive = context.node(tree.topLevel[i]);
        if (directive.name != "server")
            failAt(context, directive, "unknown top-level directive \"" + directive.name
                                       + "\" (only \"server\" is allowed)");
        if (!directive.isBlock)
            failAt(context, directive, "\"server\" requires a block");
        const BuiltServer server = buildServer(context, directive);
        collectListens(server, listens);
        servers.push_back(server.config);
    }
    if (servers.empty())
        throw ConfigError(source, 1, "no \"server\" block");
    checkListenConflicts(listens, source);
    return servers;
}
