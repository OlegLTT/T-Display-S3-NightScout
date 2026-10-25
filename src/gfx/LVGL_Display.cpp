// Связь LVGL с драйвером платы: буферы, завершение передачи, сенсор и таймер интерфейса.
#include "LVGL_Display.h"
#include "gfx/Display.h"
#include "ui_navigation.h"
#include <Arduino.h>
#include <esp_timer.h>
#include <esp_heap_caps.h>

namespace gfx {

LVGL_Display LVGL_Display::s_instance;

LVGL_Display& LVGL_Display::instance() { return s_instance; }

void LVGL_Display::init() {
    init_lvgl();

    auto& hw = Display::instance();
    m_disp = lv_display_create(hw.width(), hw.height());
    lv_display_set_color_format(m_disp, LV_COLOR_FORMAT_RGB565);

    init_buffers();
    init_flush();
    init_touch();
}

void LVGL_Display::init_lvgl() {
    lv_init();
#if defined(BOARD_TDISPLAY_S3) && LV_USE_STDLIB_MALLOC == LV_STDLIB_BUILTIN
    // Сжатие «10.0» и более широких значений создаёт слой ARGB8888 > 64 КБ.
    // Отдельный пул PSRAM сохраняет крупный шрифт и память DMA для передачи на LCD.
    constexpr size_t pool_bytes = LV_MEM_POOL_EXPAND_SIZE;
    static_assert(pool_bytes >= 256 * 1024, "LVGL must allow the large display layers");
    void* pool = heap_caps_malloc(pool_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!pool) {
        Serial.println("[UI] Failed to allocate LVGL PSRAM pool");
    } else if (!lv_mem_add_pool(pool, pool_bytes)) {
        heap_caps_free(pool);
        Serial.println("[UI] Failed to add LVGL PSRAM pool");
    } else {
        Serial.printf("[UI] LVGL PSRAM pool ready: %u bytes\n", unsigned(pool_bytes));
    }
#endif
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

// Готовность буфера сообщается LVGL после завершения передачи драйвером дисплея.
void LVGL_Display::init_flush() {
    auto& hw = Display::instance();

    hw.set_transfer_done_callback([](void* user) {
        lv_display_flush_ready((lv_display_t*)user);
    }, m_disp);

    lv_display_set_user_data(m_disp, &hw);

    lv_display_set_flush_cb(m_disp,
        [](lv_display_t* disp, const lv_area_t* a, uint8_t* px) {
            #if defined(BOARD_TDISPLAY_S3)
                lv_draw_sw_rgb565_swap(px,
                (a->x2 - a->x1 + 1) * (a->y2 - a->y1 + 1));
            #endif

            auto* hw = static_cast<Display*>(lv_display_get_user_data(disp));
            hw->draw_bitmap(a->x1, a->y1, a->x2, a->y2, (uint16_t*)px);
        }
    );
}

void LVGL_Display::init_touch() {
#if defined(BOARD_M5CORE_S3)
    m_touch = lv_indev_create();
    lv_indev_set_type(m_touch, LV_INDEV_TYPE_POINTER);
    lv_indev_set_display(m_touch, m_disp);
    lv_indev_set_read_cb(m_touch, [](lv_indev_t*, lv_indev_data_t* data) {
        static lv_point_t last_point = {0, 0};
        uint16_t x = 0, y = 0;
        const bool pressed = Display::instance().get_touch(x, y);
        if (pressed) {
            last_point.x = x;
            last_point.y = y;
        }
        data->point = last_point;
        data->state = pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
        ui_navigation::feed_touch(pressed, last_point.x, last_point.y);
    });
#endif
}

// На CoreS3 сначала обновляются кнопки и звук, затем выполняются таймеры LVGL.
void LVGL_Display::loop() {
#if defined(BOARD_M5CORE_S3)
    Display::instance().update();
#endif
    lv_timer_handler();
}

}
