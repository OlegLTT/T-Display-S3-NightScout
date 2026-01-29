#pragma once
#include "lvgl.h"
#include "logs.h"

// Создаёт экран логов (lv_screen)
lv_obj_t* ui_logs_create();

// Подключить источник логов (глобальный/внешний буфер)
void ui_logs_set_buffer(const LogBuffer* logs);

// (опционально) Принудительно обновить прямо сейчас
void ui_logs_refresh();


void logs_update_full(LogBuffer& logs);   // или const LogBuffer& если так у тебя
