#ifndef CONFIG_CONFIGSYNTAX_HPP
#define CONFIG_CONFIGSYNTAX_HPP

#include "config/ConfigLexer.hpp"

#include <cstddef>
#include <string>
#include <vector>

// One "name args...;" or "name args... { children }" entry. Children are
// indexes into DirectiveTree::nodes, because C++98 does not allow a
// std::vector of an incomplete type as a member.
struct Directive {
    std::string              name;
    std::vector<std::string> args;
    int                      line;
    bool                     isBlock;
    std::vector<std::size_t> children;
};

struct DirectiveTree {
    std::vector<Directive>   nodes;
    std::vector<std::size_t> topLevel;
};

// Builds the directive tree from tokens. Only checks the shape (braces,
// semicolons); the meaning of each directive is checked by ConfigBuilder.
DirectiveTree parseDirectives(const std::vector<Token>& tokens, const std::string& source);

#endif
