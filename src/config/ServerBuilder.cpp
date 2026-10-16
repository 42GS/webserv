#include "config/ServerBuilder.hpp"

#include <algorithm>
#include <set>

#include "config/ConfigError.hpp"
#include "config/ConfigValues.hpp"
#include "config/DirectiveRules.hpp"
#include "config/LocationBuilder.hpp"
#include "config/SharedSettings.hpp"
#include "http/Ascii.hpp"
#include "http/StatusCodes.hpp"

namespace {

bool longerPathFirst(const LocationConfig& a, const LocationConfig& b) {
  return a.path.size() > b.path.size();
}

void applyListen(const BuildContext& context, const Directive& directive,
                 BuiltServer& server) {
  ListenAddr addr;
  // Only "IPv4:port" and "port" are accepted: every port is written in the
  // file, unlike NGINX's address-only form that implies port 80.
  if (!parseListenAddr(directive.args[0], addr))
    throw ConfigError(
        context.source, directive.line,
        "invalid listen address \"" + directive.args[0] +
            "\" (expected IPv4:port or port, e.g. 127.0.0.1:8080)");
  server.config.listens.push_back(addr);
  server.listenLines.push_back(directive.line);
}

// "error_page CODE ... URI"
void applyErrorPage(const BuildContext& context, const Directive& directive,
                    ServerConfig& server) {
  const std::string& uri = directive.args.back();
  if (uri.empty() || uri[0] != '/')
    throw ConfigError(context.source, directive.line,
                      "\"error_page\" URI must start with \"/\"");
  for (std::size_t i = 0; i + 1 < directive.args.size(); ++i) {
    unsigned long code = 0;
    if (!parseDecimal(directive.args[i], http_status::kMaxCode, code) ||
        code < server_builder::kMinErrorPageCode ||
        code > server_builder::kMaxErrorPageCode)
      throw ConfigError(
          context.source, directive.line,
          "invalid error_page code \"" + directive.args[i] + "\"");
    if (server.errorPages.count(static_cast<int>(code)) != 0)
      throw ConfigError(
          context.source, directive.line,
          "duplicate error_page code \"" + directive.args[i] + "\"");
    server.errorPages[static_cast<int>(code)] = uri;
  }
}

// Locations are built after the whole server block is read, so they inherit
// server values regardless of where those appear in the block.
void addLocations(const BuildContext& context,
                  const std::vector<const Directive*>& blocks,
                  const SharedSettings& settings, ServerConfig& server) {
  std::set<std::string> paths;
  for (std::size_t i = 0; i < blocks.size(); ++i) {
    const LocationConfig location =
        buildLocation(context, *blocks[i], settings);
    if (!paths.insert(location.path).second)
      throw ConfigError(context.source, blocks[i]->line,
                        "duplicate location \"" + location.path + "\"");
    server.locations.push_back(location);
  }
  if (paths.count("/") == 0 && !settings.root.empty())
    server.locations.push_back(makeImplicitRootLocation(settings));
  std::stable_sort(server.locations.begin(), server.locations.end(),
                   longerPathFirst);
}

}  // namespace

BuiltServer buildServer(const BuildContext& context, const Directive& block) {
  if (!block.args.empty())
    throw ConfigError(context.source, block.line,
                      "\"server\" takes no arguments");
  BuiltServer server;
  SharedSettings settings = SharedSettings::defaults();
  std::vector<const Directive*> locationBlocks;
  BlockChecker checker(context.source, CONTEXT_SERVER);
  for (std::size_t i = 0; i < block.children.size(); ++i) {
    const Directive& directive = context.node(block.children[i]);
    checker.check(directive);
    if (directive.name == "location")
      locationBlocks.push_back(&directive);
    else if (directive.name == "listen")
      applyListen(context, directive, server);
    else if (directive.name == "error_page")
      applyErrorPage(context, directive, server.config);
    else
      // BlockChecker allowed it in "server", so it is a shared directive.
      static_cast<void>(applySharedDirective(context, directive, settings));
  }
  if (server.config.listens.empty())
    throw ConfigError(context.source, block.line,
                      "\"server\" has no \"listen\"");
  addLocations(context, locationBlocks, settings, server.config);
  return server;
}
