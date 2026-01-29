// ui_main.cpp
#include "ui_main.h"
#include "fonts.h"

#include <lvgl.h>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <ctime>
#include <cstddef>

// ======================================================
//                     LVGL handles
// ======================================================
static lv_obj_t *label_time          = nullptr;
static lv_obj_t *label_bg            = nullptr;
static lv_obj_t *label_iob           = nullptr;
static lv_obj_t *label_cob           = nullptr;
static lv_obj_t *label_trend         = nullptr;   // резерв
static lv_obj_t *label_delta         = nullptr;
static lv_obj_t *label_battery       = nullptr;
static lv_obj_t *label_battery_level = nullptr;
static lv_obj_t *label_age           = nullptr;

static lv_obj_t *main_screen = nullptr;

// ======================================================
//                     Arrow(s) (trend)
// ======================================================
static const lv_point_precise_t arrow_points[] = {
    {0, 15}, {60, 15}, {48, 3}, {60, 15}, {48, 27}
};

static lv_obj_t* arrows[3] = {nullptr, nullptr, nullptr};
static int32_t   arrow_angle10_raw = 0;

// базовый центр вращения (в координатах экрана)
static int32_t g_arrow_cx = 0;
static int32_t g_arrow_cy = 0;

// bbox линии (60x30), pivot (30,15)
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

// ======================================================
//                     Helpers
// ======================================================
static inline bool str_changed(const char* a, const char* b) {
    if (!a && !b) return false;
    if (!a || !b) return true;
    return std::strcmp(a, b) != 0;
}

static void set_label_if_changed(lv_obj_t* lbl, const char* txt,
                                 char* cache, size_t cache_sz) {
    if (!lbl || !txt || !cache || cache_sz == 0) return;
    if (std::strncmp(cache, txt, cache_sz) == 0) return;
    std::strncpy(cache, txt, cache_sz);
    cache[cache_sz - 1] = '\0';
    lv_label_set_text(lbl, cache);
}

static void set_color_if_changed(lv_obj_t* obj, lv_color_t c, lv_color_t* cache) {
    if (!obj || !cache) return;
    if (std::memcmp(cache, &c, sizeof(lv_color_t)) == 0) return;
    *cache = c;
    lv_obj_set_style_text_color(obj, c, LV_PART_MAIN);
}

static lv_color_t battery_color_for_pct(int pct) {
    if (pct < 0)   return lv_color_make(160,160,160);
    if (pct <= 15) return lv_color_make(255, 0, 0);
    if (pct <= 35) return lv_color_make(255, 120, 0);
    return lv_color_make(100, 255, 0);
}

// \uF240-100% \uF241-75% \uF242-50% \uF243-25% \uF244-0%
static const char* battery_icon_for_pct(int pct) {
    if (pct < 0)   return "\uF242"; // нейтральная
    if (pct >= 88) return "\uF240";
    if (pct >= 63) return "\uF241";
    if (pct >= 38) return "\uF242";
    if (pct >= 13) return "\uF243";
    return "\uF244";
}

// тренд -> угол + количество стрелок
static void trend_to_angle_and_count(const char* trend, int& angle_deg, int& count) {
    angle_deg = 0;
    count = 1;
    if (!trend || !trend[0]) return;

    if (!std::strcmp(trend, "Flat")) { angle_deg = 0; count = 1; return; }

    if (!std::strcmp(trend, "FortyFiveUp"))   { angle_deg = -45; count = 1; return; }
    if (!std::strcmp(trend, "SingleUp"))      { angle_deg = -90; count = 1; return; }
    if (!std::strcmp(trend, "DoubleUp"))      { angle_deg = -90; count = 2; return; }
    if (!std::strcmp(trend, "TripleUp"))      { angle_deg = -90; count = 3; return; }

    if (!std::strcmp(trend, "FortyFiveDown")) { angle_deg =  45; count = 1; return; }
    if (!std::strcmp(trend, "SingleDown"))    { angle_deg =  90; count = 1; return; }
    if (!std::strcmp(trend, "DoubleDown"))    { angle_deg =  90; count = 2; return; }
    if (!std::strcmp(trend, "TripleDown"))    { angle_deg =  90; count = 3; return; }

    // иногда встречается "None"
    if (!std::strcmp(trend, "None"))          { angle_deg = 0; count = 0; return; }

    angle_deg = 0;
    count = 1;
}

// цвет BG по границам sgvBounds
// bounds[0]=low, bounds[1]=lowWarn, bounds[2]=highWarn, bounds[3]=high
static lv_color_t bg_color_for_sgv(float sgv, const float bounds[4]) {
    if (!std::isfinite(sgv)) return lv_color_make(160,160,160);

    const float low      = bounds[0];
    const float lowWarn  = bounds[1];
    const float highWarn = bounds[2];
    const float high     = bounds[3];

    if (sgv < low)      return lv_color_make(255, 0, 0);      // гипо
    if (sgv < lowWarn)  return lv_color_make(255, 160, 0);    // низко
    if (sgv <= highWarn)return lv_color_make(0, 255, 0);       // норма 
    if (sgv <= high)    return lv_color_make(255, 160, 0);    // высоко
    return lv_color_make(255, 0, 0);                          // очень высоко
}

// ======================================================
//                     Cache (previous values)
// ======================================================
static float  prev_sgv_mmol   = NAN;
static float  prev_iob        = NAN;
static float  prev_cob        = NAN;
static float  prev_delta_mmol = NAN;

static int    prev_bat        = -999;

static char   prev_bg_txt[16]    = "";
static char   prev_iob_txt[16]   = "";
static char   prev_cob_txt[16]   = "";
static char   prev_delta_txt[16] = "";
static char   prev_bat_txt[16]   = "";
static char   prev_age_txt[16]   = "";
static char   prev_time_txt[16]  = "";
static char   prev_trend_txt[24] = "";

static lv_color_t prev_delta_color = lv_color_make(255,0,0);
static lv_color_t prev_bat_color   = lv_color_make(100,255,0);
static lv_color_t prev_bg_color    = lv_color_hex(0xFFFF00);

// ======================================================
//                     main_update_full
// ======================================================
void main_update_full(const NightscoutData& ns) {
    if (!label_bg) return;

    // ---- BG (sgv) ----
    if (!std::isfinite(prev_sgv_mmol) || !std::isfinite(ns.sgv_mmol) ||
        std::fabs(ns.sgv_mmol - prev_sgv_mmol) > 0.01f) {

        prev_sgv_mmol = ns.sgv_mmol;

        char s[16];
        if (std::isfinite(ns.sgv_mmol) && ns.sgv_mmol > 0.0f)
            std::snprintf(s, sizeof(s), "%.1f", ns.sgv_mmol);
        else
            std::snprintf(s, sizeof(s), "--.-");

        set_label_if_changed(label_bg, s, prev_bg_txt, sizeof(prev_bg_txt));

        // цвет BG по границам
        lv_color_t c = bg_color_for_sgv(ns.sgv_mmol, ns.sgvBounds);
        set_color_if_changed(label_bg, c, &prev_bg_color);
    }

    // ---- IOB ----
    if (!std::isfinite(prev_iob) || !std::isfinite(ns.iob) || std::fabs(ns.iob - prev_iob) > 0.01f) {
        prev_iob = ns.iob;
        char s[16];
        if (std::isfinite(ns.iob)) std::snprintf(s, sizeof(s), "I: %.2f", ns.iob);
        else                       std::snprintf(s, sizeof(s), "I: --");
        set_label_if_changed(label_iob, s, prev_iob_txt, sizeof(prev_iob_txt));
    }

    // ---- COB ----
    if (!std::isfinite(prev_cob) || !std::isfinite(ns.cob) || std::fabs(ns.cob - prev_cob) > 0.1f) {
        prev_cob = ns.cob;
        char s[16];
        if (std::isfinite(ns.cob)) std::snprintf(s, sizeof(s), "C: %.0f", ns.cob);
        else                       std::snprintf(s, sizeof(s), "C: --");
        set_label_if_changed(label_cob, s, prev_cob_txt, sizeof(prev_cob_txt));
    }

    // ---- Delta (текст + цвет) ----
    if (!std::isfinite(prev_delta_mmol) || !std::isfinite(ns.delta_mmol) ||
        std::fabs(ns.delta_mmol - prev_delta_mmol) > 0.01f) {

        prev_delta_mmol = ns.delta_mmol;

        char s[16];
        if (std::isfinite(ns.delta_mmol)) std::snprintf(s, sizeof(s), "%+.1f", ns.delta_mmol);
        else                              std::snprintf(s, sizeof(s), "--");

        set_label_if_changed(label_delta, s, prev_delta_txt, sizeof(prev_delta_txt));

        lv_color_t c = (!std::isfinite(ns.delta_mmol)) ? lv_color_make(200, 200, 200)
                     : (ns.delta_mmol > 0.05f)         ? lv_color_make(0, 255, 0)
                     : (ns.delta_mmol < -0.05f)        ? lv_color_make(255, 0, 0)
                     :                                   lv_color_make(200, 200, 200);

        set_color_if_changed(label_delta, c, &prev_delta_color);
    }

    // ---- Battery ----
    {
        int bat = ns.pumpBatPct; // -1 если нет

        if (bat != prev_bat) {
            prev_bat = bat;

            const char* icon = battery_icon_for_pct(bat);
            if (label_battery) lv_label_set_text(label_battery, icon);

            char s[16];
            if (bat >= 0) std::snprintf(s, sizeof(s), "%d%%", bat);
            else          std::snprintf(s, sizeof(s), "--%%");
            set_label_if_changed(label_battery_level, s, prev_bat_txt, sizeof(prev_bat_txt));

            lv_color_t col = battery_color_for_pct(bat);
            set_color_if_changed(label_battery, col, &prev_bat_color);
            lv_obj_set_style_text_color(label_battery_level, col, LV_PART_MAIN);
        }
    }

    // ---- Age (минуты от update_time_ms) ----
    {
        int age_min = -1;
        time_t now = time(nullptr);

        if (now > 100000 && ns.update_time_ms > 0) {
            time_t upd = (time_t)(ns.update_time_ms / 1000ULL);
            if (now >= upd) age_min = (int)((now - upd) / 60);
        }

        char s[16];
        if (age_min >= 0) std::snprintf(s, sizeof(s), "%d мин", age_min);
        else              std::snprintf(s, sizeof(s), "--");
        set_label_if_changed(label_age, s, prev_age_txt, sizeof(prev_age_txt));
    }

    // ---- Time (HH:MM) ----
    {
        tm tml;
        if (getLocalTime(&tml, 10)) {
            char s[16];
            std::snprintf(s, sizeof(s), "%02d:%02d", tml.tm_hour, tml.tm_min);
            set_label_if_changed(label_time, s, prev_time_txt, sizeof(prev_time_txt));
        }
    }

    // ---- Trend arrows: angle + count ----
    if (str_changed(prev_trend_txt, ns.trend)) {
        std::strncpy(prev_trend_txt, ns.trend, sizeof(prev_trend_txt));
        prev_trend_txt[sizeof(prev_trend_txt) - 1] = '\0';

        int ang = 0, cnt = 1;
        trend_to_angle_and_count(ns.trend, ang, cnt);

        arrows_apply_layout(ang, cnt);
        arrow_set_angle_deg_via_right(ang, 300);
    }
}

// ======================================================
//                     ui_main_create
// ======================================================
lv_obj_t *ui_main_create() {
    main_screen = lv_obj_create(nullptr);
    lv_obj_clear_flag(main_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(main_screen, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(main_screen, LV_OPA_COVER, LV_PART_MAIN);

    // ---------- Arrow(s) ----------
    int32_t PX = 320 - 40, PY = 170 - 40;
    g_arrow_cx = PX;
    g_arrow_cy = PY;

    // // где будет центр вращения
    // int32_t PX = 320 - 40, PY = 170 - 40;
    // lv_obj_set_pos(arrow, PX - 30, PY - 15);


    for (int i = 0; i < 3; i++) {
        arrows[i] = lv_line_create(main_screen);
        lv_line_set_points(arrows[i], arrow_points, sizeof(arrow_points) / sizeof(arrow_points[0]));
        lv_obj_set_style_line_width(arrows[i], 8, LV_PART_MAIN);
        lv_obj_set_style_line_rounded(arrows[i], true, LV_PART_MAIN);
        lv_obj_set_style_line_color(arrows[i], lv_color_hex(0xFF0000), LV_PART_MAIN);

        lv_obj_set_style_transform_pivot_x(arrows[i], AR_PX, LV_PART_MAIN);
        lv_obj_set_style_transform_pivot_y(arrows[i], AR_PY, LV_PART_MAIN);

        lv_obj_set_pos(arrows[i], PX - AR_PX, PY - AR_PY);
    }

    // стартовый угол
    arrow_angle10_raw = -900;
    for (int i = 0; i < 3; i++) {
        lv_obj_set_style_transform_angle(arrows[i], arrow_angle10_raw, LV_PART_MAIN);
    }

    // по умолчанию: 1 стрелка
    arrows_apply_layout(0, 1);

    // ---------- BG ----------
    label_bg = lv_label_create(main_screen);
    lv_obj_set_style_text_color(label_bg, lv_color_hex(0xFFFF00), LV_PART_MAIN);
    lv_obj_set_style_text_font(label_bg, &roninset2_180, LV_PART_MAIN);
    lv_label_set_text(label_bg, "-.-");
    lv_obj_align(label_bg, LV_ALIGN_CENTER, -40, 10);

    // ---------- IOB ----------
    label_iob = lv_label_create(main_screen);
    lv_obj_set_style_text_color(label_iob, lv_color_make(200, 200, 200), LV_PART_MAIN);
    lv_obj_set_style_text_font(label_iob, &comfortaa_bold_18, LV_PART_MAIN);
    lv_label_set_text(label_iob, "I: --");
    lv_obj_align(label_iob, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    // ---------- COB ----------
    label_cob = lv_label_create(main_screen);
    lv_obj_set_style_text_color(label_cob, lv_color_make(200, 200, 200), LV_PART_MAIN);
    lv_obj_set_style_text_font(label_cob, &comfortaa_bold_18, LV_PART_MAIN);
    lv_label_set_text(label_cob, "C: --");
    lv_obj_align(label_cob, LV_ALIGN_BOTTOM_LEFT, 120, 0);

    // ---------- Delta ----------
    // якорь 1x1 в центре стрелки
    lv_obj_t * arrow_anchor = lv_obj_create(main_screen);
    lv_obj_set_size(arrow_anchor, 1, 1);
    lv_obj_set_pos(arrow_anchor, PX, PY);
    lv_obj_set_style_text_color(label_delta, lv_color_make(255, 0, 0), LV_PART_MAIN);
    //lv_obj_add_flag(arrow_anchor, LV_OBJ_FLAG_HIDDEN);

    label_delta = lv_label_create(main_screen);
    lv_obj_set_style_text_color(label_delta, lv_color_make(255, 0, 0), LV_PART_MAIN);
    lv_obj_set_style_text_font(label_delta, &volkswagen_medium_36, LV_PART_MAIN);
    lv_label_set_text(label_delta, "--");
    lv_obj_align_to(label_delta, arrow_anchor, LV_ALIGN_OUT_TOP_RIGHT, 0, -40);
   // lv_obj_align(label_delta, LV_ALIGN_RIGHT_MID, 0, 0);

    // ---------- Battery icon ----------
    label_battery = lv_label_create(main_screen);
    lv_obj_set_style_text_color(label_battery, lv_color_make(160, 160, 160), LV_PART_MAIN);
    lv_obj_set_style_text_font(label_battery, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_label_set_text(label_battery, "\uF242");
    lv_obj_align(label_battery, LV_ALIGN_TOP_LEFT, 0, 0);

    // ---------- Battery % ----------
    label_battery_level = lv_label_create(main_screen);
    lv_obj_set_style_text_color(label_battery_level, lv_color_make(160, 160, 160), LV_PART_MAIN);
    lv_obj_set_style_text_font(label_battery_level, &comfortaa_bold_18, LV_PART_MAIN);
    lv_label_set_text(label_battery_level, "--%");
    lv_obj_align_to(label_battery_level, label_battery, LV_ALIGN_OUT_RIGHT_MID, 2, 0);

    // ---------- Age ----------
    label_age = lv_label_create(main_screen);
    lv_obj_set_style_text_color(label_age, lv_color_make(0, 255, 0), LV_PART_MAIN);
    lv_obj_set_style_text_font(label_age, &comfortaa_bold_18, LV_PART_MAIN);
    lv_label_set_text(label_age, "--");
    lv_obj_align(label_age, LV_ALIGN_TOP_RIGHT, 0, 0);

    // ---------- Time ----------
    label_time = lv_label_create(main_screen);
    lv_obj_set_style_text_color(label_time, lv_color_make(200, 200, 200), LV_PART_MAIN);
    lv_obj_set_style_text_font(label_time, &comfortaa_bold_18, LV_PART_MAIN);
    lv_label_set_text(label_time, "--:--");
    lv_obj_align(label_time, LV_ALIGN_TOP_MID, 0, 0);

    return main_screen;
}
