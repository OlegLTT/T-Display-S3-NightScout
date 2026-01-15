#pragma once
#include <lvgl.h>

namespace gfx {

class LVGL_Display {
public:
    static LVGL_Display& instance();

    void init();     // lv_init + buffers + flush
    void loop();     // lv_timer_handler

private:
    LVGL_Display() = default;

    void init_lvgl();
    void init_buffers();
    void init_flush();

    static LVGL_Display s_instance;

    lv_display_t* m_disp = nullptr;
    void* m_buf1 = nullptr;
    void* m_buf2 = nullptr;
};

} // namespace gfx
