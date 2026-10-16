#ifndef CORE_SOCKETADDRESS_HPP
#define CORE_SOCKETADDRESS_HPP

#include <netinet/in.h>

#include <string>

#include "config/ServerConfig.hpp"

namespace socket_address {

const int kBitsPerOctet = 8;

}  // namespace socket_address

// IPv4 address conversions using only allowed functions (htonl, ntohl, ...).

// Dotted text ("127.0.0.1") to a host-order address. The text must already
// be valid (the configuration parser checks it).
unsigned long parseIpv4(const std::string& text);

// A listen address as sockaddr_in, port and address in network order.
sockaddr_in toSockaddr(const ListenAddr& addr);

#endif
