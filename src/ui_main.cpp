// Основной экран: глюкоза, тренд, время, возраст данных и заряд загрузчика.
#include "ui_main.h"
#include "ui_layout.h"
#include "ui_ns_view.h"
#include "ui_ns_cache.h"
#include "trend_arrows.h"
#include "fonts.h"
#include <Arduino.h>

#include <lvgl.h>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <ctime>
#include <cstddef>

static NsUiCache g_cache;

static lv_obj_t *label_time          = nullptr;
static lv_obj_t *label_bg            = nullptr;
static lv_obj_t *label_iob           = nullptr;
static lv_obj_t *label_cob           = nullptr;
static lv_obj_t *label_trend         = nullptr;
static lv_obj_t *label_delta         = nullptr;
static lv_obj_t *label_battery       = nullptr;
static lv_obj_t *label_battery_level = nullptr;
static lv_obj_t *label_age           = nullptr;

static lv_obj_t *main_screen = nullptr;

static TrendArrows trend_arrows;

// Текст и цвета обновляются через кеш; стрелки перестраиваются при изменении тренда.
void main_update_full(const NightscoutData& ns) {
  if (!label_bg) return;

  if (label_time) {
    tm tml;
    if (getLocalTime(&tml, 10)) {
      char timeStr[8];
      std::snprintf(timeStr, sizeof(timeStr), "%02d:%02d", tml.tm_hour, tml.tm_min);
      ui_set_label_if_changed(label_time, timeStr, g_cache.time, sizeof(g_cache.time));
    }
  }

  const NsUiView v = ns_make_view(ns);

  ui_set_label_if_changed(label_bg, v.sgv, g_cache.sgv, sizeof(g_cache.sgv));
  ui_layout::fit_beside_alarm(label_bg, 230, 10);
  ui_set_color_if_changed(label_bg, v.zoneCol, &g_cache.zoneCol);

  ui_set_label_if_changed(label_delta, v.delta, g_cache.delta, sizeof(g_cache.delta));
  ui_set_color_if_changed(label_delta, v.deltaCol, &g_cache.deltaCol);

  ui_set_label_if_changed(label_age, v.age, g_cache.age, sizeof(g_cache.age));

  if (label_iob) {
    char s[16] = "I: --";
    if (!v.stale && std::isfinite(ns.iob)) std::snprintf(s, sizeof(s), "I: %.2f", ns.iob);
    ui_set_label_if_changed(label_iob, s, g_cache.iob, sizeof(g_cache.iob));
  }
  if (label_cob) {
    char s[16] = "C: --";
    if (!v.stale && std::isfinite(ns.cob)) std::snprintf(s, sizeof(s), "C: %.0f", ns.cob);
    ui_set_label_if_changed(label_cob, s, g_cache.cob, sizeof(g_cache.cob));
  }

  if (label_battery_level) {
    ui_set_label_if_changed(label_battery_level, v.bat[0] ? v.bat : "--%", g_cache.bat, sizeof(g_cache.bat));
  }
  if (label_battery) {
    lv_label_set_text(label_battery, ns_battery_icon_for_pct(ns.batteryPct));
    ui_set_color_if_changed(label_battery, v.batCol, &g_cache.batCol);
  }
  if (label_battery_level) {
    lv_obj_set_style_text_color(label_battery_level, v.batCol, LV_PART_MAIN);
  }

  if (g_cache.angleDeg != v.angleDeg || g_cache.arrowCount != v.arrowCount || ui_str_changed(g_cache.trend, v.trend)) {
    std::strncpy(g_cache.trend, v.trend, sizeof(g_cache.trend));
    g_cache.trend[sizeof(g_cache.trend) - 1] = '\0';

    g_cache.angleDeg = v.angleDeg;
    g_cache.arrowCount = v.arrowCount;

    trend_arrows.set(v.angleDeg, v.arrowCount, 300);
  }
}

lv_obj_t *ui_main_create() {
    main_screen = lv_obj_create(nullptr);
    ui_layout::prepare_screen(main_screen);
    lv_obj_clear_flag(main_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(main_screen, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(main_screen, LV_OPA_COVER, LV_PART_MAIN);

    int32_t PX = ui_layout::content_width() - 40;
    int32_t PY = ui_layout::content_height() - 40;
    trend_arrows.create(main_screen, trend_arrow::main_geometry, PX, PY,
                        lv_color_hex(0x71FFFF));

    label_bg = lv_label_create(main_screen);
    lv_obj_set_style_text_color(label_bg, lv_color_hex(0xFFFF00), LV_PART_MAIN);
    lv_obj_set_style_text_font(label_bg, &roninset2_180, LV_PART_MAIN);
    lv_label_set_text(label_bg, "-.-");
    lv_obj_align(label_bg, LV_ALIGN_CENTER, -40, 10);
    ui_layout::fit_beside_alarm(label_bg, 230, 10);

    label_iob = lv_label_create(main_screen);
    lv_obj_set_style_text_color(label_iob, lv_color_make(200, 200, 200), LV_PART_MAIN);
    lv_obj_set_style_text_font(label_iob, &comfortaa_bold_18, LV_PART_MAIN);
    lv_label_set_text(label_iob, "I: --");
    lv_obj_align(label_iob, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    label_cob = lv_label_create(main_screen);
    lv_obj_set_style_text_color(label_cob, lv_color_make(200, 200, 200), LV_PART_MAIN);
    lv_obj_set_style_text_font(label_cob, &comfortaa_bold_18, LV_PART_MAIN);
    lv_label_set_text(label_cob, "C: --");
    lv_obj_align(label_cob, LV_ALIGN_BOTTOM_LEFT, 120, 0);

    lv_obj_t * arrow_anchor = lv_obj_create(main_screen);
    lv_obj_set_size(arrow_anchor, 1, 1);
    lv_obj_set_pos(arrow_anchor, PX, PY);
    lv_obj_add_flag(arrow_anchor, LV_OBJ_FLAG_HIDDEN);

    label_delta = lv_label_create(main_screen);
    lv_obj_set_style_text_color(label_delta, lv_color_make(255, 0, 0), LV_PART_MAIN);
    lv_obj_set_style_text_font(label_delta, &volkswagen_medium_36, LV_PART_MAIN);
    lv_label_set_text(label_delta, "--");
    lv_obj_align_to(label_delta, arrow_anchor, LV_ALIGN_OUT_TOP_RIGHT, 0, -40);

    label_battery = lv_label_create(main_screen);
    lv_obj_set_style_text_color(label_battery, lv_color_make(160, 160, 160), LV_PART_MAIN);
    lv_obj_set_style_text_font(label_battery, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_label_set_text(label_battery, "\uF242");
    lv_obj_align(label_battery, LV_ALIGN_TOP_LEFT, 0, 0);

    label_battery_level = lv_label_create(main_screen);
    lv_obj_set_style_text_color(label_battery_level, lv_color_make(160, 160, 160), LV_PART_MAIN);
    lv_obj_set_style_text_font(label_battery_level, &comfortaa_bold_18, LV_PART_MAIN);
    lv_label_set_text(label_battery_level, "--%");
    lv_obj_align_to(label_battery_level, label_battery, LV_ALIGN_OUT_RIGHT_MID, 2, 0);

    label_age = lv_label_create(main_screen);
    lv_obj_set_style_text_color(label_age, lv_color_make(0, 255, 0), LV_PART_MAIN);
    lv_obj_set_style_text_font(label_age, &comfortaa_bold_18, LV_PART_MAIN);
    lv_label_set_text(label_age, "--");
    lv_obj_align(label_age, LV_ALIGN_TOP_RIGHT, 0, 0);

    label_time = lv_label_create(main_screen);
    lv_obj_set_style_text_color(label_time, lv_color_make(200, 200, 200), LV_PART_MAIN);
    lv_obj_set_style_text_font(label_time, &comfortaa_bold_18, LV_PART_MAIN);
    lv_label_set_text(label_time, "--:--");
    lv_obj_align(label_time, LV_ALIGN_TOP_MID, 0, 0);

    return main_screen;
}
