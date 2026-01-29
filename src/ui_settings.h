#pragma once
#include <lvgl.h>

class CaptivePortal; // forward decl

lv_obj_t* ui_settings_create();
void ui_settings_update_from_portal(const CaptivePortal& portal);  // дергай, когда portal.active()
