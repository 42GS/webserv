#include "core/SocketAddress.hpp"
#include "unit/TestFramework.hpp"

TEST_CASE(convertsIpv4BothWays) {
  CHECK_EQ(0x7f000001ul, parseIpv4("127.0.0.1"));
  CHECK_EQ(0ul, parseIpv4("0.0.0.0"));
  CHECK_EQ(0xfffffffful, parseIpv4("255.255.255.255"));
  CHECK_EQ(std::string("127.0.0.1"), formatIpv4(0x7f000001ul));
  CHECK_EQ(std::string("10.20.30.40"), formatIpv4(parseIpv4("10.20.30.40")));
}

TEST_CASE(buildsSockaddrInNetworkOrder) {
  ListenAddr addr;
  addr.host = "127.0.0.1";
  addr.port = 8080;
  const sockaddr_in result = toSockaddr(addr);
  CHECK_EQ(AF_INET, static_cast<int>(result.sin_family));
  CHECK_EQ(8080, static_cast<int>(ntohs(result.sin_port)));
  CHECK_EQ(0x7f000001ul,
           static_cast<unsigned long>(ntohl(result.sin_addr.s_addr)));
}
