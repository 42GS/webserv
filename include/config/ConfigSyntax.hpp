/**
 * @file  ConfigSyntax.hpp
 * @brief Second stage of configuration loading: tokens to a directive tree.
 *        설정 읽기의 두 번째 단계: 토큰을 지시어 트리로 만든다.
 */
#ifndef CONFIG_CONFIGSYNTAX_HPP
#define CONFIG_CONFIGSYNTAX_HPP

#include <cstddef>
#include <string>
#include <vector>

#include "config/ConfigLexer.hpp"

/**
 * @brief One "name args...;" or "name args... { children }" entry.
 *        "name args...;" 또는 "name args... { children }" 항목 하나.
 *
 * Children are indexes into DirectiveTree::nodes, because C++98 does not
 * allow a std::vector of an incomplete type as a member.
 * 자식은 DirectiveTree::nodes의 인덱스로 둔다. C++98에서는 불완전
 * 타입의 std::vector를 멤버로 둘 수 없기 때문이다.
 */
struct Directive {
  /** Directive name. / 지시어 이름. */
  std::string name;
  /** Arguments. / 인자. */
  std::vector<std::string> args;
  /** Line of the name. / 이름이 있는 줄. */
  int line;
  /** Ends with a block, not ";". / ";"가 아니라 블록으로 끝남. */
  bool isBlock;
  /** Indexes of the block's entries. / 블록 안 항목의 인덱스. */
  std::vector<std::size_t> children;
};

/**
 * @brief Every directive of a file, flattened into one vector.
 *        파일의 모든 지시어를 벡터 하나에 펼쳐 둔 것.
 */
struct DirectiveTree {
  /** All directives. / 모든 지시어. */
  std::vector<Directive> nodes;
  /** Indexes of the top-level ones. / 최상위 지시어의 인덱스. */
  std::vector<std::size_t> topLevel;
};

/**
 * @brief Builds the directive tree from tokens.
 *        토큰으로 지시어 트리를 만든다.
 *
 * Only the shape (braces, semicolons) is checked here; the meaning of
 * each directive is checked by buildServerConfigs().
 * 여기서는 모양(중괄호, 세미콜론)만 검사한다. 지시어의 의미는
 * buildServerConfigs()가 검사한다.
 *
 * @param[in] tokens Output of tokenizeConfig(). / tokenizeConfig()의 결과.
 * @param[in] source Name used in error messages.
 *                   오류 메시지에 쓸 이름.
 * @return The tree. / 지시어 트리.
 * @throws ConfigError On a missing ";" or "}", a stray "}", or blocks
 *         nested too deeply.
 *         ";"나 "}"가 빠졌거나, 짝 없는 "}"가 있거나, 블록이 너무 깊으면.
 */
DirectiveTree parseDirectives(const std::vector<Token>& tokens,
                              const std::string& source);

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

#endif
