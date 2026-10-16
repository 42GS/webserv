#ifndef CONFIG_DIRECTIVERULES_HPP
#define CONFIG_DIRECTIVERULES_HPP

#include "config/ConfigSyntax.hpp"

#include <set>
#include <string>

enum BlockContext {
    CONTEXT_SERVER,
    CONTEXT_LOCATION
};

// Checks the shape of each directive in one block against the directive
// table: known name, allowed in this block, argument count, block or ";",
// and at most once unless repeatable. Values are checked by the builders.
class BlockChecker {
public:
    BlockChecker(const std::string& source, BlockContext context);
    void check(const Directive& directive);   // throws ConfigError

private:
    const std::string&    source_;
    BlockContext          context_;
    std::set<std::string> seen_;
};

#endif
