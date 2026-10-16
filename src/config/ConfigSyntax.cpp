/**
 * @file  ConfigSyntax.cpp
 * @brief Recursive descent parser of the directive tree.
 *        지시어 트리를 만드는 재귀 하강 파서.
 */
#include "config/ConfigSyntax.hpp"

#include "config/ConfigError.hpp"

namespace {

/**
 * @brief Deepest block nesting: server > location > limit_except.
 *        가장 깊은 블록 중첩: server > location > limit_except.
 *
 * Deeper nesting is never valid, and limiting it keeps the recursive
 * parser from exhausting the stack.
 * 더 깊은 중첩은 쓸 일이 없고, 막아 두면 재귀 파서가 스택을 다 쓰지 않는다.
 */
const std::size_t kMaxBlockDepth = 3;

/**
 * @brief Recursive descent over the tokens; used by parseDirectives().
 *        토큰을 재귀 하강으로 읽는다. parseDirectives()가 쓴다.
 */
class SyntaxParser {
 public:
  /**
   * @brief Starts at the first token.
   *        첫 토큰부터 읽기 시작한다.
   * @param[in] tokens Tokens ending with TOKEN_END; kept by reference.
   *                   TOKEN_END로 끝나는 토큰. 참조로 보관한다.
   * @param[in] source Name used in error messages; kept by reference.
   *                   오류 메시지에 쓸 이름. 참조로 보관한다.
   */
  SyntaxParser(const std::vector<Token>& tokens, const std::string& source);

  /**
   * @brief Parses the whole input.
   *        입력 전체를 파싱한다.
   * @return The tree. / 지시어 트리.
   * @throws ConfigError See parseDirectives(). / parseDirectives() 참고.
   */
  DirectiveTree parseTopLevel();

 private:
  /// @cond
  // Not copyable: declared and never defined (C++98 has no "= delete").
  // 복사하지 않는다: 선언만 하고 정의하지 않는다 (C++98에는 "= delete"가 없다).
  SyntaxParser(const SyntaxParser&);
  SyntaxParser& operator=(const SyntaxParser&);
  /// @endcond

  /**
   * @brief Returns the token at the current position.
   *        현재 위치의 토큰을 돌려준다.
   * @return The token; TOKEN_END at the end. / 토큰. 끝에서는 TOKEN_END.
   */
  const Token& current() const;

  /**
   * @brief Parses directives until "}" or the end of input, without
   *        consuming it.
   *        "}"나 입력 끝까지 지시어를 파싱한다. 그 토큰은 소비하지 않는다.
   * @return Indexes of the parsed directives in tree_.nodes.
   *         파싱한 지시어들의 tree_.nodes 인덱스.
   */
  std::vector<std::size_t> parseList();

  /**
   * @brief Parses one directive, with its block if it has one.
   *        지시어 하나를, 블록이 있으면 블록까지 파싱한다.
   * @return Index of the directive in tree_.nodes.
   *         그 지시어의 tree_.nodes 인덱스.
   */
  std::size_t parseDirective();

  /**
   * @brief Appends a directive to tree_.nodes.
   *        지시어를 tree_.nodes 끝에 넣는다.
   * @param[in] directive The directive. / 넣을 지시어.
   * @return Its index. / 넣은 위치의 인덱스.
   */
  std::size_t store(const Directive& directive);

  /** Input. / 입력. */
  const std::vector<Token>& tokens_;
  /** Name for messages. / 메시지용 이름. */
  const std::string& source_;
  /** Current token. / 현재 토큰. */
  std::vector<Token>::size_type pos_;
  /** Blocks currently open. / 지금 열려 있는 블록 수. */
  std::size_t depth_;
  /** Result being built. / 만들고 있는 결과. */
  DirectiveTree tree_;
};

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

std::size_t SyntaxParser::store(const Directive& directive) {
  tree_.nodes.push_back(directive);
  return tree_.nodes.size() - 1;
}

}  // namespace

DirectiveTree parseDirectives(const std::vector<Token>& tokens,
                              const std::string& source) {
  SyntaxParser parser(tokens, source);
  return parser.parseTopLevel();
}
