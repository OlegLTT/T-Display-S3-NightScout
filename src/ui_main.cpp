// Основной экран: глюкоза, тренд, время, возраст данных и заряд загрузчика.
#include "ui_main.h"
#include "ui_layout.h"
#include "ui_ns_view.h"
#include "ui_ns_cache.h"
#include "fonts.h"

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

static const lv_point_precise_t arrow_points[] = {
    {0, 15}, {60, 15}, {48, 3}, {60, 15}, {48, 27}
};

static lv_obj_t* arrows[3] = {nullptr, nullptr, nullptr};
static int32_t   arrow_angle10_raw = 0;

static int32_t g_arrow_cx = 0;
static int32_t g_arrow_cy = 0;

static constexpr int32_t AR_PX = 30;
static constexpr int32_t AR_PY = 15;
static constexpr float   AR_GAP = 10.0f;

static int32_t norm3600(int32_t a) {
    a %= 3600;
    if (a < 0) a += 3600;
    return a;
}

static void arrow_anim_cb(void *var, int32_t angle10) {
    lv_obj_t *obj = (lv_obj_t *)var;
    lv_obj_set_style_transform_angle(obj, angle10, LV_PART_MAIN);
    arrow_angle10_raw = angle10;
}

static void arrows_apply_layout(int32_t angle_deg, int count) {
    if (count < 0) count = 0;
    if (count > 3) count = 3;

    const float rad = angle_deg * 3.1415926f / 180.0f;
    const float nx = -sinf(rad);
    const float ny =  cosf(rad);

    float offsets[3] = {0, 0, 0};
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

// Угол задаётся в десятых долях градуса; анимация выбирает путь поворота через правое направление.
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
    lv_label_set_text(label_battery, ns_battery_icon_for_pct(ns.uploaderBatPct));
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

    arrows_apply_layout(v.angleDeg, v.arrowCount);
    arrow_set_angle_deg_via_right(v.angleDeg, 300);
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
    g_arrow_cx = PX;
    g_arrow_cy = PY;

    for (int i = 0; i < 3; i++) {
        arrows[i] = lv_line_create(main_screen);
        lv_line_set_points(arrows[i], arrow_points, sizeof(arrow_points) / sizeof(arrow_points[0]));
        lv_obj_set_style_line_width(arrows[i], 8, LV_PART_MAIN);
        lv_obj_set_style_line_rounded(arrows[i], true, LV_PART_MAIN);
        lv_obj_set_style_line_color(arrows[i], lv_color_hex(0x71FFFF), LV_PART_MAIN);

        lv_obj_set_style_transform_pivot_x(arrows[i], AR_PX, LV_PART_MAIN);
        lv_obj_set_style_transform_pivot_y(arrows[i], AR_PY, LV_PART_MAIN);

        lv_obj_set_pos(arrows[i], PX - AR_PX, PY - AR_PY);
    }

    arrow_angle10_raw = -900;
    for (int i = 0; i < 3; i++) {
        lv_obj_set_style_transform_angle(arrows[i], arrow_angle10_raw, LV_PART_MAIN);
    }

    arrows_apply_layout(0, 1);

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
