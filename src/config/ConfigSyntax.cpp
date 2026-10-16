#include "config/ConfigSyntax.hpp"

#include "config/ConfigError.hpp"

SyntaxParser::SyntaxParser(const std::vector<Token>& tokens,
                           const std::string& source)
    : tokens_(tokens), source_(source), pos_(0), depth_(0) {}

DirectiveTree SyntaxParser::parseTopLevel() {
  tree_.topLevel = parseList();
  if (current().type == TOKEN_CLOSE_BRACE)
    throw ConfigError(source_, current().line, "unexpected \"}\"");
  return tree_;
}

const Token& SyntaxParser::current() const { return tokens_[pos_]; }

std::vector<std::size_t> SyntaxParser::parseList() {
  std::vector<std::size_t> indexes;
  while (current().type != TOKEN_END && current().type != TOKEN_CLOSE_BRACE)
    indexes.push_back(parseDirective());
  return indexes;
}

std::size_t SyntaxParser::parseDirective() {
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
    if (depth_ == config_syntax::kMaxBlockDepth)
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

std::size_t SyntaxParser::store(const Directive& directive) {
  tree_.nodes.push_back(directive);
  return tree_.nodes.size() - 1;
}

DirectiveTree parseDirectives(const std::vector<Token>& tokens,
                              const std::string& source) {
  SyntaxParser parser(tokens, source);
  return parser.parseTopLevel();
}
