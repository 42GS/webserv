/**
 * @file  Methods.cpp
 * @brief The list of implemented methods.
 *        구현한 메서드 목록.
 */
#include "http/Methods.hpp"

#include <cstddef>

namespace {

/** Methods this server implements. / 이 서버가 구현한 메서드. */
const char* const kImplementedMethods[] = {methods::kGet, methods::kHead,
                                           methods::kPost, methods::kDelete};

/**
 * @brief Number of entries in kImplementedMethods.
 *        kImplementedMethods의 항목 수.
 */
const std::size_t kMethodCount =
    sizeof(kImplementedMethods) / sizeof(kImplementedMethods[0]);

}  // namespace

bool isImplementedMethod(const std::string& method) {
  for (std::size_t i = 0; i < kMethodCount; ++i) {
    if (method == kImplementedMethods[i]) return true;
  }
  return false;
}

std::string::size_type longestImplementedMethod() {
  std::string::size_type longest = 0;
  for (std::size_t i = 0; i < kMethodCount; ++i) {
    const std::string::size_type length =
        std::string(kImplementedMethods[i]).size();
    if (length > longest) longest = length;
  }
  return longest;
}
