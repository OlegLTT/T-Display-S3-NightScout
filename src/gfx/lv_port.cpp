#include <Arduino.h>
#include <lvgl.h>

extern "C" {
  #include "esp_lcd_panel_io.h"
  #include "esp_lcd_panel_ops.h"
  #include "esp_lcd_panel_vendor.h"
  #include "esp_lcd_panel_interface.h"
  #include "esp_lcd_panel_io_interface.h"
 // #include "esp_lcd_i80_bus.h"

  #include "esp_timer.h"
  #include "esp_heap_caps.h"
}

// ====== ТВОИ ПИНЫ ======
#define PIN_TFT_WR    8
#define PIN_TFT_RD    9
#define PIN_TFT_RS    7   // DC/RS
#define PIN_TFT_CS    6
#define PIN_TFT_RST   5

#define PIN_TFT_D0    39
#define PIN_TFT_D1    40
#define PIN_TFT_D2    41
#define PIN_TFT_D3    42
#define PIN_TFT_D4    45
#define PIN_TFT_D5    46
#define PIN_TFT_D6    47
#define PIN_TFT_D7    48

#define PIN_BACKLIGHT 38
// ========================

// Разрешение T-Display S3 (обычно 170x320)
static constexpr int LCD_HOR_RES = 170;
static constexpr int LCD_VER_RES = 320;

static esp_lcd_panel_handle_t s_panel = nullptr;
static lv_display_t* s_disp = nullptr;

// Буферы LVGL (частичный рендер: 40 строк)
static void* s_buf1 = nullptr;
static void* s_buf2 = nullptr;

// lvgl tick 1ms
static void lv_tick_cb(void*) { lv_tick_inc(1); }

// DMA done callback -> можно (если хочешь) вызывать flush_ready строго после завершения.
// На Arduino-сборках иногда проще делать flush_ready сразу после draw_bitmap.
// Оставляю вариант "после draw_bitmap" (надёжнее для старта).
static bool on_color_trans_done(esp_lcd_panel_io_handle_t, esp_lcd_panel_io_event_data_t*, void*)
{
  // Если захочешь перевести на "flush_ready по DMA done":
  // lv_display_flush_ready(s_disp);
  return false;
}

// Flush callback LVGL 9
static void lvgl_flush_cb(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map)
{
  const int x1 = area->x1;
  const int y1 = area->y1;
  const int x2 = area->x2 + 1;   // esp_lcd: end is exclusive
  const int y2 = area->y2 + 1;

  esp_lcd_panel_draw_bitmap(s_panel, x1, y1, x2, y2, px_map);

  // Для старта: считаем flush завершённым сразу (работает стабильно).
  // Если захочешь максимум производительности — перенесём flush_ready в on_color_trans_done.
  lv_display_flush_ready(disp);
}

static void backlight_init()
{
  pinMode(PIN_BACKLIGHT, OUTPUT);
  digitalWrite(PIN_BACKLIGHT, HIGH); // или твой PWM-класс Backlight
}

void lv_port_init()
{
  lv_init();
  backlight_init();

  // 1) Создаём I80 bus
  esp_lcd_i80_bus_handle_t i80_bus = nullptr;

  esp_lcd_i80_bus_config_t bus_cfg = {};
  bus_cfg.clk_src = LCD_CLK_SRC_PLL160M;
  bus_cfg.dc_gpio_num = PIN_TFT_RS;   // RS/DC
  bus_cfg.wr_gpio_num = PIN_TFT_WR;   // WR
  bus_cfg.data_gpio_nums[0] = PIN_TFT_D0;
  bus_cfg.data_gpio_nums[1] = PIN_TFT_D1;
  bus_cfg.data_gpio_nums[2] = PIN_TFT_D2;
  bus_cfg.data_gpio_nums[3] = PIN_TFT_D3;
  bus_cfg.data_gpio_nums[4] = PIN_TFT_D4;
  bus_cfg.data_gpio_nums[5] = PIN_TFT_D5;
  bus_cfg.data_gpio_nums[6] = PIN_TFT_D6;
  bus_cfg.data_gpio_nums[7] = PIN_TFT_D7;
  bus_cfg.bus_width = 8;

  // максимум байт за раз (буфер LVGL может быть больше/меньше — это не критично)
  bus_cfg.max_transfer_bytes = LCD_HOR_RES * 40 * 2; // 40 строк RGB565

  ESP_ERROR_CHECK(esp_lcd_new_i80_bus(&bus_cfg, &i80_bus));

  // 2) Panel IO для I80
  esp_lcd_panel_io_handle_t io_handle = nullptr;

  esp_lcd_panel_io_i80_config_t io_cfg = {};
  io_cfg.cs_gpio_num = PIN_TFT_CS;
  io_cfg.pclk_hz = 20000000;                // 20 MHz стартово, можно пробовать 30–40
  io_cfg.trans_queue_depth = 10;
  io_cfg.on_color_trans_done = on_color_trans_done;
  io_cfg.user_ctx = nullptr;
  io_cfg.lcd_cmd_bits = 8;
  io_cfg.lcd_param_bits = 8;

  ESP_ERROR_CHECK(esp_lcd_new_panel_io_i80(i80_bus, &io_cfg, &io_handle));

  // 3) ST7789 panel
  esp_lcd_panel_dev_config_t panel_cfg = {};
  panel_cfg.reset_gpio_num = PIN_TFT_RST;
  panel_cfg.color_space = ESP_LCD_COLOR_SPACE_RGB; // если будут swapped цвета — поменяем ниже
  panel_cfg.bits_per_pixel = 16;

  ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_cfg, &s_panel));
  ESP_ERROR_CHECK(esp_lcd_panel_reset(s_panel));
  ESP_ERROR_CHECK(esp_lcd_panel_init(s_panel));
  ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(s_panel, true));

  // 4) LVGL display (v9)
  s_disp = lv_display_create(LCD_HOR_RES, LCD_VER_RES);
  lv_display_set_flush_cb(s_disp, lvgl_flush_cb);

  // Буферы LVGL: 40 строк
  const uint32_t buf_pixels = LCD_HOR_RES * 40;
  const uint32_t buf_bytes  = buf_pixels * 2; // RGB565

  s_buf1 = heap_caps_malloc(buf_bytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  s_buf2 = heap_caps_malloc(buf_bytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);

  lv_display_set_buffers(
    s_disp,
    s_buf1,
    s_buf2,
    buf_bytes,
    LV_DISPLAY_RENDER_MODE_PARTIAL
  );

  // 5) LVGL tick timer 1ms
  const esp_timer_create_args_t targs = {
    .callback = &lv_tick_cb,
    .arg = nullptr,
    .dispatch_method = ESP_TIMER_TASK,
    .name = "lv_tick"
  };
  esp_timer_handle_t tick_timer;
  ESP_ERROR_CHECK(esp_timer_create(&targs, &tick_timer));
  ESP_ERROR_CHECK(esp_timer_start_periodic(tick_timer, 1000));
}

void lv_port_loop()
{
  lv_timer_handler();
}
