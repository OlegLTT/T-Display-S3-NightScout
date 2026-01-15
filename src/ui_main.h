#pragma once

#include <lvgl.h>

//LV_FONT_DECLARE(lv_font_montserrat_36);

// создать главное окно и все виджеты
void ui_main_create();

// обновление времени (строка вида "12:34")
void ui_main_set_time(const char* text);

// обновление BG (строка вида "5.6" или "102")
void ui_main_set_bg(const char* text);

// обновление тренда (стрелка, можно текстом "↗", "→", "↘" и т.п.)
void ui_main_set_trend(const char* text);

// обновление "дельты" или дополнительной строки (например "+0.3 / 15 мин")
void ui_main_set_delta(const char* text);

// обновление строки статуса (Wi-Fi, источник данных и т.п.)
void ui_main_set_status(const char* text);
