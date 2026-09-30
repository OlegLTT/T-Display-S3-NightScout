// Связь LVGL с драйвером платы: буферы, завершение передачи, сенсор и таймер интерфейса.
#pragma once
#include <lvgl.h>

namespace gfx {

class LVGL_Display {
public:
    static LVGL_Display& instance();

    void init();
    void loop();

private:
    LVGL_Display() = default;

    void init_lvgl();
    void init_buffers();
    void init_flush();
    void init_touch();

    static LVGL_Display s_instance;

    lv_display_t* m_disp = nullptr;
    lv_indev_t* m_touch = nullptr;
    void* m_buf1 = nullptr;
    void* m_buf2 = nullptr;
};

}
