#include "config/BuildContext.hpp"

#include "config/ConfigError.hpp"

void failAt(const BuildContext& context, const Directive& directive,
            const std::string& message) {
  throw ConfigError(context.source, directive.line, message);
}
