#include "ui_clock.h"
#include "fonts.h"

static lv_obj_t* clock_screen = nullptr;

static lv_obj_t *label_time = nullptr;

void clock_update_full(const NightscoutData& ns) {
    
}

lv_obj_t* ui_clock_create()
{
    // Создаём отдельный экран-объект
    clock_screen = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(clock_screen, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(clock_screen, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_t* label = lv_label_create(clock_screen);
    lv_label_set_text(label, "23:47");
    lv_obj_set_style_text_color(label, lv_color_hex(0x00FF00), LV_PART_MAIN);
    lv_obj_set_style_text_font(label, &directive_four_condensed_bold_100, LV_PART_MAIN);
    lv_obj_align(label, LV_ALIGN_RIGHT_MID, 0, 0);
   // lv_obj_center(label);

    // ---------- age ----------
    // label_age = lv_label_create(main_screen);
    // lv_obj_set_style_text_color(label_age, lv_color_make(0, 255, 0), LV_PART_MAIN);
    // lv_obj_set_style_text_font(label_age, &comfortaa_bold_18, LV_PART_MAIN);
    // lv_label_set_text(label_age, "2 мин");
    // lv_obj_align(label_age, LV_ALIGN_TOP_RIGHT, 0, 0);


    return clock_screen;
}
