/**
 * @file  Methods.cpp
 * @brief The list of implemented methods.
 *        구현한 메서드 목록.
 */
#include "http/Methods.hpp"

namespace methods {

const char* const kImplemented[] = {kGet, kHead, kPost, kDelete};
const std::size_t kImplementedCount =
    sizeof(kImplemented) / sizeof(kImplemented[0]);

}  // namespace methods

bool isImplementedMethod(const std::string& method) {
  for (std::size_t i = 0; i < methods::kImplementedCount; ++i) {
    if (method == methods::kImplemented[i]) return true;
  }
  return false;
}

std::string::size_type longestImplementedMethod() {
  std::string::size_type longest = 0;
  for (std::size_t i = 0; i < methods::kImplementedCount; ++i) {
    const std::string::size_type length =
        std::string(methods::kImplemented[i]).size();
    if (length > longest) longest = length;
  }
  return longest;
}
