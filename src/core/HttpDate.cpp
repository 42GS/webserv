#include "core/HttpDate.hpp"

#include <sstream>

namespace {

const long kSecondsPerMinute = 60;
const long kSecondsPerHour = 3600;
const long kSecondsPerDay = 86400;
const long kEpochWeekday = 4;  // 1970-01-01 was a Thursday (index in kDays)

const char* const kDays[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
const char* const kMonths[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                               "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

struct CivilDate {
  long year;
  int month;  // 1..12
  int day;    // 1..31
};

// Days since 1970-01-01 to a proleptic Gregorian date: the
// "civil_from_days" algorithm by Howard Hinnant, with its numbers kept as
// they are so that the two can be compared line by line. Years start in
// March so that the leap day is last; an era is 400 years (146097 days).
CivilDate civilFromDays(long days) {
  days += 719468;  // days from 0000-03-01 to 1970-01-01
  const long era = (days >= 0 ? days : days - 146096) / 146097;
  const long dayOfEra = days - era * 146097;
  const long yearOfEra =
      (dayOfEra - dayOfEra / 1460 + dayOfEra / 36524 - dayOfEra / 146096) / 365;
  const long dayOfYear =  // counted from March 1
      dayOfEra - (365 * yearOfEra + yearOfEra / 4 - yearOfEra / 100);
  const long monthFromMarch = (5 * dayOfYear + 2) / 153;
  CivilDate date;
  date.day = static_cast<int>(dayOfYear - (153 * monthFromMarch + 2) / 5 + 1);
  date.month = static_cast<int>(monthFromMarch < 10 ? monthFromMarch + 3
                                                    : monthFromMarch - 9);
  date.year = yearOfEra + era * 400 + (date.month <= 2 ? 1 : 0);
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
