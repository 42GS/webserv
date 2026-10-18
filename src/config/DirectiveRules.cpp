/**
 * @file  DirectiveRules.cpp
 * @brief The directive table and its shape checks.
 *        지시어 표와 그 표에 따른 모양 검사.
 */
#include "config/DirectiveRules.hpp"

#include <cstddef>

#include "config/ConfigError.hpp"

namespace {

/**
 * @brief One row of the directive table.
 *        지시어 표의 한 행.
 */
struct DirectiveRule {
  /** Directive name. / 지시어 이름. */
  const char* name;
  /** Allowed blocks, bits of kIn*. / 쓸 수 있는 블록 (kIn* 비트). */
  int contexts;
  /** Fewest arguments. / 최소 인자 수. */
  std::size_t minArgs;
  /** Most arguments, or kAnyCount. / 최대 인자 수, 또는 kAnyCount. */
  std::size_t maxArgs;
  /** Ends with a block instead of ";". / ";" 대신 블록으로 끝난다. */
  bool takesBlock;
  /** May appear more than once in a block. / 한 블록에 여러 번 나올 수 있다. */
  bool repeatable;
};

/** No upper limit on the argument count. / 인자 수에 상한이 없음. */
const std::size_t kAnyCount = static_cast<std::size_t>(-1);

/** Allowed in "server". / "server"에서 허용. */
const int kInServer = 1 << CONTEXT_SERVER;
/** Allowed in "location". / "location"에서 허용. */
const int kInLocation = 1 << CONTEXT_LOCATION;

/**
 * @brief Every directive the configuration accepts.
 *        설정이 받는 모든 지시어.
 *
 * "location" accepts any argument count here so that LocationBuilder can
 * report modifiers ("location = /x") with a specific message.
 * "location"의 인자 수를 여기서 제한하지 않는 이유는, modifier
 * ("location = /x")를 LocationBuilder가 구체적인 메시지로 알리게 하려는
 * 것이다.
 */
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

/** Number of rows in kRules. / kRules의 행 수. */
const std::size_t kRuleCount = sizeof(kRules) / sizeof(kRules[0]);

/**
 * @brief Looks up a directive by name.
 *        이름으로 지시어 행을 찾는다.
 * @param[in] name Directive name. / 지시어 이름.
 * @return The row, or 0 for an unknown name. / 해당 행. 모르는 이름이면 0.
 */
const DirectiveRule* findRule(const std::string& name) {
  for (std::size_t i = 0; i < kRuleCount; ++i) {
    if (name == kRules[i].name) return &kRules[i];
  }
  return 0;
}

/**
 * @brief Block name for error messages.
 *        오류 메시지에 쓸 블록 이름.
 * @param[in] context Block kind. / 블록 종류.
 * @return "server" or "location". / "server" 또는 "location".
 */
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
