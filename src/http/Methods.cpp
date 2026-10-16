#include "http/Methods.hpp"

#include <cstddef>

bool isImplementedMethod(const std::string& method) {
  for (std::size_t i = 0; i < methods::kMethodCount; ++i) {
    if (method == methods::kImplementedMethods[i]) return true;
  }
  return false;
}

std::string::size_type longestImplementedMethod() {
  std::string::size_type longest = 0;
  for (std::size_t i = 0; i < methods::kMethodCount; ++i) {
    const std::string::size_type length =
        std::string(methods::kImplementedMethods[i]).size();
    if (length > longest) longest = length;
  }
  return longest;
}
