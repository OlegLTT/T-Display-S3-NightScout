// Экран часов: время, дата, глюкоза и тренд; цвет учитывает возраст данных.
#include "app_constants.h"
#include "ui_clock.h"
#include "ui_layout.h"
#include "fonts.h"
#include <Arduino.h>

#include <lvgl.h>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <cmath>

#include "ui_ns_cache.h"
#include "ui_ns_view.h"
#include "trend_arrows.h"

static const char* const MONTHS_RU[12] = {
  "января","февраля","марта","апреля","мая","июня",
  "июля","августа","сентября","октября","ноября","декабря"
};

static const char* const WEEKDAYS_RU[7] = {
  "воскресенье",
  "понедельник",
  "вторник",
  "среда",
  "четверг",
  "пятница",
  "суббота"
};

static lv_obj_t* g_scr_clock = nullptr;

static lv_obj_t* lbl_time  = nullptr;
static lv_obj_t* lbl_date  = nullptr;
static lv_obj_t* lbl_wd    = nullptr;

static lv_obj_t* lbl_sgv   = nullptr;
static lv_obj_t* lbl_delta = nullptr;
static lv_obj_t* lbl_age   = nullptr;

static lv_obj_t* top_mid   = nullptr;
static lv_obj_t* arrow_box = nullptr;

static TrendArrows trend_arrows;

static constexpr int32_t AR_BOX_W = trend_arrow::clock_geometry.width;
static constexpr int32_t AR_BOX_H = trend_arrow::clock_geometry.height;

static NsUiCache g_cache;

static inline void color_to_rgb24(lv_color_t c, uint8_t& r, uint8_t& g, uint8_t& b) {

  uint32_t u = lv_color_to_u32(c);
  r = (u >> 16) & 0xFF;
  g = (u >>  8) & 0xFF;
  b = (u >>  0) & 0xFF;
}

static lv_color_t mix_rgb(lv_color_t a, lv_color_t b, uint8_t t  ) {

  uint8_t ar, ag, ab;
  uint8_t br, bg, bb;
  color_to_rgb24(a, ar, ag, ab);
  color_to_rgb24(b, br, bg, bb);

  uint8_t rr  = (uint16_t(ar) * (255 - t) + uint16_t(br) * t) / 255;
  uint8_t gg  = (uint16_t(ag) * (255 - t) + uint16_t(bg) * t) / 255;
  uint8_t bb2 = (uint16_t(ab) * (255 - t) + uint16_t(bb) * t) / 255;

  return lv_color_make(rr, gg, bb2);
}

// Свежие данные сохраняют цвет зоны; старые или неизвестные по возрасту смешиваются с серым.
static lv_color_t color_by_age(lv_color_t base, bool haveAge, int ageMin, bool stale) {
  const lv_color_t grey = lv_color_make(120,120,120);

  if (stale || !haveAge || ageMin < 0)
    return mix_rgb(base, grey, 200);

  if (ageMin <= app_constants::fresh_age_minutes)  return base;
  if (ageMin <= app_constants::data_expiry_seconds / 60) return mix_rgb(base, grey, 120);
  return mix_rgb(base, grey, 200);
}


lv_obj_t* ui_clock_screen() {
  return g_scr_clock;
}

lv_obj_t* ui_clock_create() {
  g_scr_clock = lv_obj_create(nullptr);
  ui_layout::prepare_screen(g_scr_clock);
  lv_obj_clear_flag(g_scr_clock, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(g_scr_clock, lv_color_black(), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(g_scr_clock, LV_OPA_COVER, LV_PART_MAIN);

  lbl_sgv = lv_label_create(g_scr_clock);
  lv_obj_set_style_text_font(lbl_sgv, &comfortaa_bold_18, LV_PART_MAIN);
  lv_obj_set_style_text_color(lbl_sgv, lv_color_make(200, 200, 200), LV_PART_MAIN);
  lv_label_set_text(lbl_sgv, "--");
  lv_obj_align(lbl_sgv, LV_ALIGN_TOP_LEFT, 6, 3);

  lbl_age = lv_label_create(g_scr_clock);
  lv_obj_set_style_text_font(lbl_age, &comfortaa_bold_18, LV_PART_MAIN);
  lv_obj_set_style_text_color(lbl_age, lv_color_make(200, 200, 200), LV_PART_MAIN);
  lv_label_set_text(lbl_age, "--");
  lv_obj_align(lbl_age, LV_ALIGN_TOP_RIGHT, -6, 3);

  top_mid = lv_obj_create(g_scr_clock);
    lv_obj_clear_flag(top_mid, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(top_mid, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(top_mid, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(top_mid, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_gap(top_mid, 6, LV_PART_MAIN);

    lv_obj_set_size(top_mid, LV_SIZE_CONTENT, LV_SIZE_CONTENT);

    lv_obj_set_layout(top_mid, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(top_mid, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(top_mid,
                        LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);

    lv_obj_align(top_mid, LV_ALIGN_TOP_MID, 0, 3);

  lbl_delta = lv_label_create(top_mid);
  lv_obj_set_style_text_font(lbl_delta, &comfortaa_bold_18, LV_PART_MAIN);
  lv_obj_set_style_text_color(lbl_delta, lv_color_make(200, 200, 200), LV_PART_MAIN);
  lv_label_set_text(lbl_delta, "--");

  arrow_box = lv_obj_create(top_mid);
  lv_obj_clear_flag(arrow_box, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(arrow_box, AR_BOX_W, AR_BOX_H);
  lv_obj_set_style_bg_opa(arrow_box, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(arrow_box, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(arrow_box, 0, LV_PART_MAIN);

  trend_arrows.create(arrow_box, trend_arrow::clock_geometry, AR_BOX_W / 2, AR_BOX_H / 2,
                      lv_color_make(200, 200, 200));

  lbl_time = lv_label_create(g_scr_clock);
  lv_obj_set_style_text_font(lbl_time, &directive_four_condensed_bold_100, LV_PART_MAIN);
  lv_obj_set_style_text_color(lbl_time, lv_color_make(200, 200, 200), LV_PART_MAIN);
  lv_label_set_text(lbl_time, "--:--");
  lv_obj_align(lbl_time, LV_ALIGN_CENTER, 0, 0);
  ui_layout::fit_beside_alarm(lbl_time, ui_layout::content_width() - 4, 0);

  lbl_date = lv_label_create(g_scr_clock);
  lv_obj_set_style_text_font(lbl_date, &comfortaa_bold_18, LV_PART_MAIN);
  lv_obj_set_style_text_color(lbl_date, lv_color_make(200, 200, 200), LV_PART_MAIN);
  lv_label_set_text(lbl_date, "");
  lv_obj_align(lbl_date, LV_ALIGN_BOTTOM_LEFT, 5, -2);

  lbl_wd = lv_label_create(g_scr_clock);
  lv_obj_set_style_text_font(lbl_wd, &comfortaa_bold_18, LV_PART_MAIN);
  lv_obj_set_style_text_color(lbl_wd, lv_color_make(200, 200, 200), LV_PART_MAIN);

  lv_label_set_text(lbl_wd, "");
  lv_obj_align(lbl_wd, LV_ALIGN_BOTTOM_RIGHT, 0, -2);

  g_cache.reset();
  return g_scr_clock;
}

void clock_update_full(const NightscoutData& ns) {
  if (!g_scr_clock) return;

  tm tml;
  if (getLocalTime(&tml, 10)) {
    char timeStr[8];
    std::snprintf(timeStr, sizeof(timeStr), "%02d:%02d", tml.tm_hour, tml.tm_min);
    ui_set_label_if_changed(lbl_time, timeStr, g_cache.time, sizeof(g_cache.time));
    ui_layout::fit_beside_alarm(lbl_time, ui_layout::content_width() - 4, 0);

    char dateStr[32];
    std::snprintf(dateStr, sizeof(dateStr), "%d %s %d",
                  tml.tm_mday, MONTHS_RU[tml.tm_mon], tml.tm_year + 1900);
    ui_set_label_if_changed(lbl_date, dateStr, g_cache.date, sizeof(g_cache.date));

    const char* wd = WEEKDAYS_RU[tml.tm_wday];
    ui_set_label_if_changed(lbl_wd, wd, g_cache.wd, sizeof(g_cache.wd));
  }

  const NsUiView v = ns_make_view(ns);

  const lv_color_t zoneColUsed = color_by_age(v.zoneCol, v.haveAge, v.ageMin, v.stale);

  ui_set_label_if_changed(lbl_sgv, v.sgv, g_cache.sgv, sizeof(g_cache.sgv));
  lv_obj_set_style_text_color(lbl_sgv, zoneColUsed, LV_PART_MAIN);

  lv_obj_set_style_text_color(lbl_time, zoneColUsed, LV_PART_MAIN);

  ui_set_label_if_changed(lbl_delta, v.delta, g_cache.delta, sizeof(g_cache.delta));
  lv_obj_set_style_text_color(lbl_delta, zoneColUsed, LV_PART_MAIN);

  {
    char ageText[16] = "--";
    if (v.haveAge && v.ageMin >= 0)
      std::snprintf(ageText, sizeof(ageText), "%d мин", v.ageMin);
    ui_set_label_if_changed(lbl_age, ageText, g_cache.age, sizeof(g_cache.age));
    lv_obj_set_style_text_color(lbl_age, zoneColUsed, LV_PART_MAIN);
  }

  trend_arrows.set_color(zoneColUsed);
  trend_arrows.set(v.angleDeg, v.arrowCount, 250);
}
