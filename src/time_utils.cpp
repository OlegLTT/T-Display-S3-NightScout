// Настройка часового пояса, летнего времени и серверов синхронизации часов.
#include "time_utils.h"
#include <time.h>

// Знак смещения в POSIX TZ противоположен привычному смещению относительно UTC.
static void makeTzPosix(char* out, size_t outLen, int16_t tzMinutes) {

  int total = tzMinutes;
  int sign = (total >= 0) ? -1 : +1;
  int a = abs(total);
  int hh = a / 60;
  int mm = a % 60;

  if (mm == 0) snprintf(out, outLen, "UTC%+d", sign * hh);
  else         snprintf(out, outLen, "UTC%+d:%02d", sign * hh, mm);
}

void applyTimeConfig(int16_t tzMinutes, bool dstEnable,
                     const char* ntp1, const char* ntp2, const char* ntp3) {
  const long gmtOffsetSec = (long)tzMinutes * 60L;
  const int  dstOffsetSec = dstEnable ? 3600 : 0;
  configTime(gmtOffsetSec, dstOffsetSec, ntp1, ntp2, ntp3);

  char tz[32];
  makeTzPosix(tz, sizeof(tz), tzMinutes);
  setenv("TZ", tz, 1);
  tzset();
}
