#include "ui_clock.h"

static lv_obj_t* clock_screen = nullptr;

lv_obj_t* ui_clock_create()
{
    // Создаём отдельный экран-объект
    clock_screen = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(clock_screen, lv_color_hex(0x102030), 0);
    lv_obj_set_style_bg_opa(clock_screen, LV_OPA_COVER, 0);

    lv_obj_t* label = lv_label_create(clock_screen);
    lv_label_set_text(label, "CLOCK SCREEN");
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_center(label);

    return clock_screen;
}
