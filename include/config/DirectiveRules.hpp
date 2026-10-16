/**
 * @file  DirectiveRules.hpp
 * @brief Shape checks of directives against the directive table.
 *        지시어 표에 따라 지시어의 모양을 검사한다.
 */
#ifndef CONFIG_DIRECTIVERULES_HPP
#define CONFIG_DIRECTIVERULES_HPP

#include <set>
#include <string>

#include "config/ConfigSyntax.hpp"

/**
 * @brief The block a directive appears in.
 *        지시어가 들어 있는 블록.
 */
enum BlockContext {
  CONTEXT_SERVER,  /**< Inside "server { }". / "server { }" 안. */
  CONTEXT_LOCATION /**< Inside "location PATH { }". / "location PATH { }" 안. */
};

/**
 * @brief Checks the shape of each directive in one block.
 *        블록 하나에 든 지시어마다 모양을 검사한다.
 *
 * Shape means: known name, allowed in this block, argument count, block
 * or ";", and at most once unless repeatable. Values are checked by the
 * builders.
 * 모양이란 알려진 이름인지, 이 블록에서 쓸 수 있는지, 인자 개수, 블록과
 * ";" 중 맞는 쪽인지, 반복할 수 없는 지시어가 한 번만 나왔는지를 말한다.
 * 값은 builder가 검사한다.
 */
class BlockChecker {
 public:
  /**
   * @brief Starts checking a new block.
   *        새 블록 검사를 시작한다.
   * @param[in] source  Name used in error messages; kept by reference.
   *                    오류 메시지에 쓸 이름. 참조로 보관한다.
   * @param[in] context The kind of block. / 블록 종류.
   */
  BlockChecker(const std::string& source, BlockContext context);

  /**
   * @brief Checks one directive of the block.
   *        블록의 지시어 하나를 검사한다.
   * @param[in] directive The directive. / 검사할 지시어.
   * @throws ConfigError If the shape is wrong. / 모양이 틀리면.
   */
  void check(const Directive& directive);

 private:
  /** Name for messages. / 메시지용 이름. */
  const std::string& source_;
  /** Kind of block. / 블록 종류. */
  BlockContext context_;
  /** Names seen so far. / 지금까지 본 이름. */
  std::set<std::string> seen_;
};

#endif
