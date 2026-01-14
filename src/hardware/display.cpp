#include "display.h"
#include <Arduino.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>
//#include <esp_lcd_i80_bus.h>

// ====== PINOUT (твоя T-Display S3 i80 8-bit) ======
#define PIN_TFT_WR    8
#define PIN_TFT_RD    9
#define PIN_TFT_RS    7

#define PIN_TFT_D0    39
#define PIN_TFT_D1    40
#define PIN_TFT_D2    41
#define PIN_TFT_D3    42
#define PIN_TFT_D4    45
#define PIN_TFT_D5    46
#define PIN_TFT_D6    47
#define PIN_TFT_D7    48

#define PIN_TFT_CS    6
#define PIN_TFT_RST   5
#define PIN_BACKLIGHT 38
// ================================================

// У тебя может быть 170x320 или уже повернуто 320x170.
// С учётом swap_xy(true) в референсе — обычно LVGL удобнее 320x170.
static constexpr uint16_t LCD_W = 320;
static constexpr uint16_t LCD_H = 170;

namespace hardware {

display display::s_instance;

display& display::get() { return s_instance; }

display::display() {
    // RD держим HIGH (если не читаем)
    pinMode(PIN_TFT_RD, OUTPUT);
    digitalWrite(PIN_TFT_RD, HIGH);

    // i80 bus
    esp_lcd_i80_bus_config_t bus_config = {};
    bus_config.dc_gpio_num = PIN_TFT_RS;
    bus_config.wr_gpio_num = PIN_TFT_WR;
    bus_config.clk_src = LCD_CLK_SRC_PLL160M;
    bus_config.data_gpio_nums[0] = PIN_TFT_D0;
    bus_config.data_gpio_nums[1] = PIN_TFT_D1;
    bus_config.data_gpio_nums[2] = PIN_TFT_D2;
    bus_config.data_gpio_nums[3] = PIN_TFT_D3;
    bus_config.data_gpio_nums[4] = PIN_TFT_D4;
    bus_config.data_gpio_nums[5] = PIN_TFT_D5;
    bus_config.data_gpio_nums[6] = PIN_TFT_D6;
    bus_config.data_gpio_nums[7] = PIN_TFT_D7;
    bus_config.bus_width = 8;
    bus_config.max_transfer_bytes = LCD_W * 40 * sizeof(uint16_t); // 40 строк буфера

    ESP_ERROR_CHECK(esp_lcd_new_i80_bus(&bus_config, &m_bus_handle));

    // transfer done callback -> прокидываем наружу
    auto on_transfer_done = [](esp_lcd_panel_io_handle_t, esp_lcd_panel_io_event_data_t*, void* user_ctx) -> bool {
        auto d = static_cast<display*>(user_ctx);
        if (d->m_on_transfer_done_callback) {
            d->m_on_transfer_done_callback(d->m_on_transfer_done_user_data);
        }
        return false;
    };

    esp_lcd_panel_io_i80_config_t io_config = {};
    io_config.cs_gpio_num = PIN_TFT_CS;
    io_config.pclk_hz = 20 * 1000 * 1000; // старт 20MHz
    io_config.trans_queue_depth = 10;
    io_config.on_color_trans_done = on_transfer_done;
    io_config.user_ctx = this;
    io_config.lcd_cmd_bits = 8;
    io_config.lcd_param_bits = 8;
    io_config.dc_levels.dc_idle_level = 0;
    io_config.dc_levels.dc_cmd_level  = 0;
    io_config.dc_levels.dc_dummy_level= 0;
    io_config.dc_levels.dc_data_level = 1;

    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i80(m_bus_handle, &io_config, &m_io_handle));

    // ST7789
    esp_lcd_panel_dev_config_t dev_config = {};
    dev_config.reset_gpio_num = PIN_TFT_RST;
    dev_config.color_space = ESP_LCD_COLOR_SPACE_RGB;
    dev_config.bits_per_pixel = 16;

    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(m_io_handle, &dev_config, &m_panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(m_panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(m_panel_handle));

    // Обычно для T-Display S3 нужно:
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(m_panel_handle, true));
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(m_panel_handle, true));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(m_panel_handle, false, true));
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(m_panel_handle, 0, 35));

    // Backlight
    pinMode(PIN_BACKLIGHT, OUTPUT);
    set_backlight(brightness_level::max);
}

display::~display() {}

uint16_t display::width() const  { return LCD_W; }
uint16_t display::height() const { return LCD_H; }

void display::set_backlight(brightness_level level) {
    analogWrite(PIN_BACKLIGHT, static_cast<int>(level));
}

void display::set_transfer_done_callback(transfer_done_callback_t cb, void* user_data) {
    m_on_transfer_done_callback = cb;
    m_on_transfer_done_user_data = user_data;
}

void display::set_bitmap(uint16_t x1, uint16_t x2, uint16_t y1, uint16_t y2, uint16_t* data) {
    // esp_lcd: x2,y2 — exclusive, поэтому +1
    esp_lcd_panel_draw_bitmap(m_panel_handle, x1, y1, x2 + 1, y2 + 1, data);
}

} // namespace hardware
