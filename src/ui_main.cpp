#include "ui_main.h"
#include "fonts.h"

// Хэндлы LVGL-объектов
static lv_obj_t* label_time   = nullptr;
static lv_obj_t* label_bg     = nullptr;
static lv_obj_t* label_trend  = nullptr;
static lv_obj_t* label_delta  = nullptr;
static lv_obj_t* label_status = nullptr;

// Можно хранить основной контейнер, если потом понадобятся анимации/переключения
static lv_obj_t* main_screen  = nullptr;

void ui_main_create()
{
    // Используем текущий экран как корневой
    main_screen = lv_scr_act();
    lv_obj_set_style_bg_color(main_screen, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(main_screen, LV_OPA_COVER, 0);

    // ---------- Большой BG в центре ----------
    label_bg = lv_label_create(main_screen);
    lv_obj_set_style_text_color(label_bg, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_bg, &roninset2_180, 0);
    lv_label_set_text(label_bg, "--");
    lv_obj_align(label_bg, LV_ALIGN_CENTER, 0, -10);

    // ---------- Тренд-стрелка слева от BG ----------
    label_trend = lv_label_create(main_screen);
    lv_obj_set_style_text_color(label_trend, lv_color_hex(0x00FF00), 0); // зелёная стрелка
    lv_obj_set_style_text_font(label_trend, &lv_font_montserrat_14, 0);
    lv_label_set_text(label_trend, "→");
    lv_obj_align_to(label_trend, label_bg, LV_ALIGN_OUT_LEFT_MID, -10, 0);

    // ---------- Время в правом верхнем углу ----------
    label_time = lv_label_create(main_screen);
    lv_obj_set_style_text_color(label_time, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_time, &lv_font_montserrat_14, 0);
    lv_label_set_text(label_time, "--:--");
    lv_obj_align(label_time, LV_ALIGN_TOP_RIGHT, -8, 4);

    // ---------- Дельта под BG ----------
    label_delta = lv_label_create(main_screen);
    lv_obj_set_style_text_color(label_delta, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_style_text_font(label_delta, &lv_font_montserrat_14, 0);
    lv_label_set_text(label_delta, "+0.0 / 15 мин");
    lv_obj_align_to(label_delta, label_bg, LV_ALIGN_OUT_BOTTOM_MID, 0, 4);

    // ---------- Строка статуса внизу ----------
    label_status = lv_label_create(main_screen);
    lv_obj_set_style_text_color(label_status, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(label_status, &lv_font_montserrat_14, 0);
    lv_label_set_long_mode(label_status, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(label_status, lv_pct(100));
    lv_label_set_text(label_status, "Подключение...");
    lv_obj_align(label_status, LV_ALIGN_BOTTOM_MID, 0, -2);
}

// ----- функции обновления -----

void ui_main_set_time(const char* text)
{
    if (label_time)
        lv_label_set_text(label_time, text);
}

void ui_main_set_bg(const char* text)
{
    if (label_bg)
        lv_label_set_text(label_bg, text);
}

void ui_main_set_trend(const char* text)
{
    if (label_trend)
        lv_label_set_text(label_trend, text);
}

void ui_main_set_delta(const char* text)
{
    if (label_delta)
        lv_label_set_text(label_delta, text);
}

void ui_main_set_status(const char* text)
{
    if (label_status)
        lv_label_set_text(label_status, text);
}
