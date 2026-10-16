#include "config/DirectiveRules.hpp"

#include "config/ConfigError.hpp"

namespace {

const DirectiveRule* findRule(const std::string& name) {
  for (std::size_t i = 0; i < directive_rules::kRuleCount; ++i) {
    if (name == directive_rules::kRules[i].name)
      return &directive_rules::kRules[i];
  }
  return 0;
}

const char* contextName(BlockContext context) {
  return context == CONTEXT_SERVER ? "server" : "location";
}

}  // namespace

BlockChecker::BlockChecker(const std::string& source, BlockContext context)
    : source_(source), context_(context) {}

void BlockChecker::check(const Directive& directive) {
  const std::string quoted = "\"" + directive.name + "\"";
  const DirectiveRule* rule = findRule(directive.name);
  if (rule == 0)
    throw ConfigError(source_, directive.line, "unknown directive " + quoted);
  if ((rule->contexts & (1 << context_)) == 0)
    throw ConfigError(
        source_, directive.line,
        quoted + " is not allowed in \"" + contextName(context_) + "\"");
  if (rule->takesBlock && !directive.isBlock)
    throw ConfigError(source_, directive.line, quoted + " requires a block");
  if (!rule->takesBlock && directive.isBlock)
    throw ConfigError(source_, directive.line,
                      quoted + " does not take a block");
  if (directive.args.size() < rule->minArgs ||
      directive.args.size() > rule->maxArgs)
    throw ConfigError(source_, directive.line,
                      "invalid number of arguments in " + quoted);
  if (!rule->repeatable && !seen_.insert(directive.name).second)
    throw ConfigError(source_, directive.line, "duplicate " + quoted);
}
