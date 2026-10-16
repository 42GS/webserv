#ifndef CONFIG_LOCATIONBUILDER_HPP
#define CONFIG_LOCATIONBUILDER_HPP

#include "config/BuildContext.hpp"
#include "config/ServerConfig.hpp"
#include "config/SharedSettings.hpp"

namespace location_builder {

const std::string::size_type kMinCgiExtensionLength = 2;  // ".x"

}  // namespace location_builder

// Builds one "location PATH { ... }" block on top of the server's settings.
LocationConfig buildLocation(const BuildContext& context,
                             const Directive& block,
                             const SharedSettings& server);

// The location "/" a server gets when it has a root but no "location /".
LocationConfig makeImplicitRootLocation(const SharedSettings& server);

#endif
