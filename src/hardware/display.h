#pragma once
#include <cstdint>
#include <esp_lcd_panel_io.h>

namespace hardware {

class display {
public:
    using transfer_done_callback_t = void (*)(void*);

    enum class brightness_level : uint8_t {
        min = 0,
        max = 255,
    };

    static display& get();

    uint16_t width()  const;
    uint16_t height() const;

    void set_backlight(brightness_level level);

    void set_transfer_done_callback(transfer_done_callback_t cb, void* user_data);

    void set_bitmap(uint16_t x1, uint16_t x2, uint16_t y1, uint16_t y2, uint16_t* data);

private:
    display();
    ~display();

    display(const display&) = delete;
    display& operator=(const display&) = delete;

    static display s_instance;

    esp_lcd_i80_bus_handle_t   m_bus_handle   = nullptr;
    esp_lcd_panel_io_handle_t  m_io_handle    = nullptr;
    esp_lcd_panel_handle_t     m_panel_handle = nullptr;

    transfer_done_callback_t   m_on_transfer_done_callback = nullptr;
    void*                      m_on_transfer_done_user_data = nullptr;
};

} // namespace hardware
