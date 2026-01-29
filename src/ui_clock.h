#pragma once
#include <lvgl.h>
#include "nightscout_client.h"

lv_obj_t* ui_clock_create();
lv_obj_t* ui_clock_screen();

// Полный апдейт "как старый drawClockScreen(sprite, nsData)"
void clock_update_full(const NightscoutData& ns);
