#include "config/ConfigValues.hpp"

#include <sstream>
#include <vector>

#include "http/Ascii.hpp"

namespace {

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

bool isIpv4Address(const std::string& text) {
  const std::vector<std::string> octets = split(text, '.');
  if (octets.size() != config_values::kOctetCount) return false;
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
    unit = config_values::kKilobyte;
  else if (suffix == 'm' || suffix == 'M')
    unit = config_values::kMegabyte;
  if (unit != 1) digits = text.substr(0, text.size() - 1);
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
  if (!parseDecimal(portText, config_values::kMaxPort, port) || port == 0)
    return false;
  out.host = host;
  out.port = static_cast<unsigned short>(port);
  return true;
}

std::string formatListenAddr(const ListenAddr& addr) {
  std::ostringstream out;
  out << addr.host << ":" << addr.port;
  return out.str();
}
