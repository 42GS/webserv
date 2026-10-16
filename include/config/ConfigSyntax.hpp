#ifndef CONFIG_CONFIGSYNTAX_HPP
#define CONFIG_CONFIGSYNTAX_HPP

#include <cstddef>
#include <string>
#include <vector>

#include "config/ConfigLexer.hpp"

// One "name args...;" or "name args... { children }" entry. Children are
// indexes into DirectiveTree::nodes, because C++98 does not allow a
// std::vector of an incomplete type as a member.
struct Directive {
  std::string name;
  std::vector<std::string> args;
  int line;
  bool isBlock;
  std::vector<std::size_t> children;
};

struct DirectiveTree {
  std::vector<Directive> nodes;
  std::vector<std::size_t> topLevel;
};

// Builds the directive tree from tokens. Only checks the shape (braces,
// semicolons); the meaning of each directive is checked by ConfigBuilder.
DirectiveTree parseDirectives(const std::vector<Token>& tokens,
                              const std::string& source);

namespace config_syntax {

// server > location > limit_except. Deeper nesting is never valid, and
// limiting it keeps the recursive parser from exhausting the stack.
const std::size_t kMaxBlockDepth = 3;

}  // namespace config_syntax

// Recursive descent over the tokens; used by parseDirectives().
class SyntaxParser {
 public:
  SyntaxParser(const std::vector<Token>& tokens, const std::string& source);
  DirectiveTree parseTopLevel();

 private:
  SyntaxParser(const SyntaxParser&);
  SyntaxParser& operator=(const SyntaxParser&);

  const Token& current() const;
  // Parses directives until "}" or the end of input, without consuming it.
  std::vector<std::size_t> parseList();
  // Returns the index of the parsed directive in tree_.nodes.
  std::size_t parseDirective();
  std::size_t store(const Directive& directive);

  const std::vector<Token>& tokens_;
  const std::string& source_;
  std::vector<Token>::size_type pos_;
  std::size_t depth_;  // blocks currently open
  DirectiveTree tree_;
};

#endif
