// Кеш отображаемых значений: надписи и цвета обновляются только при изменении.
#include "ui_ns_cache.h"

#include <cstring>
#include <math.h>

bool ui_str_changed(const char* a, const char* b) {
  if (!a && !b) return false;
  if (!a || !b) return true;
  return std::strcmp(a, b) != 0;
}

void ui_set_label_if_changed(lv_obj_t* lbl,
                             const char* txt,
                             char* cache,
                             size_t cache_sz) {
  if (!lbl || !txt || !cache || cache_sz == 0) return;
  if (std::strncmp(cache, txt, cache_sz) == 0) return;

  std::strncpy(cache, txt, cache_sz);
  cache[cache_sz - 1] = '\0';
  lv_label_set_text(lbl, cache);
}

void ui_set_color_if_changed(lv_obj_t* obj,
                             lv_color_t c,
                             lv_color_t* cache) {
  if (!obj || !cache) return;
  if (std::memcmp(cache, &c, sizeof(lv_color_t)) == 0) return;

  *cache = c;
  lv_obj_set_style_text_color(obj, c, LV_PART_MAIN);
}

const char* ns_battery_icon_for_pct(int pct) {
  if (pct < 0)   return LV_SYMBOL_BATTERY_2;
  if (pct >= 88) return LV_SYMBOL_BATTERY_FULL;
  if (pct >= 63) return LV_SYMBOL_BATTERY_3;
  if (pct >= 38) return LV_SYMBOL_BATTERY_2;
  if (pct >= 13) return LV_SYMBOL_BATTERY_1;
                 return LV_SYMBOL_BATTERY_EMPTY;
}

lv_color_t ns_battery_color_for_pct(int pct) {
  if (pct < 0)   return lv_color_make(160, 160, 160);
  if (pct <= 15) return lv_color_make(255, 0, 0);
  if (pct <= 35) return lv_color_make(255, 120, 0);
  return lv_color_make(100, 255, 0);
}

void NsUiCache::reset() {

  sgv[0] = delta[0] = age[0] = time[0] = 0;
  date[0] = wd[0] = 0;
  iob[0] = cob[0] = 0;
  bat[0] = 0;
  trend[0] = 0;

  sgv_mmol   = NAN;
  delta_mmol = NAN;
  iob_val    = NAN;
  cob_val    = NAN;
  batPct     = -999;

  zoneCol  = lv_color_make(0, 0, 0);
  deltaCol = lv_color_make(0, 0, 0);
  batCol   = lv_color_make(0, 0, 0);

  angleDeg = 9999;
  arrowCount = 9999;

  stale = false;
  ageCat = -1;
}
