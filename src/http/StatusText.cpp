#include "http/StatusText.hpp"

std::string reasonPhrase(int status) {
  for (std::size_t i = 0; i < status_text::kStatusNameCount; ++i) {
    if (status_text::kStatusNames[i].status == status)
      return status_text::kStatusNames[i].reason;
  }
  return "";
}
