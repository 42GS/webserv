/**
 * @file  ConfigBuilder.cpp
 * @brief Builds all servers and checks listen conflicts between them.
 *        모든 server를 만들고 server 사이의 listen 충돌을 검사한다.
 */
#include "config/ConfigBuilder.hpp"

#include "config/BuildContext.hpp"
#include "config/ConfigError.hpp"
#include "config/ConfigValues.hpp"
#include "config/ServerBuilder.hpp"

namespace {

/**
 * @brief A listen address with the line it came from, for conflict
 *        messages.
 *        충돌 메시지에 쓰려고 listen 주소와 그 줄 번호를 묶은 것.
 */
struct ListenEntry {
  /** Address. / 주소. */
  ListenAddr addr;
  /** Line of the "listen" directive. / "listen" 지시어의 줄. */
  int line;
};

/**
 * @brief Rejects listen addresses that overlap.
 *        겹치는 listen 주소를 거부한다.
 *
 * The same interface:port twice is an error without virtual hosts (design
 * decision 4). 0.0.0.0 and a specific address on the same port bind on
 * macOS but not on Ubuntu, so they are rejected on both (wiki
 * concepts/configuration-file "포트 관련 엣지 케이스").
 * 같은 인터페이스:포트가 두 번 나오면 virtual host가 없으므로 오류다 (설계
 * 결정 4). 같은 포트의 0.0.0.0과 특정 주소는 macOS에서는 둘 다 bind되고
 * Ubuntu에서는 안 되므로, 어느 쪽에서든 거부한다 (위키
 * concepts/configuration-file "포트 관련 엣지 케이스").
 *
 * @param[in] listens Every listen of every server. / 모든 server의 listen.
 * @param[in] source  Name used in error messages. / 오류 메시지에 쓸 이름.
 * @throws ConfigError At the later of two conflicting entries.
 *         충돌하는 두 항목 중 뒤의 것에서.
 */
void checkListenConflicts(const std::vector<ListenEntry>& listens,
                          const std::string& source) {
  for (std::size_t i = 0; i < listens.size(); ++i) {
    for (std::size_t j = 0; j < i; ++j) {
      const ListenAddr& a = listens[i].addr;
      const ListenAddr& b = listens[j].addr;
      if (a.port != b.port) continue;
      if (a.host == b.host)
        throw ConfigError(source, listens[i].line,
                          "duplicate listen " + formatListenAddr(a));
      if (a.host == config_values::kAnyAddress ||
          b.host == config_values::kAnyAddress)
        throw ConfigError(source, listens[i].line,
                          "listen " + formatListenAddr(a) + " conflicts with " +
                              formatListenAddr(b));
    }
  }
}

/**
 * @brief Appends a server's listens with their lines.
 *        server의 listen을 줄 번호와 함께 목록 끝에 넣는다.
 * @param[in]     server  Built server. / 만든 server.
 * @param[in,out] listens List to extend. / 늘릴 목록.
 */
void collectListens(const BuiltServer& server,
                    std::vector<ListenEntry>& listens) {
  for (std::size_t i = 0; i < server.config.listens.size(); ++i) {
    ListenEntry entry;
    entry.addr = server.config.listens[i];
    entry.line = server.listenLines[i];
    listens.push_back(entry);
  }
}

}  // namespace

std::vector<ServerConfig> buildServerConfigs(const DirectiveTree& tree,
                                             const std::string& source) {
  const BuildContext context = {tree, source};
  std::vector<ServerConfig> servers;
  std::vector<ListenEntry> listens;

  for (std::size_t i = 0; i < tree.topLevel.size(); ++i) {
    const Directive& directive = context.node(tree.topLevel[i]);
    if (directive.name != "server")
      throw ConfigError(context.source, directive.line,
                        "unknown top-level directive \"" + directive.name +
                            "\" (only \"server\" is allowed)");
    if (!directive.isBlock)
      throw ConfigError(context.source, directive.line,
                        "\"server\" requires a block");

    const BuiltServer server = buildServer(context, directive);
    collectListens(server, listens);
    servers.push_back(server.config);
  }

  if (servers.empty()) throw ConfigError(source, 1, "no \"server\" block");
  checkListenConflicts(listens, source);
  return servers;
}
