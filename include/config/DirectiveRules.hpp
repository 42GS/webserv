#ifndef CONFIG_DIRECTIVERULES_HPP
#define CONFIG_DIRECTIVERULES_HPP

#include <cstddef>
#include <set>
#include <string>

#include "config/ConfigSyntax.hpp"

enum BlockContext { CONTEXT_SERVER, CONTEXT_LOCATION };

// One row of the directive table.
struct DirectiveRule {
  const char* name;
  int contexts;  // bit set of directive_rules::kIn*
  std::size_t minArgs;
  std::size_t maxArgs;
  bool takesBlock;
  bool repeatable;
};

namespace directive_rules {

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

}  // namespace directive_rules

// Checks the shape of each directive in one block against the directive
// table: known name, allowed in this block, argument count, block or ";",
// and at most once unless repeatable. Values are checked by the builders.
class BlockChecker {
 public:
  BlockChecker(const std::string& source, BlockContext context);
  void check(const Directive& directive);  // throws ConfigError

 private:
  const std::string& source_;
  BlockContext context_;
  std::set<std::string> seen_;
};

#endif
