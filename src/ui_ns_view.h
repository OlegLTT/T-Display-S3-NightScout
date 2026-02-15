#pragma once
#include <lvgl.h>
#include "nightscout_client.h"

struct NsUiView {
  bool ok = false;
  bool stale = true;
  bool haveAge = false;
  int  ageMin = -1;
  int  ageCat = 0;

  char sgv[12]   = "--";
  char delta[12] = "--";
  char age[12]   = "--";
  char bat[8]    = "";       // "85%" или ""

  lv_color_t zoneCol  = lv_color_make(160,160,160);
  lv_color_t deltaCol = lv_color_make(200,200,200);
  lv_color_t batCol   = lv_color_make(160,160,160);

  char trend[24] = "None";
  int  angleDeg = 0;
  int  arrowCount = 0;       // 0..3
};

NsUiView ns_make_view(const NightscoutData& ns);

// --- shared helpers ---
lv_color_t ns_battery_color_for_pct(int pct);
const char* ns_battery_icon_for_pct(int pct);
