#include "config/DirectiveRules.hpp"

#include <cstddef>

#include "config/ConfigError.hpp"

namespace {

// One row of the directive table.
struct DirectiveRule {
  const char* name;
  int contexts;  // bit set of kIn*
  std::size_t minArgs;
  std::size_t maxArgs;
  bool takesBlock;
  bool repeatable;
};

const std::size_t kAnyCount = static_cast<std::size_t>(-1);

const int kInServer = 1 << CONTEXT_SERVER;
const int kInLocation = 1 << CONTEXT_LOCATION;

// "location" accepts any argument count here so that LocationBuilder can
// report modifiers ("location = /x") with a specific message.
// clang-format off
const DirectiveRule kRules[] = {
    // name                    contexts                 min  max        block  repeat
    {"listen",               kInServer,               1,   1,         false, true},
    {"error_page",           kInServer,               2,   kAnyCount, false, true},
    {"location",             kInServer,               0,   kAnyCount, true,  true},
    {"root",                 kInServer | kInLocation, 1,   1,         false, false},
    {"index",                kInServer | kInLocation, 1,   kAnyCount, false, false},
    {"autoindex",            kInServer | kInLocation, 1,   1,         false, false},
    {"client_max_body_size", kInServer | kInLocation, 1,   1,         false, false},
    {"alias",                kInLocation,             1,   1,         false, false},
    {"return",               kInLocation,             2,   2,         false, false},
    {"limit_except",         kInLocation,             1,   kAnyCount, true,  false},
    {"upload_store",         kInLocation,             1,   1,         false, false},
    {"cgi",                  kInLocation,             2,   2,         false, true},
};
// clang-format on

const std::size_t kRuleCount = sizeof(kRules) / sizeof(kRules[0]);

const DirectiveRule* findRule(const std::string& name) {
  for (std::size_t i = 0; i < kRuleCount; ++i) {
    if (name == kRules[i].name) return &kRules[i];
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
