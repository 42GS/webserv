/**
 * @file  ConfigValues.cpp
 * @brief Parsers of size, listen address and IPv4 arguments.
 *        크기, listen 주소, IPv4 인자의 해석.
 */
#include "config/ConfigValues.hpp"

#include <sstream>
#include <vector>

#include "http/Ascii.hpp"

namespace {

/** Largest TCP port. / TCP 포트의 최댓값. */
const unsigned long kMaxPort = 65535;
/** Unit of the "k" suffix. / 접미사 "k"의 단위. */
const std::size_t kKilobyte = 1024;
/** Unit of the "m" suffix. / 접미사 "m"의 단위. */
const std::size_t kMegabyte = 1024 * 1024;

/**
 * @brief Splits text at every separator, keeping empty parts.
 *        구분자마다 텍스트를 나눈다. 빈 조각도 남긴다.
 *
 * Empty parts are kept so that "1..2.3" fails the octet count check.
 * "1..2.3"이 옥텟 개수 검사에서 걸리도록 빈 조각을 남긴다.
 *
 * @param[in] text      Text. / 텍스트.
 * @param[in] separator Separator. / 구분자.
 * @return The parts. / 나눈 조각.
 */
std::vector<std::string> split(const std::string& text, char separator) {
  std::vector<std::string> parts;
  std::string::size_type start = 0;
  while (true) {
    const std::string::size_type end = text.find(separator, start);
    if (end == std::string::npos) {
      parts.push_back(text.substr(start));
      return parts;
    }
    parts.push_back(text.substr(start, end - start));
    start = end + 1;
  }
}

/**
 * @brief True for dotted IPv4 text "a.b.c.d" with octets 0..255.
 *        옥텟이 0..255인 "a.b.c.d" 형식의 IPv4 텍스트이면 true.
 * @param[in] text Text. / 텍스트.
 * @return Whether the text is such an address. / 그런 주소인지.
 */
bool isIpv4Address(const std::string& text) {
  const std::vector<std::string> octets = split(text, '.');
  if (octets.size() != 4) return false;  // a.b.c.d
  for (std::size_t i = 0; i < octets.size(); ++i) {
    unsigned long value;
    if (!parseDecimal(octets[i], config_values::kMaxOctet, value)) return false;
  }
  return true;
}

}  // namespace

bool parseSize(const std::string& text, std::size_t& out) {
  if (text.empty()) return false;
  std::size_t unit = 1;
  std::string digits = text;
  const char suffix = text[text.size() - 1];
  if (suffix == 'k' || suffix == 'K')
    unit = kKilobyte;
  else if (suffix == 'm' || suffix == 'M')
    unit = kMegabyte;
  if (unit != 1) digits = text.substr(0, text.size() - 1);

  // The largest count whose product with unit still fits in size_t.
  // unit을 곱해도 size_t에 들어가는 가장 큰 수.
  const unsigned long maxCount =
      static_cast<unsigned long>(static_cast<std::size_t>(-1) / unit);
  unsigned long count;
  if (!parseDecimal(digits, maxCount, count)) return false;
  out = static_cast<std::size_t>(count) * unit;
  return true;
}

bool parseListenAddr(const std::string& text, ListenAddr& out) {
  std::string host = config_values::kAnyAddress;
  std::string portText = text;
  const std::string::size_type colon = text.rfind(':');
  if (colon != std::string::npos) {
    host = text.substr(0, colon);
    portText = text.substr(colon + 1);
    if (!isIpv4Address(host)) return false;
  }

  unsigned long port;
  if (!parseDecimal(portText, kMaxPort, port) || port == 0) return false;
  out.host = host;
  out.port = static_cast<unsigned short>(port);
  return true;
}

std::string formatListenAddr(const ListenAddr& addr) {
  std::ostringstream out;
  out << addr.host << ":" << addr.port;
  return out.str();
}
