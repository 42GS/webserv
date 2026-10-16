#ifndef CORE_SOCKETADDRESS_HPP
#define CORE_SOCKETADDRESS_HPP

#include <netinet/in.h>

#include <string>

#include "config/ServerConfig.hpp"

// IPv4 address conversions using only allowed functions (htonl, ntohl, ...).

// Dotted text ("127.0.0.1") to a host-order address. The text must already
// be valid (the configuration parser checks it).
unsigned long parseIpv4(const std::string& text);

// Host-order address to dotted text.
std::string formatIpv4(unsigned long address);

sockaddr_in toSockaddr(const ListenAddr& addr);

#endif
