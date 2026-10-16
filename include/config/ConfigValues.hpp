/**
 * @file  ConfigValues.hpp
 * @brief Parsers and formatters for single directive arguments.
 *        지시어 인자 하나를 해석하거나 문자열로 만드는 함수들.
 *
 * Each parser returns false when the text is not a valid value; the
 * caller reports the error with its line number.
 * 해석 함수는 값이 올바르지 않으면 false를 돌려준다. 오류는 줄 번호를
 * 아는 호출자가 알린다.
 */
#ifndef CONFIG_CONFIGVALUES_HPP
#define CONFIG_CONFIGVALUES_HPP

#include <cstddef>
#include <string>

#include "config/ServerConfig.hpp"

/**
 * @brief Values shared by the configuration and the socket code.
 *        설정과 소켓 코드가 함께 쓰는 값.
 */
namespace config_values {

/** @brief Largest IPv4 octet. / IPv4 옥텟의 최댓값. */
const unsigned long kMaxOctet = 255;

/**
 * @brief Address of "listen 8080;": every interface.
 *        "listen 8080;"의 주소: 모든 인터페이스.
 */
const char* const kAnyAddress = "0.0.0.0";

}  // namespace config_values

/**
 * @brief Parses the NGINX size syntax: digits with an optional k/K or m/M
 *        suffix.
 *        NGINX 크기 문법을 해석한다: 숫자 뒤에 k/K나 m/M이 붙을 수 있다.
 * @param[in]  text Argument text. / 인자 텍스트.
 * @param[out] out  Size in bytes; set only on success.
 *                  바이트 단위 크기. 성공했을 때만 쓴다.
 * @return False if the text is not a size or overflows size_t.
 *         크기가 아니거나 size_t를 넘으면 false.
 */
bool parseSize(const std::string& text, std::size_t& out);

/**
 * @brief Parses "port" or "a.b.c.d:port".
 *        "port" 또는 "a.b.c.d:port"를 해석한다.
 * @param[in]  text Argument text. / 인자 텍스트.
 * @param[out] out  Address; the host is "0.0.0.0" when missing. Set only
 *                  on success.
 *                  주소. 호스트가 없으면 "0.0.0.0". 성공했을 때만 쓴다.
 * @return False for an invalid IPv4 address or a port outside 1..65535.
 *         IPv4 주소가 잘못됐거나 포트가 1..65535 밖이면 false.
 */
bool parseListenAddr(const std::string& text, ListenAddr& out);

/**
 * @brief Formats an address as "a.b.c.d:port" for error messages.
 *        오류 메시지용으로 주소를 "a.b.c.d:port" 형태로 만든다.
 * @param[in] addr The address. / 주소.
 * @return The text. / 만든 문자열.
 */
std::string formatListenAddr(const ListenAddr& addr);

#endif
