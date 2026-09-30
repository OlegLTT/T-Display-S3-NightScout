// Экран часов: время, дата, глюкоза и тренд; цвет учитывает возраст данных.
#pragma once
#include <lvgl.h>
#include "nightscout_client.h"

lv_obj_t* ui_clock_create();

void clock_update_full(const NightscoutData& ns);

