/**
 * @file  ConfigLimits.hpp
 * @brief Request body size limits of the configuration.
 *        설정의 요청 본문 크기 상한.
 */
#ifndef CONFIG_CONFIGLIMITS_HPP
#define CONFIG_CONFIGLIMITS_HPP

#include <cstddef>

/**
 * @brief Body size limits used by the loader and by request processing.
 *        설정 loader와 요청 처리가 함께 쓰는 본문 크기 상한.
 */
namespace config_limits {

/**
 * @brief NGINX default for client_max_body_size ("1m").
 *        client_max_body_size의 NGINX 기본값 ("1m").
 */
const std::size_t kDefaultMaxBodySize = 1024 * 1024;

/**
 * @brief Largest client_max_body_size the configuration may set.
 *        설정에서 지정할 수 있는 가장 큰 client_max_body_size.
 *
 * Request bodies are kept in memory, so the configured limit is capped.
 * Running out of memory kills the process instead of throwing (design
 * decision 11).
 * 요청 본문을 메모리에 담기 때문에 설정 값에 상한을 둔다. 메모리가
 * 부족하면 예외 대신 프로세스가 죽는다 (설계 결정 11).
 */
const std::size_t kMaxBodySizeCeiling = 100 * 1024 * 1024;

}  // namespace config_limits

#endif
