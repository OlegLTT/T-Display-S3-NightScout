#include "LVGL_Display.h"
#include "gfx/Display.h"     
#include <esp_timer.h>
#include <esp_heap_caps.h>

namespace gfx {

LVGL_Display LVGL_Display::s_instance;

LVGL_Display& LVGL_Display::instance() { return s_instance; }

void LVGL_Display::init() {
    init_lvgl();

    auto& hw = Display::instance();            // <-- теперь это alias
    m_disp = lv_display_create(hw.width(), hw.height());

    init_buffers();
    init_flush();
}

void LVGL_Display::init_lvgl() {
    lv_init();
    lv_tick_set_cb([]() -> uint32_t {
        return esp_timer_get_time() / 1000ULL;
    });
}

void LVGL_Display::init_buffers() {
    auto& hw = Display::instance();

    constexpr int LINES = 16;
    const uint32_t bytes = hw.width() * LINES * (LV_COLOR_DEPTH / 8);

    m_buf1 = heap_caps_malloc(bytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    m_buf2 = heap_caps_malloc(bytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);

    lv_display_set_buffers(m_disp, m_buf1, m_buf2, bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
}

void LVGL_Display::init_flush() {
    auto& hw = Display::instance();

    hw.set_transfer_done_callback([](void* user) {
        lv_display_flush_ready((lv_display_t*)user);
    }, m_disp);

    lv_display_set_user_data(m_disp, &hw);

    lv_display_set_flush_cb(m_disp,
        [](lv_display_t* disp, const lv_area_t* a, uint8_t* px) {
            lv_draw_sw_rgb565_swap(px,
                (a->x2 - a->x1 + 1) * (a->y2 - a->y1 + 1));

            auto* hw = static_cast<Display*>(lv_display_get_user_data(disp));
            hw->draw_bitmap(a->x1, a->y1, a->x2, a->y2, (uint16_t*)px);
        }
    );
}

void LVGL_Display::loop() {
    lv_timer_handler();
}

} // namespace gfx
