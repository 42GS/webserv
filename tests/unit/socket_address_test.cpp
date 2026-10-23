/**
 * @file  socket_address_test.cpp
 * @brief Unit tests of the IPv4 address helpers.
 *        IPv4 주소 도우미의 단위 테스트.
 */
#include "core/SocketAddress.hpp"
#include "unit/TestFramework.hpp"

TEST_CASE(parsesIpv4) {
  CHECK_EQ(0x7f000001ul, parseIpv4("127.0.0.1"));
  CHECK_EQ(0ul, parseIpv4("0.0.0.0"));
  CHECK_EQ(0xfffffffful, parseIpv4("255.255.255.255"));
  CHECK_EQ(0x0a141e28ul, parseIpv4("10.20.30.40"));
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
