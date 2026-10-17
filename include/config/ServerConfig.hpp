/**
 * @file  ServerConfig.hpp
 * @brief The loaded configuration, as the rest of the server reads it.
 *        서버의 다른 부분이 읽는, 읽기를 마친 설정.
 */
#ifndef CONFIG_SERVERCONFIG_HPP
#define CONFIG_SERVERCONFIG_HPP

#include <cstddef>
#include <map>
#include <set>
#include <string>
#include <vector>

/**
 * @brief One interface:port pair from a "listen" directive.
 *        "listen" 지시어 하나의 인터페이스:포트 쌍.
 */
struct ListenAddr {
  /**
   * @brief Dotted IPv4 address, "0.0.0.0" for all interfaces.
   *        점으로 구분한 IPv4 주소. 모든 인터페이스는 "0.0.0.0".
   */
  std::string host;
  /** 1..65535. */
  unsigned short port;
};

/**
 * @brief Final settings of one location, with server-level values already
 *        inherited.
 *        server 단계 값을 이미 물려받은, location 하나의 최종 설정.
 */
struct LocationConfig {
  /** Prefix, starts with "/". / "/"로 시작하는 prefix. */
  std::string path;
  /** Set when "root" applies. / "root"가 적용될 때 값이 있다. */
  std::string root;
  /** Set when "alias" applies. / "alias"가 적용될 때 값이 있다. */
  std::string alias;
  /** "index" names in order. / "index" 이름, 순서대로. */
  std::vector<std::string> indexFiles;
  /** Directory listing on. / 디렉토리 목록 켜짐. */
  bool autoindex;
  /**
   * @brief Allowed methods: those of "limit_except", or GET and HEAD
   *        without it (design decision 16).
   *        허용한 메서드: "limit_except"의 메서드, 없으면 GET과 HEAD (설계
   *        결정 16).
   */
  std::set<std::string> allowedMethods;
  /** "return" code, 0 for none. / "return" 코드, 없으면 0. */
  int returnCode;
  /** "return" URL. / "return"의 URL. */
  std::string returnUrl;
  /**
   * @brief "upload_store" directory; empty means uploads are not allowed.
   *        "upload_store" 디렉토리. 비어 있으면 업로드를 허용하지 않는다.
   */
  std::string uploadStore;
  /**
   * @brief CGI interpreter by extension, such as ".php" to its path.
   *        확장자별 CGI 인터프리터. 예: ".php" → 인터프리터 경로.
   */
  std::map<std::string, std::string> cgiByExtension;
  /** client_max_body_size in bytes. / 바이트 단위 client_max_body_size. */
  std::size_t maxBodySize;
};

/**
 * @brief Settings of one "server" block.
 *        "server" 블록 하나의 설정.
 */
struct ServerConfig {
  /** Addresses to listen on. / 들을 주소. */
  std::vector<ListenAddr> listens;
  /** Status code to page URI. / 상태 코드 → 페이지 URI. */
  std::map<int, std::string> errorPages;
  /**
   * @brief Locations sorted longest path first, for prefix matching.
   *        prefix 매칭을 위해 경로가 긴 것부터 정렬한 location.
   */
  std::vector<LocationConfig> locations;
};

#endif
