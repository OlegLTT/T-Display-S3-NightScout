// Кеш отображаемых значений: надписи и цвета обновляются только при изменении.
#pragma once
#include <lvgl.h>
#include <cstddef>

bool ui_str_changed(const char* a, const char* b);

void ui_set_label_if_changed(lv_obj_t* lbl,
                             const char* txt,
                             char* cache,
                             size_t cache_sz);

void ui_set_color_if_changed(lv_obj_t* obj,
                             lv_color_t c,
                             lv_color_t* cache);

const char* ns_battery_icon_for_pct(int pct);
lv_color_t  ns_battery_color_for_pct(int pct);

struct NsUiCache {

  char sgv[16]   = "";
  char delta[16] = "";
  char age[16]   = "";
  char time[16]  = "";
  char date[45]  = "";
  char wd[25]    = "";

  char iob[16]   = "";
  char cob[16]   = "";

  char bat[16]   = "";
  char trend[24] = "";

  float sgv_mmol;
  float delta_mmol;
  float iob_val;
  float cob_val;
  int   batPct;

  lv_color_t zoneCol;
  lv_color_t deltaCol;
  lv_color_t batCol;

  int angleDeg;
  int arrowCount;

  bool stale;
  int  ageCat;

  NsUiCache() { reset(); }
  void reset();
};
