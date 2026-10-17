// Дисплей ST7789 по шине i80 и пассивный buzzer T-Display S3 на GPIO13.
#include "../app_constants.h"
#include "board_config.h"

#if defined(BOARD_TDISPLAY_S3)
#include "BoardDisplay_TDisplayS3.h"
#include "../backlight.h"
#include "../buzzer.h"
#include <Arduino.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>

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

static constexpr uint16_t LCD_W = 320;
static constexpr uint16_t LCD_H = 170;

namespace gfx {

void BoardDisplay_TDisplayS3::set_alarm_volume(uint8_t percent) {
    m_alarm_volume = percent > 100 ? 100 : percent;
    buzzer::setVolume(m_alarm_volume);
}

void BoardDisplay_TDisplayS3::set_alarm_sound(bool active) {
    buzzer::setAlarm(active && m_alarm_volume > 0);
}

bool BoardDisplay_TDisplayS3::alarm_sound_available() const { return buzzer::available(); }

// Драйвер создаётся из setup(), после инициализации GPIO-зуммера.
BoardDisplay_TDisplayS3& BoardDisplay_TDisplayS3::instance() {
    static BoardDisplay_TDisplayS3 display;
    return display;
}

BoardDisplay_TDisplayS3 ::BoardDisplay_TDisplayS3 () {

    pinMode(PIN_TFT_RD, OUTPUT);
    digitalWrite(PIN_TFT_RD, HIGH);

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
    bus_config.max_transfer_bytes = LCD_W * 40 * sizeof(uint16_t);

    ESP_ERROR_CHECK(esp_lcd_new_i80_bus(&bus_config, &m_bus_handle));

    auto on_transfer_done = [](esp_lcd_panel_io_handle_t, esp_lcd_panel_io_event_data_t*, void* user_ctx) -> bool {
        auto d = static_cast<BoardDisplay_TDisplayS3 *>(user_ctx);
        if (d->m_on_transfer_done_callback) {
            d->m_on_transfer_done_callback(d->m_on_transfer_done_user_data);
        }
        return false;
    };

    esp_lcd_panel_io_i80_config_t io_config = {};
    io_config.cs_gpio_num = PIN_TFT_CS;
    io_config.pclk_hz = 20 * 1000 * 1000;
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

    esp_lcd_panel_dev_config_t dev_config = {};
    dev_config.reset_gpio_num = PIN_TFT_RST;
    dev_config.color_space = ESP_LCD_COLOR_SPACE_RGB;
    dev_config.bits_per_pixel = 16;

    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(m_io_handle, &dev_config, &m_panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(m_panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(m_panel_handle));

    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(m_panel_handle, true));
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(m_panel_handle, true));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(m_panel_handle, false, true));
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(m_panel_handle, 0, 35));

    // До Backlight::begin() подсветка включена обычным GPIO, без analogWrite().
    digitalWrite(PIN_BACKLIGHT, HIGH);
    pinMode(PIN_BACKLIGHT, OUTPUT);
}

BoardDisplay_TDisplayS3 ::~BoardDisplay_TDisplayS3 () {}

uint16_t BoardDisplay_TDisplayS3 ::width() const  { return LCD_W; }
uint16_t BoardDisplay_TDisplayS3 ::height() const { return LCD_H; }

void BoardDisplay_TDisplayS3 ::set_backlight(brightness_level level) {
    Backlight::setPercentStatic((uint16_t(level) * 100 + 254) / 255);
}

void BoardDisplay_TDisplayS3 ::set_transfer_done_callback(transfer_done_callback_t cb, void* user_data) {
    m_on_transfer_done_callback = cb;
    m_on_transfer_done_user_data = user_data;
}

void BoardDisplay_TDisplayS3::draw_bitmap(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t* data) {
    esp_lcd_panel_draw_bitmap(m_panel_handle, x1, y1, x2 + 1, y2 + 1, data);
}

}

#endif
