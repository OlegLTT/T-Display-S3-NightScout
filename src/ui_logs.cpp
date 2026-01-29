#include "ui_logs.h"
#include <cstdio>

static constexpr int LOG_CAP = (int)LogBuffer::kCap;
static constexpr int BUF_SZ  = (int)LogBuffer::kMsgLen + 1;

static lv_obj_t* g_scr_log = nullptr;
static lv_obj_t* g_col = nullptr;

static lv_obj_t* g_labels[LOG_CAP];
static lv_obj_t* g_measure = nullptr;

static const LogBuffer* g_logs = nullptr;
static uint32_t g_last_version = 0;

static lv_coord_t g_w = 0;
static lv_coord_t g_h = 0;
static const lv_font_t* g_font = &lv_font_montserrat_12; 

void logs_update_full(LogBuffer& logs) {}   // или const LogBuffer& если так у тебя

static lv_color_t color_for_level(LogLevel lvl) {
    switch (lvl) {
        case LogLevel::Warn:  return lv_palette_main(LV_PALETTE_YELLOW);
        case LogLevel::Error: return lv_palette_main(LV_PALETTE_RED);
        default:              return lv_palette_main(LV_PALETTE_GREEN);
    }
}

static void hide_all() {
    for (int i = 0; i < LOG_CAP; i++) {
        lv_obj_add_flag(g_labels[i], LV_OBJ_FLAG_HIDDEN);
    }
}

static void render_if_needed() {
    if (!g_scr_log || !g_col) return;
    if (!g_logs) { hide_all(); return; }

    uint32_t ver = g_logs->version();
    if (ver == g_last_version) return;
    g_last_version = ver;

    const size_t cnt = g_logs->count();
    if (cnt == 0) { hide_all(); return; }

    struct Pick { LogEntry e; lv_color_t col; };
    Pick picks[LOG_CAP];
    int pick_n = 0;
    lv_coord_t used_h = 0;

    // Выбираем с конца (newest first), пока влезает по высоте
    for (size_t i = 0; i < cnt && pick_n < LOG_CAP; i++) {
        LogEntry e = g_logs->getNewestFirst(i);

        char buf[BUF_SZ];
        snprintf(buf, sizeof(buf), "%s", picks[i].e.msg);

        // меряем высоту с wrap
        lv_label_set_text(g_measure, buf);
        lv_obj_update_layout(g_measure);
        lv_coord_t hh = lv_obj_get_height(g_measure);
        if (hh <= 0) hh = 1;

        if (used_h + hh > g_h) break;

        picks[pick_n++] = { e, color_for_level(e.level) };
        used_h += hh;
    }

    // Рисуем в обратном порядке: старые сверху, новые снизу
    int out = 0;
    for (int i = pick_n - 1; i >= 0; i--) {
        char buf[BUF_SZ];
        snprintf(buf, sizeof(buf), "%s", picks[i].e.msg);

        lv_obj_t* lb = g_labels[out++];
        lv_label_set_text(lb, buf);
        lv_obj_set_style_text_color(lb, picks[i].col, 0);
        lv_obj_clear_flag(lb, LV_OBJ_FLAG_HIDDEN);
    }

    for (int i = out; i < LOG_CAP; i++) {
        lv_obj_add_flag(g_labels[i], LV_OBJ_FLAG_HIDDEN);
    }

    lv_obj_update_layout(g_col);
}

static void logs_timer_cb(lv_timer_t*) {
    render_if_needed();
}

lv_obj_t* ui_logs_create() {
    // Создаём screen
    g_scr_log = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(g_scr_log, lv_color_black(), 0);
    lv_obj_set_style_border_width(g_scr_log, 0, 0);
    lv_obj_set_style_pad_all(g_scr_log, 0, 0);

    g_w = lv_disp_get_hor_res(nullptr);
    g_h = lv_disp_get_ver_res(nullptr);

    // контейнер-колонка
    g_col = lv_obj_create(g_scr_log);
    lv_obj_set_size(g_col, g_w, g_h);
    lv_obj_set_align(g_col, LV_ALIGN_TOP_LEFT);
    lv_obj_set_style_bg_opa(g_col, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(g_col, 0, 0);
    lv_obj_set_style_pad_all(g_col, 0, 0);
    lv_obj_set_style_pad_row(g_col, 0, 0);

    lv_obj_set_layout(g_col, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(g_col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(g_col,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);

    // пул label'ов
    for (int i = 0; i < LOG_CAP; i++) {
        g_labels[i] = lv_label_create(g_col);
        lv_label_set_long_mode(g_labels[i], LV_LABEL_LONG_WRAP);
        lv_obj_set_width(g_labels[i], g_w);
        lv_obj_set_style_text_font(g_labels[i], g_font, 0);
        lv_obj_set_style_pad_all(g_labels[i], 0, 0);
        lv_obj_add_flag(g_labels[i], LV_OBJ_FLAG_HIDDEN);
    }

    // скрытый label для измерений
    g_measure = lv_label_create(g_col);
    lv_label_set_long_mode(g_measure, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(g_measure, g_w);
    lv_obj_set_style_text_font(g_measure, g_font, 0);
    lv_obj_set_style_pad_all(g_measure, 0, 0);
    lv_obj_add_flag(g_measure, LV_OBJ_FLAG_HIDDEN);

    // обновлялка (например, 200 мс)
    lv_timer_create(logs_timer_cb, 200, nullptr);

    // начальное состояние
    hide_all();
    g_last_version = 0;

    return g_scr_log;
}

void ui_logs_set_buffer(const LogBuffer* logs) {
    g_logs = logs;
    g_last_version = 0; // чтобы сразу перерисовалось
    render_if_needed();
}

void ui_logs_refresh() {
    // принудительно
    g_last_version = 0;
    render_if_needed();
}
