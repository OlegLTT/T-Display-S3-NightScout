#pragma once
#include <lvgl.h>
#include "nightscout_client.h"

// Экран MAIN
lv_obj_t* ui_main_create();

// "Как старый drawMainScreen(sprite, nsData)" — полный апдейт виджетов
void main_update_full(const NightscoutData& ns);

// // (опционально) доступ к экрану, если нужно переключать
// lv_obj_t* ui_main_screen();
