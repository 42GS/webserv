/**
 * @file  SocketAddress.cpp
 * @brief IPv4 text to socket addresses.
 *        IPv4 텍스트를 소켓 주소로 바꾼다.
 */
#include "core/SocketAddress.hpp"

#include <arpa/inet.h>

#include "config/ConfigValues.hpp"
#include "http/Ascii.hpp"

unsigned long parseIpv4(const std::string& text) {
  unsigned long address = 0;
  std::string::size_type start = 0;
  for (;;) {
    const std::string::size_type dot = text.find('.', start);
    const std::string octetText =
        text.substr(start, dot == std::string::npos ? dot : dot - start);
    unsigned long octet = 0;
    // The configuration parser has already checked the address; an octet
    // that still fails to parse counts as 0.
    // 설정 파서가 이미 주소를 검사했다. 그래도 해석되지 않는 옥텟은 0으로
    // 친다.
    static_cast<void>(parseDecimal(octetText, config_values::kMaxOctet, octet));
    address = (address << 8) | octet;
    if (dot == std::string::npos) return address;
    start = dot + 1;
  }
}

sockaddr_in toSockaddr(const ListenAddr& addr) {
  // Value-initialized: every field starts as zero.
  // 값 초기화: 모든 필드가 0에서 시작한다.
  sockaddr_in result = sockaddr_in();
  result.sin_family = AF_INET;
  result.sin_port = htons(addr.port);
  result.sin_addr.s_addr = htonl(static_cast<uint32_t>(parseIpv4(addr.host)));
  return result;
}
