#include <Arduino.h>
#include "gfx/LVGL_Display.h"
#include "ui_main.h"
#include "ui_clock.h"

static lv_obj_t* g_scr_main = nullptr;
static lv_obj_t* g_scr_clock = nullptr;
static bool g_show_main = true;

static void switch_screen_cb(lv_timer_t*)
{
    g_show_main = !g_show_main;

    // Анимация: слайд влево/вправо
    if (g_show_main) {
        lv_screen_load_anim(g_scr_main, LV_SCREEN_LOAD_ANIM_OVER_BOTTOM, 300, 0, false);
    } else {
        lv_screen_load_anim(g_scr_clock, LV_SCR_LOAD_ANIM_MOVE_LEFT, 300, 0, false);
    }
}

void setup()
{
    delay(200);
    gfx::LVGL_Display::instance().init();

    // Создаём главный экран UI
    g_scr_main = ui_main_create();
    g_scr_clock = ui_clock_create();

    // каждые 5 секунд переключаем
   // lv_timer_create(switch_screen_cb, 3000, nullptr);
}

void loop()
{
    gfx::LVGL_Display::instance().loop();
    delay(5);
}
