#ifndef CONFIG_BUILDCONTEXT_HPP
#define CONFIG_BUILDCONTEXT_HPP

#include <string>

#include "config/ConfigSyntax.hpp"

// What every builder needs: the directive tree and the source name for errors.
struct BuildContext {
  const DirectiveTree& tree;
  const std::string& source;

  const Directive& node(std::size_t index) const { return tree.nodes[index]; }
};

// Throws ConfigError at the directive's line.
void failAt(const BuildContext& context, const Directive& directive,
            const std::string& message);

#endif
