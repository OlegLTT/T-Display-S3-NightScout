// Основной экран: глюкоза, тренд, время, возраст данных и заряд загрузчика.
#pragma once
#include <lvgl.h>
#include "nightscout_data.h"

lv_obj_t* ui_main_create();

void main_update_full(const NightscoutData& ns);
