#pragma once

// Определяем, для какой платы собрали прошивку
#if defined(BOARD_TDISPLAY_S3)

// ------- T-Display S3 -------
  #define BOARD_NAME        "T-Display S3"

  // Пины подсветки, TFT, кнопок и т.п.
  #define PIN_BACKLIGHT     38
  #define PIN_BUTTON_A      0
  #define PIN_BUTTON_B      14
  #define PIN_BUZZER        13
  // ... остальные пины T-Display

#elif defined(BOARD_M5CORE_S3)

// ------- M5CoreS3 -------
  #define BOARD_NAME        "M5CoreS3"

  // Подсветкой управляет M5Unified через контроллер питания, не GPIO.
  #define PIN_BACKLIGHT     -1

#elif defined(BOARD_ESP32S3_DEV)
// Waveshare ESP32-S3 1.43" AMOLED (ESP32-S3-Touch-AMOLED-1.43)

#define BOARD_NAME          "ESP32-S3 1.43 AMOLED"

// -------- ДИСПЛЕЙ AMOLED (QSPI) --------
// По вики:
// GPIO9  - QSPI_CS
// GPIO10 - QSPI_CLK
// GPIO11 - QSPI_D0
// GPIO12 - QSPI_D1
// GPIO13 - QSPI_D2
// GPIO14 - QSPI_D3
// GPIO21 - AMOLED_RST
// GPIO42 - AMOLED_EN (питание / подсветка)
#define PIN_TFT_CS          9      // QSPI_CS
#define PIN_TFT_SCK         10     // QSPI_CLK (если драйверу нужен)
#define PIN_TFT_D0          11
#define PIN_TFT_D1          12
#define PIN_TFT_D2          13
#define PIN_TFT_D3          14
#define PIN_TFT_RST         21
#define PIN_BACKLIGHT       42     // AMOLED_EN – можно использовать как backlight/power

#define TFT_WIDTH           466
#define TFT_HEIGHT          466

// -------- ТАЧ FT3168 (I2C) --------
// GPIO47 - TP_SDA
// GPIO48 - TP_SCL
#define PIN_TOUCH_SDA       47
#define PIN_TOUCH_SCL       48
// INT у тача в таблице не указан – нужно будет посмотреть в схеме.
// Пока можно обойтись без прерывания и опрашивать по I2C.

// -------- RTC PCF85063 --------
// GPIO15 - RTC_INT
#define PIN_RTC_INT         15

// -------- IMU (QMI8658) --------
// GPIO8 - IMU_INT
#define PIN_IMU_INT         8

// -------- SD-карта --------
// GPIO38 - SD_CS
// GPIO39 - SD_MOSI
// GPIO40 - SD_MISO
// GPIO41 - SD_SCLK
#define PIN_SD_CS           38
#define PIN_SD_MOSI         39
#define PIN_SD_MISO         40
#define PIN_SD_SCK          41

// -------- Аккум / питание --------
// GPIO4 - BAT_ADC
#define PIN_BAT_ADC         4

// -------- UART для отладки --------
// GPIO43 - TXD, GPIO44 - RXD
#define PIN_UART_TX         43
#define PIN_UART_RX         44

// BOOT кнопка: GPIO0
#define PIN_KEY_BOOT        0

#define PIN_I2C_SDA 47
#define PIN_I2C_SCL 48

#else
  #error "Не определена плата! Задай BOARD_TDISPLAY_S3 / BOARD_M5CORE_S3 / BOARD_ESP32S3_DEV в build_flags."
#endif
