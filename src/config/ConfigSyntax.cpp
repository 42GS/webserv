#include "config/ConfigSyntax.hpp"

#include "config/ConfigError.hpp"

namespace {

// server > location > limit_except. Deeper nesting is never valid, and
// limiting it keeps the recursion below from exhausting the stack.
const std::size_t kMaxBlockDepth = 3;

class SyntaxParser {
 public:
  SyntaxParser(const std::vector<Token>& tokens, const std::string& source)
      : tokens_(tokens), source_(source), pos_(0), depth_(0) {}

  DirectiveTree parseTopLevel() {
    tree_.topLevel = parseList();
    if (current().type == TOKEN_CLOSE_BRACE)
      throw ConfigError(source_, current().line, "unexpected \"}\"");
    return tree_;
  }

 private:
  const Token& current() const { return tokens_[pos_]; }

  // Parses directives until "}" or the end of input, without consuming it.
  std::vector<std::size_t> parseList() {
    std::vector<std::size_t> indexes;
    while (current().type != TOKEN_END && current().type != TOKEN_CLOSE_BRACE)
      indexes.push_back(parseDirective());
    return indexes;
  }

  // Returns the index of the parsed directive in tree_.nodes.
  std::size_t parseDirective() {
    if (current().type != TOKEN_WORD)
      throw ConfigError(
          source_, current().line,
          "expected a directive name, found \"" + current().text + "\"");
    Directive directive;
    directive.name = current().text;
    directive.line = current().line;
    directive.isBlock = false;
    ++pos_;
    while (current().type == TOKEN_WORD) {
      directive.args.push_back(current().text);
      ++pos_;
    }
    if (current().type == TOKEN_SEMICOLON) {
      ++pos_;
      return store(directive);
    }
    if (current().type == TOKEN_OPEN_BRACE) {
      if (depth_ == kMaxBlockDepth)
        throw ConfigError(source_, directive.line,
                          "blocks are nested too deeply");
      ++pos_;
      directive.isBlock = true;
      ++depth_;
      directive.children = parseList();
      --depth_;
      if (current().type != TOKEN_CLOSE_BRACE)
        throw ConfigError(source_, directive.line,
                          "block \"" + directive.name + "\" is not closed");
      ++pos_;
      return store(directive);
    }
    throw ConfigError(
        source_, directive.line,
        "directive \"" + directive.name + "\" is not terminated by \";\"");
  }

  std::size_t store(const Directive& directive) {
    tree_.nodes.push_back(directive);
    return tree_.nodes.size() - 1;
  }

  const std::vector<Token>& tokens_;
  const std::string& source_;
  std::vector<Token>::size_type pos_;
  std::size_t depth_;  // blocks currently open
  DirectiveTree tree_;
};

}  // namespace

DirectiveTree parseDirectives(const std::vector<Token>& tokens,
                              const std::string& source) {
  SyntaxParser parser(tokens, source);
  return parser.parseTopLevel();
}
