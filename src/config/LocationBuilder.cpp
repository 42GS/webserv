/**
 * @file  LocationBuilder.cpp
 * @brief Location blocks and the directives allowed only in them.
 *        location 블록과 그 안에서만 쓰는 지시어.
 */
#include "config/LocationBuilder.hpp"

#include "config/ConfigError.hpp"
#include "config/ConfigValues.hpp"
#include "config/DirectiveRules.hpp"
#include "http/Ascii.hpp"
#include "http/Methods.hpp"
#include "http/StatusCodes.hpp"

namespace {

/**
 * @brief True for the codes "return" accepts: 301, 302, 303, 307, 308.
 *        "return"이 받는 코드이면 true: 301, 302, 303, 307, 308.
 * @param[in] code Status code. / 상태 코드.
 * @return Whether it is one of them. / 그중 하나인지.
 */
bool isRedirectCode(unsigned long code) {
  return code == http_status::kMovedPermanently ||
         code == http_status::kFound || code == http_status::kSeeOther ||
         code == http_status::kTemporaryRedirect ||
         code == http_status::kPermanentRedirect;
}

/**
 * @brief A location at @p path with the shared values of @p settings and
 *        no redirect.
 *        @p path 위치에 @p settings의 공통 값을 가지고 redirect가 없는
 *        location.
 *
 * Without "limit_except" a location allows only GET and HEAD, so that
 * methods that change files must be listed (design decision 16).
 * "limit_except"가 없으면 GET과 HEAD만 허용한다. 파일을 바꾸는 메서드는
 * 적어야 열린다 (설계 결정 16).
 *
 * @param[in] path     Location prefix. / location prefix.
 * @param[in] settings Shared values to copy. / 복사할 공통 값.
 * @return The location, without root or alias.
 *         root와 alias가 아직 없는 location.
 */
LocationConfig makeLocation(const std::string& path,
                            const SharedSettings& settings) {
  LocationConfig location;
  location.path = path;
  location.allowedMethods.insert(methods::kGet);
  location.allowedMethods.insert(methods::kHead);
  location.returnCode = 0;
  copySharedSettings(settings, location);
  return location;
}

/**
 * @brief Applies "return CODE URL".
 *        "return CODE URL"을 적용한다.
 * @param[in]     context   Tree and source name. / 트리와 소스 이름.
 * @param[in]     directive The directive. / 지시어.
 * @param[in,out] location  Location to update. / 고칠 location.
 * @throws ConfigError If CODE is not a redirect code.
 *         CODE가 redirect 코드가 아니면.
 */
void applyReturn(const BuildContext& context, const Directive& directive,
                 LocationConfig& location) {
  unsigned long code = 0;
  if (!parseDecimal(directive.args[0], http_status::kMaxCode, code) ||
      !isRedirectCode(code))
    throw ConfigError(context.source, directive.line,
                      "\"return\" code must be 301, 302, 303, 307 or 308");
  location.returnCode = static_cast<int>(code);
  location.returnUrl = directive.args[1];
}

/**
 * @brief Applies "limit_except METHOD ... { deny all; }", the only form
 *        accepted.
 *        받아들이는 유일한 형태인 "limit_except METHOD ... { deny all; }"을
 *        적용한다.
 * @param[in]     context   Tree and source name. / 트리와 소스 이름.
 * @param[in]     directive The directive. / 지시어.
 * @param[in,out] location  Location whose allowedMethods is filled.
 *                          allowedMethods를 채울 location.
 * @throws ConfigError For an unimplemented method or another block body.
 *         구현하지 않은 메서드이거나 블록 내용이 다르면.
 */
void applyLimitExcept(const BuildContext& context, const Directive& directive,
                      LocationConfig& location) {
  location.allowedMethods.clear();
  for (std::size_t i = 0; i < directive.args.size(); ++i) {
    if (!isImplementedMethod(directive.args[i]))
      throw ConfigError(context.source, directive.line,
                        "unsupported method \"" + directive.args[i] +
                            "\" in \"limit_except\"");
    location.allowedMethods.insert(directive.args[i]);
  }

  // NGINX: allowing GET also allows HEAD.
  // NGINX와 같이 GET을 허용하면 HEAD도 허용한다.
  if (location.allowedMethods.count(methods::kGet) != 0)
    location.allowedMethods.insert(methods::kHead);

  const char* const onlyDeny =
      "\"limit_except\" block must contain only \"deny all;\"";
  if (directive.children.size() != 1)
    throw ConfigError(context.source, directive.line, onlyDeny);
  const Directive& deny = context.node(directive.children[0]);
  if (deny.name != "deny" || deny.isBlock || deny.args.size() != 1 ||
      deny.args[0] != "all")
    throw ConfigError(context.source, deny.line, onlyDeny);
}

/**
 * @brief Applies "cgi .EXT INTERPRETER".
 *        "cgi .EXT INTERPRETER"를 적용한다.
 * @param[in]     context   Tree and source name. / 트리와 소스 이름.
 * @param[in]     directive The directive. / 지시어.
 * @param[in,out] location  Location to update. / 고칠 location.
 * @throws ConfigError If the extension is not ".x" or longer or repeats,
 *         or the program path is not absolute.
 *         확장자가 ".x" 이상의 형태가 아니거나 중복되거나, 프로그램 경로가
 *         절대 경로가 아니면.
 */
void applyCgi(const BuildContext& context, const Directive& directive,
              LocationConfig& location) {
  const std::string& extension = directive.args[0];
  if (extension.size() < 2 || extension[0] != '.')  // at least ".x"
    throw ConfigError(context.source, directive.line,
                      "\"cgi\" extension must look like \".php\"");
  // The script runs in its own directory, where a relative program path
  // would no longer be found (design 9).
  // 스크립트는 자기 디렉토리에서 실행되므로, 상대 경로 프로그램은 거기서
  // 찾을 수 없다 (설계 9절).
  if (directive.args[1].empty() || directive.args[1][0] != '/')
    throw ConfigError(context.source, directive.line,
                      "\"cgi\" program must be an absolute path");
  if (location.cgiByExtension.count(extension) != 0)
    throw ConfigError(context.source, directive.line,
                      "duplicate \"cgi\" extension \"" + extension + "\"");
  location.cgiByExtension[extension] = directive.args[1];
}

/**
 * @brief Applies a directive allowed only in location blocks.
 *        location 블록에서만 쓰는 지시어를 적용한다.
 *
 * BlockChecker has already rejected names that are not in the directive
 * table.
 * 지시어 표에 없는 이름은 BlockChecker가 이미 거부했다.
 *
 * @param[in]     context   Tree and source name. / 트리와 소스 이름.
 * @param[in]     directive The directive. / 지시어.
 * @param[in,out] location  Location to update. / 고칠 location.
 * @throws ConfigError For an invalid value. / 값이 잘못됐으면.
 */
void applyLocationDirective(const BuildContext& context,
                            const Directive& directive,
                            LocationConfig& location) {
  if (directive.name == "alias")
    location.alias = directive.args[0];
  else if (directive.name == "return")
    applyReturn(context, directive, location);
  else if (directive.name == "limit_except")
    applyLimitExcept(context, directive, location);
  else if (directive.name == "upload_store")
    location.uploadStore = directive.args[0];
  else if (directive.name == "cgi")
    applyCgi(context, directive, location);
}

}  // namespace

LocationConfig buildLocation(const BuildContext& context,
                             const Directive& block,
                             const SharedSettings& server) {
  if (block.args.size() != 1)
    throw ConfigError(
        context.source, block.line,
        "\"location\" takes exactly one path (modifiers are not supported)");
  if (block.args[0].empty() || block.args[0][0] != '/')
    throw ConfigError(context.source, block.line,
                      "location path must start with \"/\"");

  LocationConfig location = makeLocation(block.args[0], server);
  SharedSettings own = server;
  // Cleared to tell this location's own root from the inherited one.
  // 이 location이 직접 지정한 root와 물려받은 root를 구분하려고 비운다.
  own.root.clear();

  BlockChecker checker(context.source, CONTEXT_LOCATION);
  for (std::size_t i = 0; i < block.children.size(); ++i) {
    const Directive& directive = context.node(block.children[i]);
    checker.check(directive);
    if (!applySharedDirective(context, directive, own))
      applyLocationDirective(context, directive, location);
  }

  copySharedSettings(own, location);
  if (!own.root.empty() && !location.alias.empty())
    throw ConfigError(context.source, block.line,
                      "\"root\" and \"alias\" in the same location");
  if (location.alias.empty())
    location.root = own.root.empty() ? server.root : own.root;
  if (location.returnCode == 0 && location.root.empty() &&
      location.alias.empty())
    throw ConfigError(
        context.source, block.line,
        "location \"" + location.path + "\" has no \"root\" or \"alias\"");
  return location;
}

LocationConfig makeImplicitRootLocation(const SharedSettings& server) {
  LocationConfig location = makeLocation("/", server);
  location.root = server.root;
  return location;
}
