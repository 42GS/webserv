#ifndef CONFIG_SHAREDSETTINGS_HPP
#define CONFIG_SHAREDSETTINGS_HPP

#include "config/BuildContext.hpp"
#include "config/ServerConfig.hpp"

#include <cstddef>
#include <string>
#include <vector>

// Settings that may appear in both server and location blocks. A location
// starts from its server's values and overrides what it sets itself.
struct SharedSettings {
    std::string              root;
    std::vector<std::string> indexFiles;
    bool                     autoindex;
    std::size_t              maxBodySize;

    static SharedSettings defaults();
};

// Applies root, index, autoindex or client_max_body_size. Returns false if
// the directive is not one of them. The directive's shape is already checked.
bool applySharedDirective(const BuildContext& context, const Directive& directive, SharedSettings& settings);

// Copies index, autoindex and body size into the location. Root and alias
// are decided by the caller.
void copySharedSettings(const SharedSettings& settings, LocationConfig& location);

#endif
