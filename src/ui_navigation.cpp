// Навигация и индикация тревоги: сенсорная кнопка CoreS3 и компактный индикатор T-Display.
#include "ui_navigation.h"
#include "ui_layout.h"
#include "fonts.h"
#include <cstdio>
#include <cstring>

#if defined(BOARD_M5CORE_S3)
namespace ui_navigation {
namespace {
constexpr int32_t swipe_threshold = 35;
constexpr uint32_t alarm_background = 0x101010;

Callback on_action = nullptr;
lv_obj_t* reserved_bar = nullptr;
lv_obj_t* alarm_button = nullptr;
lv_obj_t* alarm_label = nullptr;
bool snooze_pending = false;
int current_mode = -1;
bool portal_was_active = false;
bool pointer_down = false;
bool tracking = false;
int16_t start_x = 0, start_y = 0;
int16_t last_x = 0, last_y = 0;
bool action_pending = false;
Action pending_action = Action::SwitchLeft;

void queue(Action action) {
    pending_action = action;
    action_pending = true;
}
}

void init(Callback callback) {
    on_action = callback;
    tracking = false;
    action_pending = false;
    current_mode = -1;

    if (!reserved_bar) {
        reserved_bar = lv_obj_create(lv_layer_top());
        lv_obj_remove_style_all(reserved_bar);
        lv_obj_remove_flag(reserved_bar, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_remove_flag(reserved_bar, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_size(reserved_bar, lv_pct(100), ui_layout::reserved_bottom_height);
        lv_obj_align(reserved_bar, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_set_style_bg_color(reserved_bar, lv_color_hex(alarm_background), 0);
        lv_obj_set_style_bg_opa(reserved_bar, LV_OPA_COVER, 0);

        alarm_button = lv_button_create(reserved_bar);
        lv_obj_remove_style_all(alarm_button);
        lv_obj_remove_flag(alarm_button, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_size(alarm_button, lv_pct(100), lv_pct(100));
        lv_obj_set_style_bg_color(alarm_button, lv_color_hex(alarm_background), 0);
        lv_obj_set_style_bg_opa(alarm_button, LV_OPA_COVER, 0);
        lv_obj_set_style_text_color(alarm_button, lv_color_white(), 0);
        lv_obj_set_style_text_font(alarm_button, &comfortaa_bold_18, 0);
        lv_obj_add_event_cb(alarm_button, [](lv_event_t*) {
            snooze_pending = true;
        }, LV_EVENT_CLICKED, nullptr);
        alarm_label = lv_label_create(alarm_button);
        lv_obj_center(alarm_label);
        set_alarm_status(false, false, false, true, 0, 0);
    }
    update(false, false);
}

void set_alarm_status(bool enabled, bool valid, bool sounding, bool audio_available,
                      uint32_t remaining_seconds, uint8_t snooze_minutes, bool missing) {
    if (!alarm_button) return;
    if (enabled) lv_obj_remove_flag(alarm_button, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(alarm_button, LV_OBJ_FLAG_HIDDEN);
    char text[128];
    uint32_t color = 0x999999;
    if (!enabled) {
        std::snprintf(text, sizeof(text), "Звук выключен");
    } else if (!audio_available) {
        std::snprintf(text, sizeof(text), "Ошибка звука");
        color = 0xFF0000;
    } else if (remaining_seconds) {
        color = 0x6ABEFF;
        std::snprintf(text, sizeof(text), "Отсрочка %u мин | %lu:%02lu",
            unsigned(snooze_minutes), (unsigned long)(remaining_seconds / 60),
            (unsigned long)(remaining_seconds % 60));
    } else if (sounding) {
        std::snprintf(text, sizeof(text), "%s", missing ? "Нет данных 15+ мин" : "Тревога!");
        color = 0xFF0000;
    } else if (!valid) {
        std::snprintf(text, sizeof(text), "Нет свежих данных");
        color = 0xFFD166;
    } else {
        std::snprintf(text, sizeof(text), "Звук включён");
        color = 0x00FF00;
    }
    static char previous_text[128] = {};
    static uint32_t previous_color = UINT32_MAX;
    if (std::strcmp(previous_text, text) != 0) {
        lv_label_set_text(alarm_label, text);
        std::snprintf(previous_text, sizeof(previous_text), "%s", text);
    }
    if (color != previous_color) {
        lv_obj_set_style_text_color(alarm_label, lv_color_hex(color), 0);
        previous_color = color;
    }
    if (enabled && audio_available) lv_obj_remove_state(alarm_button, LV_STATE_DISABLED);
    else lv_obj_add_state(alarm_button, LV_STATE_DISABLED);
}

void update(bool logs_active, bool portal_active) {
    const int mode = logs_active ? 1 : (portal_active ? 2 : 0);
    if (mode == current_mode && portal_active == portal_was_active) return;
    current_mode = mode;
    portal_was_active = portal_active;
    tracking = false;
    action_pending = false;
    snooze_pending = false;
    if (reserved_bar) {
        if (mode == 0) lv_obj_remove_flag(reserved_bar, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(reserved_bar, LV_OBJ_FLAG_HIDDEN);
    }

}

// Жест выбирается после отпускания; диагональные перемещения не запускают навигацию.
void feed_touch(bool pressed, int16_t x, int16_t y) {
    if (pressed) {
        if (!pointer_down) {
            tracking = on_action && current_mode >= 0 && current_mode != 2
                    && !action_pending && x >= 0 && x < ui_layout::content_width()
                    && y >= 0 && y < ui_layout::content_height(current_mode == 0);
            start_x = x;
            start_y = y;
        }
        pointer_down = true;
        last_x = x;
        last_y = y;
        return;
    }

    const bool complete = pointer_down && tracking;
    pointer_down = false;
    tracking = false;
    if (!complete || action_pending) return;

    const int32_t dx = static_cast<int32_t>(last_x) - start_x;
    const int32_t dy = static_cast<int32_t>(last_y) - start_y;
    const int32_t ax = dx < 0 ? -dx : dx;
    const int32_t ay = dy < 0 ? -dy : dy;

    if (current_mode == 0 && ax >= swipe_threshold && ax * 2 >= ay * 3) {
        queue(dx < 0 ? Action::SwitchLeft : Action::SwitchRight);
    } else if (current_mode == 1 && ay >= swipe_threshold && ay * 2 >= ax * 3) {

        queue(dy < 0 ? Action::ScrollDown : Action::ScrollUp);
    }
}

// Обработчики интерфейса откладывают действие до основного цикла, чтобы не менять экран из события LVGL.
void poll() {
    if (snooze_pending && on_action && current_mode == 0) {
        snooze_pending = false;
        on_action(Action::CycleAlarmSnooze);
    }
    if (!action_pending || !on_action) return;
    const Action action = pending_action;
    action_pending = false;
    on_action(action);
}
}
#elif defined(BOARD_TDISPLAY_S3)
namespace ui_navigation {
namespace {
lv_obj_t* alarm_label = nullptr;
lv_obj_t* alarm_timer = nullptr;
lv_obj_t* alarm_slash = nullptr;
bool alarm_enabled = false;

// На экранах журнала и настроек индикатор тревоги скрыт.
bool screen_hides_alarm = true;
}
void init(Callback) {
    alarm_label = lv_label_create(lv_layer_top());
    lv_obj_set_style_text_font(alarm_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_align(alarm_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(alarm_label, 46);
    lv_obj_align(alarm_label, LV_ALIGN_LEFT_MID, 1, 0);
    lv_obj_remove_flag(alarm_label, LV_OBJ_FLAG_CLICKABLE);
    alarm_slash = lv_line_create(alarm_label);
    static const lv_point_precise_t slash_points[] = {{10, 25}, {36, 2}};
    lv_line_set_points(alarm_slash, slash_points, 2);
    lv_obj_set_style_line_width(alarm_slash, 2, 0);
    lv_obj_set_style_line_color(alarm_slash, lv_color_hex(0xFF0000), 0);
    lv_obj_remove_flag(alarm_slash, LV_OBJ_FLAG_CLICKABLE);
    alarm_timer = lv_label_create(lv_layer_top());
    lv_obj_set_style_text_font(alarm_timer, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_align(alarm_timer, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(alarm_timer, 46);
    lv_obj_remove_flag(alarm_timer, LV_OBJ_FLAG_CLICKABLE);
    set_alarm_status(false, false, false, true, 0, 0);
}
void update(bool logs_active, bool portal_active) {
    screen_hides_alarm = logs_active || portal_active;
    if (!alarm_label) return;
    lv_obj_t* labels[] = {alarm_label, alarm_timer};
    for (lv_obj_t* label : labels) {
        if (screen_hides_alarm || !alarm_enabled) lv_obj_add_flag(label, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_remove_flag(label, LV_OBJ_FLAG_HIDDEN);
    }
}
void set_alarm_status(bool enabled, bool valid, bool sounding, bool available,
                      uint32_t remaining, uint8_t, bool missing) {
    alarm_enabled = enabled;
    if (!alarm_label) return;
    update(screen_hides_alarm, false);
    char text[32];
    uint32_t color = 0x999999;
    if (!enabled) std::snprintf(text, sizeof(text), LV_SYMBOL_MUTE);
    else if (!available) {
        std::snprintf(text, sizeof(text), LV_SYMBOL_WARNING);
        color = 0xFF0000;
    } else if (remaining) {
        std::snprintf(text, sizeof(text), LV_SYMBOL_MUTE);
        color = 0xFF0000;
    } else {
        std::snprintf(text, sizeof(text), LV_SYMBOL_VOLUME_MAX);
        color = 0x00FF00;
    }

    // Перечёркнутый красный значок и минуты отображаются только при активной отсрочке.
    if (enabled && available && remaining) lv_obj_remove_flag(alarm_slash, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(alarm_slash, LV_OBJ_FLAG_HIDDEN);
    if (std::strcmp(lv_label_get_text(alarm_label), text) != 0) lv_label_set_text(alarm_label, text);
    lv_obj_set_style_text_color(alarm_label, lv_color_hex(color), 0);
    char countdown[16] = {};
    if (enabled && available && remaining) {
        std::snprintf(countdown, sizeof(countdown), "%lu",
            (unsigned long)((remaining + 59) / 60));
    }
    if (std::strcmp(lv_label_get_text(alarm_timer), countdown) != 0)
        lv_label_set_text(alarm_timer, countdown);
    lv_obj_set_style_text_color(alarm_timer, lv_color_hex(color), 0);
    lv_obj_align_to(alarm_timer, alarm_label, LV_ALIGN_OUT_BOTTOM_MID, 0, 2);
}
void feed_touch(bool, int16_t, int16_t) {}
void poll() {}
}
#endif
