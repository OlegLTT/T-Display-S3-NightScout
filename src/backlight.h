#pragma once
#include <Arduino.h>
#include "board_config.h"

struct BacklightConfig {
  int pin = PIN_BACKLIGHT;
  int channel = 7;
  int freq = 12000;
  int resBits = 10;
};

class Backlight {
public:
  // ===== Instance API (нормальный) =====
  void setLevels(const uint8_t* levels, size_t count);
  void begin();
  void begin(const BacklightConfig& cfg);
  void setPercent(uint8_t percent);
  void next();
  uint8_t currentPercent() const;

  // ===== Static API (как раньше) =====
  static void Use(Backlight& instance);   // зарегистрировать "глобальный" экземпляр
  static void beginStatic();              // begin() через зарегистрированный экземпляр
  static void nextStatic();               // next()  через зарегистрированный экземпляр
  static uint8_t currentPercentStatic();  // currentPercent() через зарегистрированный экземпляр
  static void setPercentStatic(uint8_t p);

  void restoreFromPercent(uint8_t percent);  // подобрать ближайший уровень и выставить

private:
  void backlightInit();
  void backlightForceMax();

  BacklightConfig _cfg{};
  uint32_t _maxDuty = 1023;

  const uint8_t* _levels = nullptr;
  size_t _count = 0;
  size_t _idx = 0;

  static Backlight* _self;
};
