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

#else
  #error "Не определена плата! Задай BOARD_TDISPLAY_S3 / BOARD_M5CORE_S3 в build_flags."
#endif
