// Дисплей, динамик, сенсор и кнопка питания CoreS3 через M5Unified.
#pragma once

#include "board_config.h"

#if BOARD_M5CORE_S3

#include <cstdint>

namespace gfx {

class BoardDisplay_CoreS3 {
public:
    using transfer_done_callback_t = void (*)(void*);
    enum class brightness_level : uint8_t { min = 0, max = 255 };
    enum class button_event : uint8_t { none, click, double_click, hold };

    static BoardDisplay_CoreS3& instance();

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

    button_event take_button_event();
    void reset_button_events();

private:
    BoardDisplay_CoreS3();
    ~BoardDisplay_CoreS3() = default;

    BoardDisplay_CoreS3(const BoardDisplay_CoreS3&) = delete;
    BoardDisplay_CoreS3& operator=(const BoardDisplay_CoreS3&) = delete;

    void update_alarm_sound(uint32_t now);
    bool m_sound_available = false;
    bool m_alarm_active = false;
    bool m_critical_alarm = false;
    uint8_t m_beep_index = 0;
    uint32_t m_last_beep = 0;

    transfer_done_callback_t m_on_transfer_done_callback = nullptr;
    void* m_on_transfer_done_user_data = nullptr;

    button_event m_button_event = button_event::none;
    bool m_button_click_pending = false;
    uint32_t m_button_clicked_at = 0;
};

}

#endif
