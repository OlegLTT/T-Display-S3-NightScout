#include "time_utils.h"

// timezone
static constexpr int timezone = 5; // как у вас
static constexpr int timezoneOffset = timezone * SECS_PER_MIN;

bool isDST(time_t timestamp) {
  if (month(timestamp) < 3 || month(timestamp) > 11) return false;

  int secondSunInMarch = 14 - ((5 * year(timestamp) / 4 + 1) % 7);
  int firstSunInNovember = 7 - ((5 * year(timestamp) / 4 + 1) % 7);

  if (month(timestamp) == 3) {
    if (day(timestamp) > secondSunInMarch) return true;
    else if (day(timestamp) == secondSunInMarch && hour(timestamp) >= 2) return true;
  }

  if (month(timestamp) == 11) {
    if (day(timestamp) < firstSunInNovember) return true;
    else if (day(timestamp) == firstSunInNovember && hour(timestamp) < 2) return true;
  }
  return false;
}

void adjustTimezone(time_t& timestamp) {
  timestamp += timezoneOffset * SECS_PER_MIN;
  if (isDST(timestamp)) timestamp += SECS_PER_HOUR;
}
