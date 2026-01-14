#pragma once

#include <LovyanGFX.hpp>
#include "board_config.h"   // тянем пины отсюда

// Класс дисплея для LilyGO T-Display S3
class LGFX_TDisplayS3 : public lgfx::LGFX_Device
{
  lgfx::Panel_ST7789   _panel;
  lgfx::Bus_Parallel8  _bus;
  lgfx::Light_PWM      _light;

public:
  LGFX_TDisplayS3(void)
  {
    { // Шина (8-битная параллельная)
      auto cfg = _bus.config();

      cfg.freq_write = 20000000;

      cfg.pin_wr = 8;
      cfg.pin_rd = 9;
      cfg.pin_rs = 7;

      cfg.pin_d0 = 39;
      cfg.pin_d1 = 40;
      cfg.pin_d2 = 41;
      cfg.pin_d3 = 42;
      cfg.pin_d4 = 45;
      cfg.pin_d5 = 46;
      cfg.pin_d6 = 47;
      cfg.pin_d7 = 48;

      _bus.config(cfg);
      _panel.setBus(&_bus);
    }
    { // Панель ST7789 170x320, окно 240x320
      auto cfg = _panel.config();

      cfg.pin_cs   = 6;
      cfg.pin_rst  = 5;
      cfg.pin_busy = -1;

      cfg.panel_width  = 170;
      cfg.panel_height = 320;
      cfg.offset_x     = 35;  // центрируем 170 в 240
      cfg.offset_y     = 0;
      cfg.offset_rotation = 0;

      cfg.dummy_read_pixel = 8;
      cfg.dummy_read_bits  = 1;
      cfg.readable         = false;
      cfg.invert           = true;
      cfg.rgb_order        = false;
      cfg.dlen_16bit       = false;
      cfg.bus_shared       = true;

      _panel.config(cfg);
    }
    { // Подсветка (PWM)
      auto cfg = _light.config();

      cfg.pin_bl      = PIN_BACKLIGHT;     // твой BL пин
      cfg.invert      = false;
      cfg.freq        = 44100;
      cfg.pwm_channel = 0;

      _light.config(cfg);
      _panel.setLight(&_light);
    }

    setPanel(&_panel);
  }
};
