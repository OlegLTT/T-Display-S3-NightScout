// Временная надпись с выбранной яркостью поверх текущего экрана.
#include "ui_overlay.h"
#include <lvgl.h>
#include <cstdio>

static constexpr lv_coord_t OVERLAY_PAD = 8;

void BrightnessOverlay::init(lv_obj_t* parent) {
  if (_panel) return;

  if (!parent) parent = lv_layer_top();

  _panel = lv_obj_create(parent);
  lv_obj_clear_flag(_panel, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_set_size(_panel, 1, 1);

  lv_obj_set_style_border_width(_panel, 0, 0);
  lv_obj_set_style_radius(_panel, 10, 0);
  lv_obj_set_style_bg_opa(_panel, LV_OPA_50, 0);
  lv_obj_set_style_bg_color(_panel, lv_color_make(0, 0, 255), 0);
  lv_obj_set_style_pad_all(_panel, OVERLAY_PAD, 0);

  lv_obj_align(_panel, LV_ALIGN_CENTER, 0, 8);

  _label = lv_label_create(_panel);
  lv_obj_set_style_text_color(_label, lv_color_white(), 0);
  lv_obj_set_style_text_font(_label, &lv_font_montserrat_24, 0);

  lv_label_set_long_mode(_label, LV_LABEL_LONG_CLIP);
  lv_label_set_text(_label, "");

  lv_obj_align(_label, LV_ALIGN_CENTER, 0, 0);

  lv_obj_add_flag(_panel, LV_OBJ_FLAG_HIDDEN);
}

void BrightnessOverlay::show(uint8_t percent, uint32_t durationMs) {
  if (!_panel) init(nullptr);

  char buf[24];
  snprintf(buf, sizeof(buf), "%u%%", (unsigned)percent);
  lv_label_set_text(_label, buf);

  lv_obj_update_layout(_label);

  lv_coord_t lw = lv_obj_get_width(_label);
  lv_coord_t lh = lv_obj_get_height(_label);

  lv_obj_set_size(_panel, lw + OVERLAY_PAD * 2, lh + OVERLAY_PAD * 2);

  lv_obj_align(_label, LV_ALIGN_CENTER, 0, 0);

  lv_obj_clear_flag(_panel, LV_OBJ_FLAG_HIDDEN);

  if (_timer) {
    lv_timer_set_period(_timer, durationMs);
    lv_timer_reset(_timer);
    lv_timer_set_repeat_count(_timer, 1);
  } else {
    _timer = lv_timer_create(hide_timer_cb, durationMs, this);
    lv_timer_set_repeat_count(_timer, 1);
  }
}

void BrightnessOverlay::hide() {
  if (_panel) lv_obj_add_flag(_panel, LV_OBJ_FLAG_HIDDEN);

  if (_timer) {
    lv_timer_del(_timer);
    _timer = nullptr;
  }
}

void BrightnessOverlay::hide_timer_cb(lv_timer_t* t) {
  auto* self = static_cast<BrightnessOverlay*>(lv_timer_get_user_data(t));
  if (self) self->hide();
}
