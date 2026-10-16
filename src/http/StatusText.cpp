#include "http/StatusText.hpp"

#include <cstddef>

namespace {

struct StatusName {
  int status;
  const char* reason;
};

// Names as in raw/iana-http-status-codes.txt of the wiki.
const StatusName kStatusNames[] = {
    {200, "OK"},
    {201, "Created"},
    {204, "No Content"},
    {301, "Moved Permanently"},
    {302, "Found"},
    {303, "See Other"},
    {307, "Temporary Redirect"},
    {308, "Permanent Redirect"},
    {400, "Bad Request"},
    {403, "Forbidden"},
    {404, "Not Found"},
    {405, "Method Not Allowed"},
    {408, "Request Timeout"},
    {413, "Content Too Large"},
    {414, "URI Too Long"},
    {415, "Unsupported Media Type"},
    {431, "Request Header Fields Too Large"},
    {500, "Internal Server Error"},
    {501, "Not Implemented"},
    {502, "Bad Gateway"},
    {504, "Gateway Timeout"},
    {505, "HTTP Version Not Supported"},
};

}  // namespace

std::string reasonPhrase(int status) {
  for (std::size_t i = 0; i < sizeof(kStatusNames) / sizeof(kStatusNames[0]);
       ++i) {
    if (kStatusNames[i].status == status) return kStatusNames[i].reason;
  }
  return "";
}
