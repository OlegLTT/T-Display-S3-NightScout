// Просмотр журнала с переносом строк, прокруткой и сохранением позиции при обновлении.
#pragma once
#include "lvgl.h"
#include "logs.h"

lv_obj_t* ui_logs_create();

void ui_logs_set_buffer(const LogBuffer* logs);

void ui_logs_refresh();

void ui_logs_scroll(int delta);

void ui_logs_scroll_page(int delta);

void logs_update_full(LogBuffer& logs);
