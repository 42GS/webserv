#include "core/HttpDate.hpp"

#include <sstream>

namespace {

const long kSecondsPerMinute = 60;
const long kSecondsPerHour = 3600;
const long kSecondsPerDay = 86400;
const long kDaysPerWeek = 7;
const long kEpochWeekday = 4;  // 1970-01-01 was a Thursday (index in kDays)

const char* const kDays[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
const char* const kMonths[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                               "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

struct CivilDate {
  long year;
  int month;  // 1..12
  int day;    // 1..31
};

// Days since 1970-01-01 to a proleptic Gregorian date, following the
// "civil_from_days" algorithm by Howard Hinnant (eras of 400 years,
// years starting in March so that the leap day is last).
CivilDate civilFromDays(long days) {
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
  days += kEpochShift;
  const long era = (days >= 0 ? days : days - kDaysPerEra + 1) / kDaysPerEra;
  const long dayOfEra = days - era * kDaysPerEra;
  const long yearOfEra =
      (dayOfEra - dayOfEra / kDaysPer4Years + dayOfEra / kDaysPerCentury -
       dayOfEra / (kDaysPerEra - 1)) /
      kDaysPerYear;
  const long dayOfYear =  // counted from March 1
      dayOfEra - (kDaysPerYear * yearOfEra + yearOfEra / 4 - yearOfEra / 100);
  const long monthFromMarch =
      (kMonthsPerFiveMonths * dayOfYear + kMonthRounding) / kDaysPerFiveMonths;
  CivilDate date;
  date.day =
      static_cast<int>(dayOfYear -
                       (kDaysPerFiveMonths * monthFromMarch + kMonthRounding) /
                           kMonthsPerFiveMonths +
                       1);
  date.month = static_cast<int>(monthFromMarch < kMonthsBeforeJanuary
                                    ? monthFromMarch + kMarch
                                    : monthFromMarch + kMarch - kMonthsPerYear);
  date.year = yearOfEra + era * kYearsPerEra + (date.month < kMarch ? 1 : 0);
  return date;
}

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
  long weekday = (days + kEpochWeekday) % kDaysPerWeek;
  if (weekday < 0) weekday += kDaysPerWeek;

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
