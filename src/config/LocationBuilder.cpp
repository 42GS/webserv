#include "config/LocationBuilder.hpp"

#include "config/ConfigValues.hpp"
#include "config/DirectiveRules.hpp"
#include "http/Methods.hpp"
#include "http/StatusCodes.hpp"

namespace {

bool isRedirectCode(unsigned long code) {
  return code == http_status::kMovedPermanently ||
         code == http_status::kFound || code == http_status::kSeeOther ||
         code == http_status::kTemporaryRedirect ||
         code == http_status::kPermanentRedirect;
}

LocationConfig makeLocation(const std::string& path,
                            const SharedSettings& settings) {
  LocationConfig location;
  location.path = path;
  location.returnCode = 0;
  copySharedSettings(settings, location);
  return location;
}

void applyReturn(const BuildContext& context, const Directive& directive,
                 LocationConfig& location) {
  unsigned long code = 0;
  if (!parseUnsigned(directive.args[0], http_status::kMaxCode, code) ||
      !isRedirectCode(code))
    failAt(context, directive,
           "\"return\" code must be 301, 302, 303, 307 or 308");
  location.returnCode = static_cast<int>(code);
  location.returnUrl = directive.args[1];
}

// Accepts only the form "limit_except METHOD ... { deny all; }".
void applyLimitExcept(const BuildContext& context, const Directive& directive,
                      LocationConfig& location) {
  for (std::size_t i = 0; i < directive.args.size(); ++i) {
    if (!isImplementedMethod(directive.args[i]))
      failAt(context, directive,
             "unsupported method \"" + directive.args[i] +
                 "\" in \"limit_except\"");
    location.allowedMethods.insert(directive.args[i]);
  }
  // NGINX: allowing GET also allows HEAD.
  if (location.allowedMethods.count(methods::kGet) != 0)
    location.allowedMethods.insert(methods::kHead);
  const char* const onlyDeny =
      "\"limit_except\" block must contain only \"deny all;\"";
  if (directive.children.size() != 1) failAt(context, directive, onlyDeny);
  const Directive& deny = context.node(directive.children[0]);
  if (deny.name != "deny" || deny.isBlock || deny.args.size() != 1 ||
      deny.args[0] != "all")
    failAt(context, deny, onlyDeny);
}

void applyCgi(const BuildContext& context, const Directive& directive,
              LocationConfig& location) {
  const std::string& extension = directive.args[0];
  if (extension.size() < location_builder::kMinCgiExtensionLength ||
      extension[0] != '.')
    failAt(context, directive, "\"cgi\" extension must look like \".php\"");
  if (location.cgiByExtension.count(extension) != 0)
    failAt(context, directive,
           "duplicate \"cgi\" extension \"" + extension + "\"");
  location.cgiByExtension[extension] = directive.args[1];
}

// Applies a directive allowed only in location blocks. BlockChecker has
// already rejected names that are not in the directive table.
void applyLocationDirective(const BuildContext& context,
                            const Directive& directive,
                            LocationConfig& location) {
  if (directive.name == "alias")
    location.alias = directive.args[0];
  else if (directive.name == "return")
    applyReturn(context, directive, location);
  else if (directive.name == "limit_except")
    applyLimitExcept(context, directive, location);
  else if (directive.name == "upload_store")
    location.uploadStore = directive.args[0];
  else if (directive.name == "cgi")
    applyCgi(context, directive, location);
}

}  // namespace

LocationConfig buildLocation(const BuildContext& context,
                             const Directive& block,
                             const SharedSettings& server) {
  if (block.args.size() != 1)
    failAt(context, block,
           "\"location\" takes exactly one path (modifiers are not supported)");
  if (block.args[0].empty() || block.args[0][0] != '/')
    failAt(context, block, "location path must start with \"/\"");
  LocationConfig location = makeLocation(block.args[0], server);
  SharedSettings own = server;
  own.root.clear();  // to tell this location's own root from the inherited one
  BlockChecker checker(context.source, CONTEXT_LOCATION);
  for (std::size_t i = 0; i < block.children.size(); ++i) {
    const Directive& directive = context.node(block.children[i]);
    checker.check(directive);
    if (!applySharedDirective(context, directive, own))
      applyLocationDirective(context, directive, location);
  }
  copySharedSettings(own, location);
  if (!own.root.empty() && !location.alias.empty())
    failAt(context, block, "\"root\" and \"alias\" in the same location");
  if (location.alias.empty())
    location.root = own.root.empty() ? server.root : own.root;
  if (location.returnCode == 0 && location.root.empty() &&
      location.alias.empty())
    failAt(context, block,
           "location \"" + location.path + "\" has no \"root\" or \"alias\"");
  return location;
}

LocationConfig makeImplicitRootLocation(const SharedSettings& server) {
  LocationConfig location = makeLocation("/", server);
  location.root = server.root;
  return location;
}
