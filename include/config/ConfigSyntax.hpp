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

#endif
