// Подготовка текста, цветов и стрелок из данных Nightscout для обоих экранов.
#include "app_constants.h"
#include "ui_ns_view.h"
#include "ui_ns_cache.h"
#include "glucose_zone.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>

static lv_color_t zone_color_for_sgv(float sgv, const float b[4]) {
  switch (glucose_zone(sgv, b)) {
    case GlucoseZone::LowRed:
    case GlucoseZone::HighRed:    return lv_color_make(255, 0, 0);
    case GlucoseZone::LowYellow:
    case GlucoseZone::HighYellow:
#if defined(BOARD_TDISPLAY_S3)
      return lv_color_make(255, 255, 0);
#else
      return lv_color_make(255, 160, 0);
#endif
    case GlucoseZone::InRange:   return lv_color_make(0, 255, 0);
    default:                    return lv_color_make(160, 160, 160);
  }
}

static lv_color_t delta_color(float d) {
  if (!std::isfinite(d)) return lv_color_make(200, 200, 200);
  if (d > app_constants::delta_neutral_mmol)         return lv_color_make(  0, 255,   0);
  if (d < -app_constants::delta_neutral_mmol)        return lv_color_make(255,   0,   0);
                         return lv_color_make(200, 200, 200);
}

static lv_color_t battery_color_for_pct(int pct) {
  if (pct < 0)   return lv_color_make(160, 160, 160);
  if (pct <= app_constants::battery_red_percent) return lv_color_make(255,   0,   0);
  if (pct <= app_constants::battery_orange_percent) return lv_color_make(255, 120,   0);
                 return lv_color_make(100, 255,   0);
}

static void trend_to_angle_and_count(const char* trend, int& angle_deg, int& count) {
  angle_deg = 0; count = 1;
  if (!trend || !trend[0]) return;

  if (!strcmp(trend, "Flat")) { angle_deg = 0; count = 1; return; }

  if (!strcmp(trend, "FortyFiveUp"))   { angle_deg = -45; count = 1; return; }
  if (!strcmp(trend, "SingleUp"))      { angle_deg = -90; count = 1; return; }
  if (!strcmp(trend, "DoubleUp"))      { angle_deg = -90; count = 2; return; }
  if (!strcmp(trend, "TripleUp"))      { angle_deg = -90; count = 3; return; }

  if (!strcmp(trend, "FortyFiveDown")) { angle_deg =  45; count = 1; return; }
  if (!strcmp(trend, "SingleDown"))    { angle_deg =  90; count = 1; return; }
  if (!strcmp(trend, "DoubleDown"))    { angle_deg =  90; count = 2; return; }
  if (!strcmp(trend, "TripleDown"))    { angle_deg =  90; count = 3; return; }

  if (!strcmp(trend, "None"))          { angle_deg = 0; count = 0; return; }
}

NsUiView ns_make_view(const NightscoutData& ns) {
  NsUiView v;

  v.ok = ns.ok;

  time_t now = time(nullptr);
  time_t upd = (ns.update_time_ms > 0) ? (time_t)(ns.update_time_ms / 1000ULL) : 0;

  if (v.ok && now > app_constants::valid_epoch_seconds && upd > 0 && now >= upd) {
    v.ageMin = (int)((now - upd) / 60);
    v.haveAge = true;

    if (v.ageMin <= app_constants::fresh_age_minutes) v.ageCat = 1;
    else if (v.ageMin <= app_constants::data_expiry_seconds / 60) v.ageCat = 2;
    else v.ageCat = 3;
  }

  v.stale = (v.ageCat == 3);

  if (v.ok && !v.stale && std::isfinite(ns.sgv_mmol) && ns.sgv_mmol > 0.0f) {
    std::snprintf(v.sgv, sizeof(v.sgv), "%.1f", ns.sgv_mmol);
  } else {
    std::snprintf(v.sgv, sizeof(v.sgv), "--");
  }

  if (v.ok && !v.stale && std::isfinite(ns.delta_mmol)) {
    std::snprintf(v.delta, sizeof(v.delta), "%+.1f", ns.delta_mmol);
  } else {
    std::snprintf(v.delta, sizeof(v.delta), "--");
  }

  if (v.haveAge) std::snprintf(v.age, sizeof(v.age), "%d мин", v.ageMin);
  else           std::snprintf(v.age, sizeof(v.age), "--");

  v.zoneCol = (v.ok && !v.stale) ? zone_color_for_sgv(ns.sgv_mmol, ns.sgvBounds)
                                : lv_color_make(120,120,120);

  v.deltaCol = (v.ok && !v.stale) ? delta_color(ns.delta_mmol)
                                 : lv_color_make(120,120,120);

  if (ns.batteryPct >= 0) {
    std::snprintf(v.bat, sizeof(v.bat), "%d%%", ns.batteryPct);
  } else {
    v.bat[0] = '\0';
  }
  v.batCol = battery_color_for_pct(ns.batteryPct);

  std::strncpy(v.trend, ns.trend, sizeof(v.trend));
  v.trend[sizeof(v.trend) - 1] = '\0';
  trend_to_angle_and_count(v.trend, v.angleDeg, v.arrowCount);
  if (v.stale) v.arrowCount = 0;

  return v;
}
