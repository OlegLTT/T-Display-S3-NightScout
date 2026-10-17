// Временная надпись с выбранной яркостью поверх текущего экрана.
#pragma once
#include "app_constants.h"
#include <lvgl.h>
#include <stdint.h>

class BrightnessOverlay {
public:
  void init(lv_obj_t* parent = nullptr);
  void show(uint8_t percent, uint32_t durationMs = app_constants::brightness_overlay_ms);
  void hide();

private:
  static void hide_timer_cb(lv_timer_t* t);

  lv_obj_t*   _panel = nullptr;
  lv_obj_t*   _label = nullptr;
  lv_timer_t* _timer = nullptr;
};
