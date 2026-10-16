/**
 * @file  StatusText.hpp
 * @brief Reason phrases of status codes.
 *        상태 코드의 이유 문구.
 */
#ifndef HTTP_STATUSTEXT_HPP
#define HTTP_STATUSTEXT_HPP

#include <string>

/**
 * @brief Reason phrase from the IANA status code registry.
 *        IANA 상태 코드 등록부의 이유 문구.
 * @param[in] status Status code. / 상태 코드.
 * @return The phrase, or "" for a code not in the table (the status line
 *         still ends with SP, RFC 9112 section 4).
 *         이유 문구. 표에 없는 코드면 "" (그래도 상태 줄은 SP로 끝난다,
 *         RFC 9112 4절).
 */
std::string reasonPhrase(int status);

#endif
