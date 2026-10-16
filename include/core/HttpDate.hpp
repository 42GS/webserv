#ifndef CORE_HTTPDATE_HPP
#define CORE_HTTPDATE_HPP

#include <ctime>
#include <string>

namespace http_date {

const long kSecondsPerMinute = 60;
const long kSecondsPerHour = 3600;
const long kSecondsPerDay = 86400;
const long kDaysPerWeek = 7;
const long kEpochWeekday = 4;  // 1970-01-01 was a Thursday (index in kDays)

const char* const kDays[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
const char* const kMonths[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                               "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

// Constants of the civil_from_days algorithm (see HttpDate.cpp).
const long kEpochShift = 719468;  // days from 0000-03-01 to 1970-01-01
const long kYearsPerEra = 400;
const long kDaysPerYear = 365;
const long kDaysPer4Years = 4 * kDaysPerYear;          // 1460
const long kDaysPerCentury = 100 * kDaysPerYear + 24;  // 36524
const long kDaysPerEra = 400 * kDaysPerYear + 97;      // 146097
const long kDaysPerFiveMonths = 153;  // Mar..Jul, and again Aug..Dec
const long kMonthsPerFiveMonths = 5;
const long kMonthRounding = 2;  // makes the 153/5 day split exact
const long kMarch = 3;
const long kMonthsBeforeJanuary = 10;  // Mar..Dec in a March-based year
const long kMonthsPerYear = 12;
const long kYearsPerLeapCycle = 4;  // a leap year every 4 years
const long kYearsPerCentury = 100;  // except at most centuries

const long kSmallestTwoDigitValue = 10;  // below it, a leading "0"

struct CivilDate {
  long year;
  int month;  // 1..12
  int day;    // 1..31
};

}  // namespace http_date

// Seconds since the epoch (UTC) as an IMF-fixdate, for example
// "Sun, 06 Nov 1994 08:49:37 GMT" (RFC 9110 section 5.6.7). Computed by
// hand because the allowed function list has no time functions (design
// decision 10).
std::string formatHttpDate(std::time_t secondsSinceEpoch);

#endif
