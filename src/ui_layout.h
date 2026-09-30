// Общие размеры интерфейса и размещение крупных цифр рядом с индикатором тревоги.
#pragma once

#include "board_config.h"
#include <lvgl.h>

namespace ui_layout {

// На T-Display масштабируем крупные цифры так, чтобы они не заходили в полосу индикатора слева.
inline void fit_beside_alarm(lv_obj_t* label, int right, int y_offset) {
#if defined(BOARD_TDISPLAY_S3)
    constexpr int left = 50;
    lv_obj_update_layout(label);
    const int width = lv_obj_get_width(label);
    const int scale = width > right - left ? (right - left) * 256 / width : 256;
    lv_obj_set_style_transform_pivot_x(label, lv_pct(50), 0);
    lv_obj_set_style_transform_pivot_y(label, lv_pct(50), 0);
    lv_obj_set_style_transform_scale_x(label, scale, 0);
    lv_obj_set_style_transform_scale_y(label, scale, 0);
    lv_obj_align(label, LV_ALIGN_CENTER,
        (left + right) / 2 - lv_display_get_horizontal_resolution(nullptr) / 2, y_offset);
#endif
}
#if defined(BOARD_M5CORE_S3)

// Нижняя полоса CoreS3 предназначена для сенсорного управления отсрочкой.
constexpr int reserved_bottom_height = 44;
#else
constexpr int reserved_bottom_height = 0;
#endif

inline int content_width() {
    return lv_display_get_horizontal_resolution(nullptr);
}

inline int content_height(bool reserve_alarm_control = true) {
    return lv_display_get_vertical_resolution(nullptr)
        - (reserve_alarm_control ? reserved_bottom_height : 0);
}

inline void prepare_screen(lv_obj_t* screen, bool reserve_alarm_control = true) {
#if defined(BOARD_M5CORE_S3)
    lv_obj_set_style_pad_bottom(screen,
        reserve_alarm_control ? reserved_bottom_height : 0, 0);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
#endif
}
}
