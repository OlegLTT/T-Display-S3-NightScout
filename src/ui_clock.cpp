// Экран часов: время, дата, глюкоза и тренд; цвет учитывает возраст данных.
#include "ui_clock.h"
#include "ui_layout.h"
#include "fonts.h"

#include <lvgl.h>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <cmath>

#include "ui_ns_cache.h"
#include "ui_ns_view.h"

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

static lv_obj_t* arrows[3] = {nullptr, nullptr, nullptr};
static int32_t   arrow_angle10_raw = 0;

static int32_t g_arrow_cx = 0;
static int32_t g_arrow_cy = 0;

static constexpr int32_t AR_PX = 9;
static constexpr int32_t AR_PY = 8;
static constexpr float   AR_GAP = 10.0f;

static constexpr int32_t AR_BOX_W = 56;
static constexpr int32_t AR_BOX_H = 26;

static const lv_point_precise_t arrow_points[] = {
  {0, 8}, {18, 8}, {9, 0}, {18, 8}, {9, 16}
};

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

  if (ageMin <= 5)  return base;
  if (ageMin <= 15) return mix_rgb(base, grey, 120);
  return mix_rgb(base, grey, 200);
}

static int32_t norm3600(int32_t a) {
  a %= 3600;
  if (a < 0) a += 3600;
  return a;
}

static void arrow_anim_cb(void *var, int32_t angle10) {
  lv_obj_set_style_transform_angle((lv_obj_t*)var, angle10, LV_PART_MAIN);
  arrow_angle10_raw = angle10;
}

// Привязываем стрелки к фактическому положению надписи изменения глюкозы.
static void arrows_update_base_from_delta() {

  if (!arrow_box) return;
  g_arrow_cx = AR_BOX_W / 2;
  g_arrow_cy = AR_BOX_H / 2;
}

static void arrows_apply_layout(int32_t angle_deg, int count) {
  if (count < 0) count = 0;
  if (count > 3) count = 3;

  const float rad = angle_deg * 3.1415926f / 180.0f;
  const float nx = -sinf(rad);
  const float ny =  cosf(rad);

  float offsets[3] = {0,0,0};
  if (count == 2) { offsets[0] = -0.5f; offsets[1] = +0.5f; }
  if (count == 3) { offsets[0] = -1.0f; offsets[1] =  0.0f; offsets[2] = +1.0f; }

  for (int i = 0; i < 3; i++) {
    if (!arrows[i]) continue;

    if (i < count) {
      lv_obj_clear_flag(arrows[i], LV_OBJ_FLAG_HIDDEN);

      float k = offsets[i] * AR_GAP;
      int32_t cx = (int32_t)lroundf((float)g_arrow_cx + nx * k);
      int32_t cy = (int32_t)lroundf((float)g_arrow_cy + ny * k);

      lv_obj_set_pos(arrows[i], cx - AR_PX, cy - AR_PY);
    } else {
      lv_obj_add_flag(arrows[i], LV_OBJ_FLAG_HIDDEN);
    }
  }
}

static void arrow_set_angle_deg_via_right(int32_t target_deg, uint32_t time_ms) {
  if (!arrows[0]) return;

  int32_t start_raw  = arrow_angle10_raw;
  int32_t start_norm = norm3600(start_raw);
  int32_t target10   = norm3600(target_deg * 10);

  int32_t diff;
  if (target10 >= start_norm) diff = (target10 - start_norm) - 3600;
  else                        diff = (target10 - start_norm) + 3600;

  if (diff == 0) return;

  int32_t end_raw = start_raw + diff;

  for (int i = 0; i < 3; i++) {
    if (!arrows[i]) continue;

    lv_anim_del(arrows[i], arrow_anim_cb);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, arrows[i]);
    lv_anim_set_exec_cb(&a, arrow_anim_cb);
    lv_anim_set_time(&a, time_ms);
    lv_anim_set_values(&a, start_raw, end_raw);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);
  }
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

  for (int i = 0; i < 3; i++) {
    arrows[i] = lv_line_create(arrow_box);
    lv_line_set_points(arrows[i], arrow_points,
                       sizeof(arrow_points) / sizeof(arrow_points[0]));
    lv_obj_set_style_line_width(arrows[i], 3, LV_PART_MAIN);
    lv_obj_set_style_line_rounded(arrows[i], true, LV_PART_MAIN);
    lv_obj_set_style_line_color(arrows[i], lv_color_make(200, 200, 200), LV_PART_MAIN);

    lv_obj_set_style_transform_pivot_x(arrows[i], AR_PX, LV_PART_MAIN);
    lv_obj_set_style_transform_pivot_y(arrows[i], AR_PY, LV_PART_MAIN);
  }

  arrows_update_base_from_delta();
  arrow_angle10_raw = 0;
  for (int i = 0; i < 3; i++) {
    if (!arrows[i]) continue;
    lv_obj_set_style_transform_angle(arrows[i], arrow_angle10_raw, LV_PART_MAIN);
  }
  arrows_apply_layout(0, 0);

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

  arrows_update_base_from_delta();

  for (int i = 0; i < 3; i++) {
    if (!arrows[i]) continue;
    lv_obj_set_style_line_color(arrows[i], zoneColUsed, LV_PART_MAIN);
  }

  if (v.arrowCount <= 0) {
    arrows_apply_layout(0, 0);
  } else {
    const bool trendChanged = ui_str_changed(g_cache.trend, v.trend);

    if (trendChanged || g_cache.angleDeg != v.angleDeg || g_cache.arrowCount != v.arrowCount) {
      std::strncpy(g_cache.trend, v.trend, sizeof(g_cache.trend));
      g_cache.trend[sizeof(g_cache.trend) - 1] = '\0';

      g_cache.angleDeg = v.angleDeg;
      g_cache.arrowCount = v.arrowCount;

      arrows_apply_layout(v.angleDeg, v.arrowCount);
      arrow_set_angle_deg_via_right(v.angleDeg, 250);
    } else {
      arrows_apply_layout(v.angleDeg, v.arrowCount);
    }
  }
}
