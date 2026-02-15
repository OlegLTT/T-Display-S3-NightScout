#pragma once
#include <lvgl.h>
#include "nightscout_client.h"

lv_obj_t* ui_clock_create();

void clock_update_full(const NightscoutData& ns);

