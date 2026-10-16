#ifndef CONFIG_CONFIGLIMITS_HPP
#define CONFIG_CONFIGLIMITS_HPP

#include <cstddef>

namespace config_limits {

// NGINX default for client_max_body_size ("1m").
const std::size_t kDefaultMaxBodySize = 1024 * 1024;

// Request bodies are kept in memory, so the configured limit is capped.
// Running out of memory kills the process instead of throwing (design decision
// 11).
const std::size_t kMaxBodySizeCeiling = 100 * 1024 * 1024;

}  // namespace config_limits

#endif
