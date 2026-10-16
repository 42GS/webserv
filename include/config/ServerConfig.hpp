#ifndef CONFIG_SERVERCONFIG_HPP
#define CONFIG_SERVERCONFIG_HPP

#include <cstddef>
#include <map>
#include <set>
#include <string>
#include <vector>

// One interface:port pair from a "listen" directive.
struct ListenAddr {
  std::string host;  // dotted IPv4 address, "0.0.0.0" for all interfaces
  unsigned short port;
};

// Final settings of one location, with server-level values already inherited.
struct LocationConfig {
  std::string path;   // prefix, starts with "/"
  std::string root;   // set when "root" applies
  std::string alias;  // set when "alias" applies
  std::vector<std::string> indexFiles;
  bool autoindex;
  std::set<std::string> allowedMethods;  // empty: every known method
  int returnCode;                        // 0: no redirect
  std::string returnUrl;
  std::string uploadStore;  // empty: uploads not allowed
  // ".php" -> interpreter path
  std::map<std::string, std::string> cgiByExtension;
  std::size_t maxBodySize;
};

struct ServerConfig {
  std::vector<ListenAddr> listens;
  std::map<int, std::string> errorPages;  // status code -> URI
  std::vector<LocationConfig> locations;  // longest path first
};

#endif
