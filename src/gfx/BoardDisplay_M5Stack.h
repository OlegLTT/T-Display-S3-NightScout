// Общий драйвер M5Stack Core и CoreS3: дисплей, динамик и кнопки через M5Unified.
#pragma once

#include "board_config.h"

#if defined(BOARD_USES_M5UNIFIED)

#include <cstdint>

namespace gfx {

class BoardDisplay_M5Stack {
public:
    using transfer_done_callback_t = void (*)(void*);
    enum class brightness_level : uint8_t { min = 0, max = 255 };
    enum class button_event : uint8_t { none, click, double_click, hold };
    enum class button_id : uint8_t { power, a, b, c, a_c };

    static BoardDisplay_M5Stack& instance();

    uint16_t width() const;
    uint16_t height() const;

    void set_backlight(brightness_level level);
    void set_alarm_sound(bool active, bool critical = false);
    void set_alarm_volume(uint8_t percent);
    bool alarm_sound_available() const { return m_sound_available; }
    void set_transfer_done_callback(transfer_done_callback_t cb, void* user_data);
    void draw_bitmap(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t* data);

    void update();
    bool get_touch(uint16_t& x, uint16_t& y) const;

    button_event take_button_event(button_id id = button_id::power);
    bool take_button_press();
    void reset_button_events();

private:
    BoardDisplay_M5Stack();
    ~BoardDisplay_M5Stack() = default;

    BoardDisplay_M5Stack(const BoardDisplay_M5Stack&) = delete;
    BoardDisplay_M5Stack& operator=(const BoardDisplay_M5Stack&) = delete;

    void update_alarm_sound(uint32_t now);
    bool m_sound_available = false;
    bool m_alarm_active = false;
    bool m_critical_alarm = false;
    uint8_t m_beep_index = 0;
    uint32_t m_last_beep = 0;

    transfer_done_callback_t m_on_transfer_done_callback = nullptr;
    void* m_on_transfer_done_user_data = nullptr;

    button_event m_button_events[5] = {};
#if defined(BOARD_M5STACK)
    bool m_buttons_ac_active = false;
    bool m_buttons_ac_held = false;
#endif
    bool m_button_pressed = false;
    bool m_button_click_pending = false;
    uint32_t m_button_clicked_at = 0;
};

}

#endif
