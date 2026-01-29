#include "ui_first.h"
#include "fonts.h"

static lv_obj_t *first_screen = nullptr;
static lv_obj_t *label = nullptr;
static lv_obj_t *label_l1 = nullptr;

// Экран FIRST
lv_obj_t* ui_first_create(){

    // Используем текущий экран как корневой
    first_screen = lv_obj_create(nullptr); //lv_scr_act();
    lv_obj_set_style_bg_color(first_screen, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(first_screen, LV_OPA_COVER, LV_PART_MAIN);


    label = lv_label_create(first_screen);
    lv_obj_set_style_text_color(label, lv_color_hex(0xFF0000), LV_PART_MAIN);
    lv_obj_set_style_text_font(label, &comfortaa_bold_18, LV_PART_MAIN);
    lv_label_set_text(label, "ПРИВЕТСТВИЕ");
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);

    label_l1 = lv_label_create(first_screen);
    lv_obj_set_pos(label_l1, 1, 10);
    lv_obj_set_width(label_l1, 200);
    lv_obj_set_style_text_font(label_l1, &corporate_s_w10_16, LV_PART_MAIN);
    lv_obj_set_style_text_color(label_l1, lv_color_hex(0xCCFF33), LV_PART_MAIN); // greenyellow-ish
    lv_label_set_text(label_l1, "1) Подключитесь к Wi-Fi");

    return first_screen;
}
