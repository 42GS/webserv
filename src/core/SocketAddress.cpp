#include "core/SocketAddress.hpp"

#include <arpa/inet.h>

#include <cstring>
#include <sstream>

namespace {

const int kOctetCount = 4;
const int kBitsPerOctet = 8;
const unsigned long kOctetMask = 0xff;

}  // namespace

unsigned long parseIpv4(const std::string& text) {
  unsigned long address = 0;
  unsigned long octet = 0;
  for (std::string::size_type i = 0; i < text.size(); ++i) {
    if (text[i] == '.') {
      address = (address << kBitsPerOctet) | octet;
      octet = 0;
    } else {
      octet = octet * 10 + static_cast<unsigned long>(text[i] - '0');
    }
  }
  return (address << kBitsPerOctet) | octet;
}

std::string formatIpv4(unsigned long address) {
  std::ostringstream out;
  for (int i = kOctetCount - 1; i >= 0; --i) {
    out << ((address >> (i * kBitsPerOctet)) & kOctetMask);
    if (i > 0) out << '.';
  }
  return out.str();
}

sockaddr_in toSockaddr(const ListenAddr& addr) {
  sockaddr_in result;
  std::memset(&result, 0, sizeof(result));
  result.sin_family = AF_INET;
  result.sin_port = htons(addr.port);
  result.sin_addr.s_addr = htonl(static_cast<uint32_t>(parseIpv4(addr.host)));
  return result;
}
