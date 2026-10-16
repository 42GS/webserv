#include "core/HttpDate.hpp"

#include <sstream>

namespace http_date {
namespace {

// Days since 1970-01-01 to a proleptic Gregorian date, following the
// "civil_from_days" algorithm by Howard Hinnant (eras of 400 years,
// years starting in March so that the leap day is last).
CivilDate civilFromDays(long days) {
  days += kEpochShift;
  const long era = (days >= 0 ? days : days - kDaysPerEra + 1) / kDaysPerEra;
  const long dayOfEra = days - era * kDaysPerEra;
  const long yearOfEra =
      (dayOfEra - dayOfEra / kDaysPer4Years + dayOfEra / kDaysPerCentury -
       dayOfEra / (kDaysPerEra - 1)) /
      kDaysPerYear;
  const long dayOfYear =  // counted from March 1
      dayOfEra - (kDaysPerYear * yearOfEra + yearOfEra / kYearsPerLeapCycle -
                  yearOfEra / kYearsPerCentury);
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
  if (value < kSmallestTwoDigitValue) out << '0';
  out << value;
}

}  // namespace
}  // namespace http_date

std::string formatHttpDate(std::time_t secondsSinceEpoch) {
  using namespace http_date;
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
