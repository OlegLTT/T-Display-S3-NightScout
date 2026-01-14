#include <Arduino.h>
#include <lvgl.h>
#include <esp_timer.h>
#include "hardware/display.h"
#include "esp_heap_caps.h"

static lv_display_t* g_disp = nullptr;
static void* g_buf1 = nullptr;
static void* g_buf2 = nullptr;

static void ui_show_main()
{
    lv_obj_t* scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    lv_obj_t* label = lv_label_create(scr);
    lv_label_set_text(label, "HELLO MAIN SCREEN");
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_center(label);
}

static void lvgl_init_with_display()
{
    lv_init();

    // tick (LVGL 9)
    lv_tick_set_cb([]() -> uint32_t {
        return (uint32_t)(esp_timer_get_time() / 1000ULL);
    });

    auto& disp_hw = hardware::display::get();

    // буфер: 16 строк (как в референсе) — можно 40, если RAM позволяет
    const uint32_t lines = 16;
    const uint32_t buf_pixels = disp_hw.width() * lines;
    const uint32_t buf_bytes  = buf_pixels * (LV_COLOR_DEPTH / 8);

    g_buf1 = heap_caps_malloc(buf_bytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    g_buf2 = heap_caps_malloc(buf_bytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);

    g_disp = lv_display_create(disp_hw.width(), disp_hw.height());
    lv_display_set_buffers(g_disp, g_buf1, g_buf2, buf_bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);

    // по окончанию передачи -> LVGL flush_ready
    disp_hw.set_transfer_done_callback([](void* user_data) {
        lv_display_flush_ready((lv_display_t*)user_data);
    }, g_disp);

    // flush_cb
    lv_display_set_user_data(g_disp, &disp_hw);
    lv_display_set_flush_cb(g_disp, [](lv_display_t* d, const lv_area_t* a, uint8_t* px) {
        // если у тебя “байты местами”, оставь swap:
        lv_draw_sw_rgb565_swap(px, (a->x2 - a->x1 + 1) * (a->y2 - a->y1 + 1));

        auto hw = (hardware::display*)lv_display_get_user_data(d);
        hw->set_bitmap(a->x1, a->x2, a->y1, a->y2, (uint16_t*)px);
    });
}

void setup()
{
    delay(200);

    lvgl_init_with_display();
    ui_show_main();
}

void loop()
{
    // LVGL сам скажет, когда ему надо снова
    delay(lv_timer_handler());
}
