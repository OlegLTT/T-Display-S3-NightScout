// ===== ui_logs.cpp =====
#include "ui_logs.h"

#include <lvgl.h>
#include <cstdio>
#include <cstdint>

// =================== CONFIG ===================
static constexpr int        MAX_ROWS    = 60;   // сколько LVGL label держим
static constexpr uint32_t   REFRESH_MS  = 120;
static constexpr lv_coord_t SB_W        = 4;    // scrollbar width

// =================== LVGL OBJECTS ===================
static lv_obj_t* g_scr_log  = nullptr;
static lv_obj_t* g_root     = nullptr;

static lv_obj_t* g_labels[MAX_ROWS]{};

// скрытый label для измерения высоты строки после WRAP
static lv_obj_t* g_measure  = nullptr;

static lv_obj_t* g_sb_track = nullptr;
static lv_obj_t* g_sb_thumb = nullptr;

// =================== DATA ===================
static const LogBuffer* g_logs = nullptr;
static uint32_t g_last_version = 0;

// oldest-first index (0 = oldest)
static size_t    g_startOldest = 0;

// пиксельный срез сверху у верхней записи (0..height-1)
static lv_coord_t g_offPx = 0;

// tail mode: прижимаем хвост к низу экрана
static bool   g_followTail  = true;

// anchor (для удержания верхней записи при добавлении новых логов, когда НЕ в хвосте)
static bool     g_anchorValid  = false;
static uint32_t g_anchorSeqTop = 0;

static lv_coord_t g_w = 0;
static lv_coord_t g_h = 0;

static const lv_font_t* g_font = &lv_font_montserrat_12;
static lv_coord_t g_lineH = 12;

// сколько реально показали
static int g_shownCount = 0;

// =================== HELPERS ===================
static lv_color_t color_for_level(LogLevel lvl) {
    switch (lvl) {
        case LogLevel::Warn:  return lv_palette_main(LV_PALETTE_YELLOW);
        case LogLevel::Error: return lv_palette_main(LV_PALETTE_RED);
        default:              return lv_palette_main(LV_PALETTE_GREEN);
    }
}

static void hide_all() {
    for (int i = 0; i < MAX_ROWS; i++) {
        if (g_labels[i]) lv_obj_add_flag(g_labels[i], LV_OBJ_FLAG_HIDDEN);
    }
}

static LogEntry get_by_oldest_index(const LogBuffer* lb, size_t cnt, size_t oldestIndex) {
    const size_t newestFirst = (cnt - 1) - oldestIndex;
    return lb->getNewestFirst(newestFirst);
}

static bool find_oldest_index_by_seq(const LogBuffer* lb, size_t cnt, uint32_t seq, size_t& outOldestIndex) {
    for (size_t i = 0; i < cnt; i++) {
        LogEntry e = get_by_oldest_index(lb, cnt, i);
        if (e.seq == seq) { outOldestIndex = i; return true; }
    }
    return false;
}

static void clamp_start_basic() {
    if (!g_logs) { g_startOldest = 0; return; }
    const size_t cnt = g_logs->count();
    if (cnt == 0) { g_startOldest = 0; return; }
    if (g_startOldest >= cnt) g_startOldest = cnt - 1;
}

static lv_coord_t measure_entry_height(const char* text, lv_coord_t text_w) {
    if (!g_measure) return 0;

    lv_label_set_text(g_measure, text ? text : "");
    lv_obj_set_width(g_measure, text_w);
    lv_obj_update_layout(g_measure);

    lv_coord_t h = lv_obj_get_height(g_measure);
    if (h <= 0) h = lv_font_get_line_height(g_font);
    return h;
}

// tail-позиция (start + offPx), чтобы низ был прижат к низу экрана
static void compute_tail_pos(size_t& outStart, lv_coord_t& outOffPx) {
    outStart = 0;
    outOffPx = 0;

    if (!g_logs) return;
    const size_t cnt = g_logs->count();
    if (cnt == 0) return;

    const lv_coord_t text_w = g_w - SB_W;

    lv_coord_t used = 0;
    size_t start = cnt;  // будем уменьшать

    while (start > 0) {
        const size_t idx = start - 1;
        LogEntry e = get_by_oldest_index(g_logs, cnt, idx);
        lv_coord_t h = measure_entry_height(e.msg, text_w);

        used += h;
        start--;

        if (used >= g_h) {
            outStart = start;
            outOffPx = used - g_h; // сколько срезать сверху у outStart
            if (outOffPx < 0) outOffPx = 0;
            return;
        }
    }

    outStart = 0;
    outOffPx = 0;
}

// render: частичный верхний label по offPx, низ клипается root
static int render_from_pos(size_t startOldest, lv_coord_t offPx) {
    if (!g_logs) { hide_all(); g_shownCount = 0; return 0; }
    const size_t cnt = g_logs->count();
    if (cnt == 0) { hide_all(); g_shownCount = 0; return 0; }
    if (startOldest >= cnt) { hide_all(); g_shownCount = 0; return 0; }

    const lv_coord_t text_w = g_w - SB_W;

    hide_all();

    lv_coord_t y = -offPx;
    int shownEntries = 0;
    int usedLabels = 0;

    for (;;) {
        const size_t idx = startOldest + (size_t)shownEntries;
        if (idx >= cnt) break;
        if (usedLabels >= MAX_ROWS) break;

        LogEntry e = get_by_oldest_index(g_logs, cnt, idx);
        lv_coord_t h = measure_entry_height(e.msg, text_w);
        if (h <= 0) h = g_lineH;

        // запись полностью выше экрана — пропускаем, не расходуя label
        if (y + h <= 0) {
            y += h;
            shownEntries++;
            continue;
        }

        // запись полностью ниже экрана — стоп
        if (y >= g_h) break;

        lv_obj_t* lb = g_labels[usedLabels];
        if (!lb) break;

        lv_label_set_text(lb, e.msg);
        lv_obj_set_style_text_color(lb, color_for_level(e.level), 0);

        lv_opa_t opa = (e.seq & 1) ? LV_OPA_70 : LV_OPA_100;
        lv_obj_set_style_text_opa(lb, opa, 0);

        lv_obj_set_pos(lb, 0, y);
        lv_obj_set_width(lb, text_w);
        lv_obj_clear_flag(lb, LV_OBJ_FLAG_HIDDEN);
        lv_obj_update_layout(lb);

        // реальная высота после layout
        lv_coord_t real_h = lv_obj_get_height(lb);
        if (real_h > 0) h = real_h;

        y += h;
        shownEntries++;
        usedLabels++;

        // защита от бесконечного цикла
        if (shownEntries > (int)cnt) break;
    }

    // спрятать остаток label'ов
    for (int i = usedLabels; i < MAX_ROWS; i++) {
        if (g_labels[i]) lv_obj_add_flag(g_labels[i], LV_OBJ_FLAG_HIDDEN);
    }

    // shownCount — сколько лог-записей реально “в окне” начиная с startOldest (включая те, что могут быть частично видны)
    // нам важнее usedLabels для "сколько строк на экране", но оставим g_shownCount как usedLabels
    g_shownCount = usedLabels;
    return usedLabels;
}

// scrollbar: по позиции (start + доля offPx)
static void update_scrollbar() {
    if (!g_sb_track || !g_sb_thumb || !g_logs) return;

    const size_t cnt = g_logs->count();
    if (cnt == 0) {
        lv_obj_add_flag(g_sb_track, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    // хвостовая позиция (максимум)
    size_t tailStart = 0;
    lv_coord_t tailOff = 0;
    compute_tail_pos(tailStart, tailOff);

    // если всё помещается — не показываем
    if (tailStart == 0 && tailOff == 0) {
        lv_obj_add_flag(g_sb_track, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_obj_clear_flag(g_sb_track, LV_OBJ_FLAG_HIDDEN);

    lv_coord_t trackH = lv_obj_get_height(g_sb_track);
    if (trackH <= 0) trackH = 1;

    // thumbH — грубо по доле видимых записей (ок)
    const size_t shown = (g_shownCount > 0) ? (size_t)g_shownCount : 1;
    lv_coord_t thumbH = (lv_coord_t)((uint32_t)trackH * (uint32_t)shown / (uint32_t)cnt);
    if (thumbH < 10) thumbH = 10;
    if (thumbH > trackH) thumbH = trackH;

    lv_coord_t maxY = trackH - thumbH;

    // позицию переводим в "виртуальные единицы": start * 1024 + frac
    // tailPos аналогично
    const lv_coord_t text_w = g_w - SB_W;

    auto pos_units = [&](size_t start, lv_coord_t off) -> uint32_t {
        // вычислим высоту верхней записи, чтобы получить долю off
        if (!g_logs) return 0;
        if (start >= cnt) return (uint32_t)(cnt * 1024u);
        LogEntry e = get_by_oldest_index(g_logs, cnt, start);
        lv_coord_t h = measure_entry_height(e.msg, text_w);
        if (h <= 0) h = g_lineH;
        if (off < 0) off = 0;
        if (off > h) off = h;
        uint32_t frac = (uint32_t)((uint32_t)off * 1024u / (uint32_t)h);
        return (uint32_t)start * 1024u + frac;
    };

    const uint32_t curU  = pos_units(g_startOldest, g_offPx);
    uint32_t tailU = pos_units(tailStart, tailOff);  // <-- УБРАЛИ const

    uint32_t yU = curU;
    if (yU > tailU) yU = tailU;
    if (tailU == 0) tailU = 1;

    lv_coord_t y = (lv_coord_t)((uint64_t)maxY * (uint64_t)yU / (uint64_t)tailU);

    lv_obj_set_height(g_sb_thumb, thumbH);
    lv_obj_set_y(g_sb_thumb, y);
}

static void render_force() {
    if (!g_scr_log || !g_root) return;

    if (!g_logs || g_logs->count() == 0) {
        hide_all();
        update_scrollbar();
        return;
    }

    clamp_start_basic();

    if (g_followTail) {
        compute_tail_pos(g_startOldest, g_offPx);
    } else {
        // поджать offPx по высоте верхней записи
        const size_t cnt = g_logs->count();
        if (g_startOldest < cnt) {
            const lv_coord_t text_w = g_w - SB_W;
            LogEntry top = get_by_oldest_index(g_logs, cnt, g_startOldest);
            lv_coord_t h = measure_entry_height(top.msg, text_w);
            if (h <= 0) h = g_lineH;
            if (g_offPx < 0) g_offPx = 0;
            if (g_offPx >= h) g_offPx = h - 1;
            if (g_offPx < 0) g_offPx = 0;
        } else {
            g_offPx = 0;
        }
    }

    render_from_pos(g_startOldest, g_offPx);

    // обновим followTail по факту
    size_t tailStart = 0;
    lv_coord_t tailOff = 0;
    compute_tail_pos(tailStart, tailOff);
    g_followTail = (g_startOldest == tailStart && g_offPx == tailOff);

    // anchor: держим верхнюю запись при добавлении новых логов (если не хвост)
    const size_t cnt = g_logs->count();
    if (!g_followTail && cnt > 0) {
        LogEntry top = get_by_oldest_index(g_logs, cnt, g_startOldest);
        g_anchorSeqTop = top.seq;
        g_anchorValid  = true;
    } else {
        g_anchorValid = false;
    }

    update_scrollbar();
}

static void on_buffer_changed() {
    if (!g_logs) return;

    const size_t cnt = g_logs->count();
    if (cnt == 0) { g_startOldest = 0; g_offPx = 0; render_force(); return; }

    clamp_start_basic();

    if (g_followTail) {
        render_force();
        return;
    }

    // удерживаем верхнюю запись по seq (offPx сохраняем)
    if (g_anchorValid) {
        size_t found = 0;
        if (find_oldest_index_by_seq(g_logs, cnt, g_anchorSeqTop, found)) {
            g_startOldest = found;
        } else {
            clamp_start_basic();
            g_offPx = 0;
        }
    }

    render_force();
}

static void render_if_needed() {
    if (!g_logs) return;
    const uint32_t ver = g_logs->version();
    if (ver == g_last_version) return;
    g_last_version = ver;
    on_buffer_changed();
}

static void logs_timer_cb(lv_timer_t*) {
    render_if_needed();
}

// =================== PUBLIC API ===================
lv_obj_t* ui_logs_create() {
    g_scr_log = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(g_scr_log, lv_color_black(), 0);
    lv_obj_set_style_border_width(g_scr_log, 0, 0);
    lv_obj_set_style_pad_all(g_scr_log, 0, 0);

    g_w = lv_disp_get_hor_res(nullptr);
    g_h = lv_disp_get_ver_res(nullptr);

    g_lineH = lv_font_get_line_height(g_font);
    if (g_lineH < 10) g_lineH = 10;

    // root: plain container (NO FLEX!)
    g_root = lv_obj_create(g_scr_log);
    lv_obj_set_size(g_root, g_w - SB_W, g_h);
    lv_obj_align(g_root, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_opa(g_root, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(g_root, 0, 0);
    lv_obj_set_style_pad_all(g_root, 0, 0);
    lv_obj_clear_flag(g_root, LV_OBJ_FLAG_SCROLLABLE);

    // важно: клип, чтобы частичные строки были без артефактов
    lv_obj_set_style_clip_corner(g_root, true, 0);

    // измеритель
    g_measure = lv_label_create(g_root);
    lv_obj_set_width(g_measure, g_w - SB_W);
    lv_obj_set_style_text_font(g_measure, g_font, 0);
    lv_obj_set_style_text_line_space(g_measure, 0, 0);
    lv_obj_set_style_text_letter_space(g_measure, 0, 0);
    lv_label_set_long_mode(g_measure, LV_LABEL_LONG_WRAP);
    lv_label_set_text(g_measure, "");
    lv_obj_add_flag(g_measure, LV_OBJ_FLAG_HIDDEN);

    // labels
    const lv_coord_t text_w = g_w - SB_W;
    for (int i = 0; i < MAX_ROWS; i++) {
        lv_obj_t* lb = lv_label_create(g_root);
        g_labels[i] = lb;

        lv_obj_set_pos(lb, 0, 0);
        lv_obj_set_width(lb, text_w);
        lv_obj_set_style_text_font(lb, g_font, 0);
        lv_obj_set_style_text_line_space(lb, 0, 0);
        lv_obj_set_style_text_letter_space(lb, 0, 0);

        lv_label_set_long_mode(lb, LV_LABEL_LONG_WRAP);
        lv_label_set_text(lb, "");

        lv_obj_add_flag(lb, LV_OBJ_FLAG_HIDDEN);
    }

    // scrollbar
    g_sb_track = lv_obj_create(g_scr_log);
    lv_obj_set_size(g_sb_track, SB_W, g_h);
    lv_obj_align(g_sb_track, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_set_style_bg_opa(g_sb_track, LV_OPA_10, 0);
    lv_obj_set_style_border_width(g_sb_track, 0, 0);
    lv_obj_set_style_pad_all(g_sb_track, 0, 0);
    lv_obj_clear_flag(g_sb_track, LV_OBJ_FLAG_SCROLLABLE);

    g_sb_thumb = lv_obj_create(g_sb_track);
    lv_obj_set_width(g_sb_thumb, SB_W);
    lv_obj_set_style_bg_opa(g_sb_thumb, LV_OPA_60, 0);
    lv_obj_set_style_border_width(g_sb_thumb, 0, 0);
    lv_obj_set_style_pad_all(g_sb_thumb, 0, 0);
    lv_obj_clear_flag(g_sb_thumb, LV_OBJ_FLAG_SCROLLABLE);

    lv_timer_create(logs_timer_cb, REFRESH_MS, nullptr);

    // reset state
    g_logs         = nullptr;
    g_last_version = 0;
    g_startOldest  = 0;
    g_offPx        = 0;
    g_followTail   = true;
    g_anchorValid  = false;
    g_shownCount   = 0;

    hide_all();
    update_scrollbar();
    return g_scr_log;
}

void ui_logs_set_buffer(const LogBuffer* logs) {
    g_logs = logs;
    g_last_version = 0;
    g_startOldest  = 0;
    g_offPx        = 0;
    g_followTail   = true;
    g_anchorValid  = false;
    g_shownCount   = 0;

    render_force();
}

void ui_logs_refresh() {
    g_last_version = 0;
    render_force();
}

void ui_logs_scroll(int delta) {
    if (!g_logs) return;
    const size_t cnt = g_logs->count();
    if (cnt == 0) return;

    const lv_coord_t text_w = g_w - SB_W;

    lv_coord_t step = g_lineH;
    if (step < 8) step = 8;

    if (delta > 0) { // вверх к старым
        g_followTail = false;

        g_offPx -= step;
        while (g_offPx < 0) {
            if (g_startOldest == 0) { g_offPx = 0; break; }

            const size_t prev = g_startOldest - 1;
            LogEntry e = get_by_oldest_index(g_logs, cnt, prev);
            lv_coord_t h = measure_entry_height(e.msg, text_w);
            if (h <= 0) h = step;

            g_startOldest = prev;
            g_offPx += h;
        }

    } else if (delta < 0) { // вниз к новым
        g_offPx += step;

        while (true) {
            if (g_startOldest >= cnt) { g_startOldest = cnt - 1; g_offPx = 0; break; }

            LogEntry top = get_by_oldest_index(g_logs, cnt, g_startOldest);
            lv_coord_t h = measure_entry_height(top.msg, text_w);
            if (h <= 0) h = step;

            if (g_offPx < h) break;

            g_offPx -= h;

            if (g_startOldest + 1 < cnt) {
                g_startOldest++;
                g_followTail = false;
            } else {
                g_followTail = true;
                break;
            }
        }
    }

    // если дошли до хвоста — включим followTail
    size_t tailStart = 0;
    lv_coord_t tailOff = 0;
    compute_tail_pos(tailStart, tailOff);
    if (g_startOldest > tailStart || (g_startOldest == tailStart && g_offPx >= tailOff)) {
        g_followTail = true;
    }

    render_force();
}

void ui_logs_scroll_page(int delta) {
    if (!g_logs) return;
    const size_t cnt = g_logs->count();
    if (cnt == 0) return;

    lv_coord_t pagePx = g_h - g_lineH;
    if (pagePx < g_lineH) pagePx = g_lineH;

    int steps = (int)(pagePx / (g_lineH > 0 ? g_lineH : 10));
    if (steps < 1) steps = 1;

    for (int i = 0; i < steps; i++) {
        ui_logs_scroll(delta);
    }
}
