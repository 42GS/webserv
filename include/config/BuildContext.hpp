/**
 * @file  BuildContext.hpp
 * @brief Input shared by the configuration builders.
 *        설정 builder들이 함께 쓰는 입력.
 */
#ifndef CONFIG_BUILDCONTEXT_HPP
#define CONFIG_BUILDCONTEXT_HPP

#include <string>

#include "config/ConfigSyntax.hpp"

/**
 * @brief What every builder needs: the directive tree and the source name
 *        for error messages.
 *        모든 builder에 필요한 것: 지시어 트리와 오류 메시지에 쓸 소스 이름.
 *
 * Both members are references; the context lives only while the tree and
 * the name are being built from.
 * 두 멤버 모두 참조라서, context는 트리와 이름으로 설정을 만드는 동안만 쓴다.
 */
struct BuildContext {
  /** Parsed directives. / 파싱한 지시어. */
  const DirectiveTree& tree;
  /** File name for messages. / 메시지용 파일 이름. */
  const std::string& source;

  /**
   * @brief Returns the directive stored at @p index of the tree.
   *        트리의 @p index 위치에 있는 지시어를 돌려준다.
   * @param[in] index Index into DirectiveTree::nodes.
   *                  DirectiveTree::nodes의 인덱스.
   * @return The directive. / 해당 지시어.
   */
  const Directive& node(std::size_t index) const { return tree.nodes[index]; }
};

#endif
