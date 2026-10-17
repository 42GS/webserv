/**
 * @file  SocketAddress.hpp
 * @brief IPv4 address conversions using only allowed functions (htonl,
 *        htons).
 *        허용 함수(htonl, htons)만 쓰는 IPv4 주소 변환.
 */
#ifndef CORE_SOCKETADDRESS_HPP
#define CORE_SOCKETADDRESS_HPP

#include <netinet/in.h>

#include <string>

#include "config/ServerConfig.hpp"

/**
 * @brief Converts dotted text ("127.0.0.1") to a host-order address.
 *        점 표기 텍스트("127.0.0.1")를 호스트 바이트 순서 주소로 바꾼다.
 * @param[in] text Address already checked by the configuration parser.
 *                 설정 파서가 이미 검사한 주소.
 * @return The address; an octet that still fails to parse counts as 0.
 *         주소. 그래도 해석되지 않는 옥텟은 0으로 친다.
 */
unsigned long parseIpv4(const std::string& text);

/**
 * @brief A listen address as sockaddr_in.
 *        listen 주소를 sockaddr_in으로 바꾼다.
 * @param[in] addr The address. / 주소.
 * @return Port and address in network order. / 네트워크 바이트 순서의 포트와
 * 주소.
 */
sockaddr_in toSockaddr(const ListenAddr& addr);

/**
 * @brief Dotted text of an IPv4 address.
 *        IPv4 주소의 점 표기 텍스트.
 *
 * inet_ntoa() and inet_ntop() are not on the list of allowed functions,
 * so the text is built from ntohl() (wiki analysis/subject-vs-evaluation
 * 1.7).
 * inet_ntoa()와 inet_ntop()는 허용 함수 목록에 없어서 ntohl()로 직접
 * 만든다 (위키 analysis/subject-vs-evaluation 1.7절).
 *
 * @param[in] address Address in network order. / 네트워크 바이트 순서 주소.
 * @return Text such as "127.0.0.1". / "127.0.0.1" 같은 텍스트.
 */
std::string formatIpv4(in_addr_t address);

#endif
