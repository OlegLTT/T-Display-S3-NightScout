#include "ui_main.h"
#include "fonts.h"

// LV_FONT_DECLARE(lv_font_montserrat_14);
// LV_FONT_DECLARE(lv_font_montserrat_16);
// LV_FONT_DECLARE(lv_font_montserrat_24);
// LV_FONT_DECLARE(lv_font_montserrat_36);
// LV_FONT_DECLARE(lv_font_roninset2_180);

// Хэндлы LVGL-объектов
static lv_obj_t* label_time   = nullptr;
static lv_obj_t* label_bg     = nullptr;
static lv_obj_t* label_trend  = nullptr;
static lv_obj_t* label_delta  = nullptr;
static lv_obj_t* label_status = nullptr;

// Можно хранить основной контейнер, если потом понадобятся анимации/переключения
static lv_obj_t* main_screen  = nullptr;

// Точки стрелки (горизонтальная вправо)
// (0,10) -> (40,10) — ствол
// (40,10) -> (30,0) и (40,10) -> (30,20) — «ушки» стрелки
static lv_point_precise_t  arrow_points[] = {{ 0, 15 }, { 60, 15 }, { 48,  3 }, { 60, 15 }, { 48, 27 }};

static void anim_rotate_cb(void * var, int32_t angle)
{
    lv_obj_t * obj = (lv_obj_t *)var;
    lv_obj_set_style_transform_angle(obj, angle, 0);
}

lv_obj_t* ui_main_create()
{
    // Используем текущий экран как корневой
    main_screen = lv_scr_act();
    lv_obj_set_style_bg_color(main_screen, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(main_screen, LV_OPA_COVER, 0);

    lv_obj_t * arrow = lv_line_create(main_screen);
   // lv_line_set_points(arrow, arrow_points, LV_ARRAY_SIZE(arrow_points));
    lv_line_set_points(arrow, arrow_points, sizeof(arrow_points) / sizeof(arrow_points[0]));
    lv_obj_set_style_line_width(arrow, 8, LV_PART_MAIN);
    lv_obj_set_style_line_rounded(arrow, true, LV_PART_MAIN);
    lv_obj_set_style_line_color(arrow, lv_color_hex(0xFF0000), LV_PART_MAIN);
    lv_obj_set_pos(arrow, 320 - 62-10, 170 - 62);   // x, y
    // Повернуть стрелку на 90° (углы в десятках градусов: 90° = 900)
    
    // Точка вращения — центр  геометрии (40x20)
    lv_obj_set_style_transform_pivot_x(arrow, 30, LV_PART_MAIN);
    lv_obj_set_style_transform_pivot_y(arrow, 15, LV_PART_MAIN);
//=================
    lv_anim_t a;
    lv_anim_init(&a);

    lv_anim_set_var(&a, arrow);
    lv_anim_set_exec_cb(&a, anim_rotate_cb);

    lv_anim_set_time(&a, 2000);          // 2 секунды на оборот
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_values(&a, 0, 3600);     // 0° → 360° (в десятках градусов)

    lv_anim_set_path_cb(&a, lv_anim_path_linear);

    lv_anim_start(&a);


  //  lv_obj_set_style_transform_angle(arrow, 900, LV_PART_MAIN);

    // ---------- Большой BG в центре ----------
    label_bg = lv_label_create(main_screen);
    lv_obj_set_style_text_color(label_bg, lv_color_hex(0xFFFF00), LV_PART_MAIN);
    lv_obj_set_style_text_font(label_bg, &roninset2_180, LV_PART_MAIN);
    lv_label_set_text(label_bg, "5.6");
    lv_obj_align(label_bg, LV_ALIGN_CENTER, 0, -10);

    // ---------- Тренд-стрелка слева от BG ----------
    label_trend = lv_label_create(main_screen);
    lv_obj_set_style_text_color(label_trend, lv_color_hex(0x00FF00), LV_PART_MAIN); // зелёная стрелка
    lv_obj_set_style_text_font(label_trend, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_label_set_text(label_trend, "->");
    lv_obj_align_to(label_trend, label_bg, LV_ALIGN_OUT_LEFT_MID, -10, 0);

    // ---------- Время в правом верхнем углу ----------
    label_time = lv_label_create(main_screen);
    lv_obj_set_style_text_color(label_time, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(label_time, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_label_set_text(label_time, "20:30");
    lv_obj_align(label_time, LV_ALIGN_TOP_RIGHT, -8, 4);

    // ---------- Дельта под BG ----------
    label_delta = lv_label_create(main_screen);
    lv_obj_set_style_text_color(label_delta, lv_color_hex(0xAAAAAA), LV_PART_MAIN);
    lv_obj_set_style_text_font(label_delta, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_label_set_text(label_delta, "+0.0 / 15 min");
    lv_obj_align_to(label_delta, label_bg, LV_ALIGN_OUT_BOTTOM_MID, 0, 4);

    // ---------- Строка статуса внизу ----------
    label_status = lv_label_create(main_screen);
    lv_obj_set_style_text_color(label_status, lv_color_hex(0x888888), LV_PART_MAIN);
    lv_obj_set_style_text_font(label_status, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_label_set_long_mode(label_status, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(label_status, lv_pct(100));
    lv_label_set_text(label_status, "Connecting...");
    lv_obj_align(label_status, LV_ALIGN_BOTTOM_MID, 0, -2);



    return main_screen;
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
