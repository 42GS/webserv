/**
 * @file  HttpDate.hpp
 * @brief The HTTP Date header value.
 *        HTTP Date 헤더 값.
 */
#ifndef CORE_HTTPDATE_HPP
#define CORE_HTTPDATE_HPP

#include <ctime>
#include <string>

/**
 * @brief Formats seconds since the epoch (UTC) as an IMF-fixdate.
 *        epoch(UTC) 이후의 초를 IMF-fixdate로 만든다.
 *
 * For example "Sun, 06 Nov 1994 08:49:37 GMT" (RFC 9110 section 5.6.7).
 * Computed by hand because the allowed function list has no time
 * functions (design decision 10).
 * 예: "Sun, 06 Nov 1994 08:49:37 GMT" (RFC 9110 5.6.7절). 허용 함수
 * 목록에 시간 함수가 없어 직접 계산한다 (설계 결정 10).
 *
 * @param[in] secondsSinceEpoch Time from Clock::now(). / Clock::now()의 시각.
 * @return The date text. / 날짜 문자열.
 */
std::string formatHttpDate(std::time_t secondsSinceEpoch);

#endif
