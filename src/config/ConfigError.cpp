#include "config/ConfigError.hpp"

#include <sstream>

namespace {

std::string formatMessage(const std::string& source, int line, const std::string& message)
{
    std::ostringstream out;
    out << source;
    if (line > 0)
        out << ":" << line;
    out << ": " << message;
    return out.str();
}

}  // namespace

ConfigError::ConfigError(const std::string& source, int line, const std::string& message)
    : std::runtime_error(formatMessage(source, line, message))
{
}
