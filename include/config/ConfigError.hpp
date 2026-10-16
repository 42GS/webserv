#ifndef CONFIG_CONFIGERROR_HPP
#define CONFIG_CONFIGERROR_HPP

#include <stdexcept>
#include <string>

// A configuration problem, reported as "source:line: message"
// ("source: message" when line is 0, e.g. the file cannot be read).
class ConfigError : public std::runtime_error {
 public:
  ConfigError(const std::string& source, int line, const std::string& message);
};

#endif
