#include <Arduino.h>
#include <Arduino.h>
#include "gfx/LVGL_Display.h"
#include "ui_main.h"


void setup()
{
    delay(200);
    gfx::LVGL_Display::instance().init();

    // Создаём главный экран UI
    ui_main_create();
}

void loop()
{
    gfx::LVGL_Display::instance().loop();
    delay(5);
}
