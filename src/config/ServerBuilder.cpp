/**
 * @file  ServerBuilder.cpp
 * @brief Server blocks and the directives allowed only in them.
 *        server 블록과 그 안에서만 쓰는 지시어.
 */
#include "config/ServerBuilder.hpp"

#include <algorithm>
#include <set>

#include "config/ConfigError.hpp"
#include "config/ConfigValues.hpp"
#include "config/DirectiveRules.hpp"
#include "config/LocationBuilder.hpp"
#include "config/SharedSettings.hpp"
#include "http/Ascii.hpp"
#include "http/StatusCodes.hpp"

namespace {

/**
 * @brief Smallest code error_page accepts.
 *        error_page가 받는 가장 작은 코드.
 *
 * error_page accepts codes 300..599 (chosen in step 1: redirect and error
 * responses are the ones that get a page).
 * error_page는 300..599를 받는다 (1단계에서 정함: 페이지가 붙는 것은
 * redirect와 오류 응답이다).
 */
const unsigned long kMinErrorPageCode = 300;
/** Largest code error_page accepts. / error_page가 받는 가장 큰 코드. */
const unsigned long kMaxErrorPageCode = 599;

/**
 * @brief Sort order of locations: longer path first.
 *        location 정렬 순서: 경로가 긴 것이 먼저.
 * @param[in] a Location. / location.
 * @param[in] b Location. / location.
 * @return True if @p a goes before @p b. / @p a가 @p b보다 앞이면 true.
 */
bool longerPathFirst(const LocationConfig& a, const LocationConfig& b) {
  return a.path.size() > b.path.size();
}

/**
 * @brief Applies "listen ADDRESS".
 *        "listen ADDRESS"를 적용한다.
 * @param[in]     context   Tree and source name. / 트리와 소스 이름.
 * @param[in]     directive The directive. / 지시어.
 * @param[in,out] server    Server to update, with the line kept for
 *                          conflict messages.
 *                          고칠 server. 충돌 메시지용으로 줄 번호도 남긴다.
 * @throws ConfigError For an invalid address. / 주소가 잘못됐으면.
 */
void applyListen(const BuildContext& context, const Directive& directive,
                 BuiltServer& server) {
  ListenAddr addr;
  // Only "IPv4:port" and "port" are accepted: every port is written in the
  // file, unlike NGINX's address-only form that implies port 80.
  // "IPv4:port"와 "port"만 받는다. 주소만 쓰면 포트 80이 되는 NGINX와
  // 달리, 포트는 항상 파일에 적혀 있어야 한다.
  if (!parseListenAddr(directive.args[0], addr))
    throw ConfigError(
        context.source, directive.line,
        "invalid listen address \"" + directive.args[0] +
            "\" (expected IPv4:port or port, e.g. 127.0.0.1:8080)");
  server.config.listens.push_back(addr);
  server.listenLines.push_back(directive.line);
}

/**
 * @brief Applies "error_page CODE ... URI".
 *        "error_page CODE ... URI"를 적용한다.
 * @param[in]     context   Tree and source name. / 트리와 소스 이름.
 * @param[in]     directive The directive. / 지시어.
 * @param[in,out] server    Server whose errorPages is filled.
 *                          errorPages를 채울 server.
 * @throws ConfigError For a URI not starting with "/", a code outside
 *         300..599, or a code given twice.
 *         URI가 "/"로 시작하지 않거나, 코드가 300..599 밖이거나, 같은
 *         코드가 두 번 나오면.
 */
void applyErrorPage(const BuildContext& context, const Directive& directive,
                    ServerConfig& server) {
  const std::string& uri = directive.args.back();
  if (uri.empty() || uri[0] != '/')
    throw ConfigError(context.source, directive.line,
                      "\"error_page\" URI must start with \"/\"");

  for (std::size_t i = 0; i + 1 < directive.args.size(); ++i) {
    unsigned long code = 0;
    if (!parseDecimal(directive.args[i], http_status::kMaxCode, code) ||
        code < kMinErrorPageCode || code > kMaxErrorPageCode)
      throw ConfigError(
          context.source, directive.line,
          "invalid error_page code \"" + directive.args[i] + "\"");
    if (server.errorPages.count(static_cast<int>(code)) != 0)
      throw ConfigError(
          context.source, directive.line,
          "duplicate error_page code \"" + directive.args[i] + "\"");
    server.errorPages[static_cast<int>(code)] = uri;
  }
}

/**
 * @brief Builds the location blocks and sorts them longest path first.
 *        location 블록들을 만들고 경로가 긴 것부터 정렬한다.
 *
 * Locations are built after the whole server block is read, so they
 * inherit server values regardless of where those appear in the block.
 * server 블록을 다 읽은 뒤에 location을 만들기 때문에, server 값이 블록
 * 어디에 있든 location이 물려받는다.
 *
 * @param[in]     context  Tree and source name. / 트리와 소스 이름.
 * @param[in]     blocks   The "location" directives. / "location" 지시어들.
 * @param[in]     settings Server values to inherit. / 물려줄 server 값.
 * @param[in,out] server   Server whose locations are filled.
 *                         locations를 채울 server.
 * @throws ConfigError For an invalid or duplicate location.
 *         location이 잘못됐거나 중복되면.
 */
void addLocations(const BuildContext& context,
                  const std::vector<const Directive*>& blocks,
                  const SharedSettings& settings, ServerConfig& server) {
  std::set<std::string> paths;
  for (std::size_t i = 0; i < blocks.size(); ++i) {
    const LocationConfig location =
        buildLocation(context, *blocks[i], settings);
    if (!paths.insert(location.path).second)
      throw ConfigError(context.source, blocks[i]->line,
                        "duplicate location \"" + location.path + "\"");
    server.locations.push_back(location);
  }

  if (paths.count("/") == 0 && !settings.root.empty())
    server.locations.push_back(makeImplicitRootLocation(settings));
  std::stable_sort(server.locations.begin(), server.locations.end(),
                   longerPathFirst);
}

}  // namespace

BuiltServer buildServer(const BuildContext& context, const Directive& block) {
  if (!block.args.empty())
    throw ConfigError(context.source, block.line,
                      "\"server\" takes no arguments");

  BuiltServer server;
  SharedSettings settings = SharedSettings::defaults();
  std::vector<const Directive*> locationBlocks;
  BlockChecker checker(context.source, CONTEXT_SERVER);
  for (std::size_t i = 0; i < block.children.size(); ++i) {
    const Directive& directive = context.node(block.children[i]);
    checker.check(directive);
    if (directive.name == "location")
      locationBlocks.push_back(&directive);
    else if (directive.name == "listen")
      applyListen(context, directive, server);
    else if (directive.name == "error_page")
      applyErrorPage(context, directive, server.config);
    else
      // BlockChecker allowed it in "server", so it is a shared directive.
      // BlockChecker가 "server"에서 허용했으므로 공통 지시어다.
      static_cast<void>(applySharedDirective(context, directive, settings));
  }

  if (server.config.listens.empty())
    throw ConfigError(context.source, block.line,
                      "\"server\" has no \"listen\"");
  addLocations(context, locationBlocks, settings, server.config);
  return server;
}
