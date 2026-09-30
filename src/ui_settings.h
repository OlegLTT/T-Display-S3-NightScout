// Экран подключения к веб-настройкам: реквизиты Wi-Fi и QR-коды сети и адреса портала.
#pragma once
#include <lvgl.h>

class CaptivePortal;

lv_obj_t* ui_settings_create();
void ui_settings_update_from_portal(const CaptivePortal& portal);
