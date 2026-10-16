#ifndef CONFIG_SERVERBUILDER_HPP
#define CONFIG_SERVERBUILDER_HPP

#include <vector>

#include "config/BuildContext.hpp"
#include "config/ServerConfig.hpp"

namespace server_builder {

// error_page accepts codes 300..599 (chosen in step 1: redirect and error
// responses are the ones that get a page).
const unsigned long kMinErrorPageCode = 300;
const unsigned long kMaxErrorPageCode = 599;

}  // namespace server_builder

struct BuiltServer {
  ServerConfig config;
  // Line of each entry in config.listens, for error messages.
  std::vector<int> listenLines;
};

// Builds one "server { ... }" block, including its locations.
BuiltServer buildServer(const BuildContext& context, const Directive& block);

#endif
