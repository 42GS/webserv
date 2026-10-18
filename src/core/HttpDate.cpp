/**
 * @file  HttpDate.cpp
 * @brief IMF-fixdate formatting without time functions.
 *        시간 함수 없이 IMF-fixdate를 만든다.
 */
#include "core/HttpDate.hpp"

#include <sstream>

namespace {

/** Seconds in a minute. / 1분의 초. */
const long kSecondsPerMinute = 60;
/** Seconds in an hour. / 1시간의 초. */
const long kSecondsPerHour = 3600;
/** Seconds in a day. / 하루의 초. */
const long kSecondsPerDay = 86400;
/**
 * @brief 1970-01-01 was a Thursday (index in kDays).
 *        1970-01-01은 목요일 (kDays의 인덱스).
 */
const long kEpochWeekday = 4;

/**
 * @brief Day names of IMF-fixdate, Sunday first.
 *        IMF-fixdate의 요일 이름, 일요일부터.
 */
const char* const kDays[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
/** Month names of IMF-fixdate. / IMF-fixdate의 월 이름. */
const char* const kMonths[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                               "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

/**
 * @brief A proleptic Gregorian calendar date.
 *        (1582년 이전에도 적용한) 그레고리력 날짜.
 */
struct CivilDate {
  /** Year. / 연도. */
  long year;
  /** 1..12. */
  int month;
  /** 1..31. */
  int day;
};

/**
 * @brief Converts days since 1970-01-01 to a calendar date.
 *        1970-01-01 이후의 일수를 날짜로 바꾼다.
 *
 * The "civil_from_days" algorithm by Howard Hinnant, with its numbers kept
 * as they are so that the two can be compared line by line. Years start in
 * March so that the leap day is last; an era is 400 years (146097 days).
 * Howard Hinnant의 "civil_from_days" 알고리즘이다. 원래 알고리즘과 한
 * 줄씩 대조할 수 있도록 숫자를 그대로 두었다. 윤일이 마지막에 오도록
 * 한 해를 3월부터 세고, 한 era는 400년(146097일)이다.
 *
 * @param[in] days Days since 1970-01-01, may be negative.
 *                 1970-01-01 이후의 일수. 음수일 수 있다.
 * @return The date. / 날짜.
 */
CivilDate civilFromDays(long days) {
  days += 719468;  // 0000-03-01 .. 1970-01-01
  const long era = (days >= 0 ? days : days - 146096) / 146097;
  const long dayOfEra = days - era * 146097;
  const long yearOfEra =
      (dayOfEra - dayOfEra / 1460 + dayOfEra / 36524 - dayOfEra / 146096) / 365;
  const long dayOfYear =  // from March 1 / 3월 1일부터
      dayOfEra - (365 * yearOfEra + yearOfEra / 4 - yearOfEra / 100);
  const long monthFromMarch = (5 * dayOfYear + 2) / 153;

  CivilDate date;
  date.day = static_cast<int>(dayOfYear - (153 * monthFromMarch + 2) / 5 + 1);
  date.month = static_cast<int>(monthFromMarch < 10 ? monthFromMarch + 3
                                                    : monthFromMarch - 9);
  date.year = yearOfEra + era * 400 + (date.month <= 2 ? 1 : 0);
  return date;
}

/**
 * @brief Writes a value as two digits, with a leading zero below 10.
 *        값을 두 자리로 쓴다. 10보다 작으면 앞에 0을 붙인다.
 * @param[in,out] out   Stream to write to. / 쓸 스트림.
 * @param[in]     value 0..99.
 */
void appendTwoDigits(std::ostringstream& out, long value) {
  if (value < 10) out << '0';
  out << value;
}

}  // namespace

std::string formatHttpDate(std::time_t secondsSinceEpoch) {
  const long seconds = static_cast<long>(secondsSinceEpoch);
  long days = seconds / kSecondsPerDay;
  long secondOfDay = seconds % kSecondsPerDay;
  if (secondOfDay < 0) {
    secondOfDay += kSecondsPerDay;
    --days;
  }

  const CivilDate date = civilFromDays(days);
  long weekday = (days + kEpochWeekday) % 7;
  if (weekday < 0) weekday += 7;

  std::ostringstream out;
  out << kDays[weekday] << ", ";
  appendTwoDigits(out, date.day);
  out << ' ' << kMonths[date.month - 1] << ' ' << date.year << ' ';
  appendTwoDigits(out, secondOfDay / kSecondsPerHour);
  out << ':';
  appendTwoDigits(out, secondOfDay % kSecondsPerHour / kSecondsPerMinute);
  out << ':';
  appendTwoDigits(out, secondOfDay % kSecondsPerMinute);
  out << " GMT";
  return out.str();
}
