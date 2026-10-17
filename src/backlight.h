// Выбранная яркость хранится отдельно от временного максимума тревоги и настроек.
#pragma once
#include "app_constants.h"
#include <Arduino.h>
#include "board_config.h"

struct BacklightConfig {
  int pin = PIN_BACKLIGHT;
  int channel = app_constants::backlight_channel;
  int freq = app_constants::backlight_pwm_hz;
  int resBits = app_constants::backlight_resolution_bits;
};

class Backlight {
public:

  void setLevels(const uint8_t* levels, size_t count);
  void begin();
  void begin(const BacklightConfig& cfg);
  void setPercent(uint8_t percent);
  void setAlarmBoost(bool active);
  void setSettingsBoost(bool active);
  void next();
  uint8_t currentPercent() const;

  static void Use(Backlight& instance);
  static void beginStatic();
  static void nextStatic();
  static uint8_t currentPercentStatic();
  static void setPercentStatic(uint8_t p);

  void restoreFromPercent(uint8_t percent);

private:
  void applyPercent(uint8_t percent);
  uint8_t _requestedPercent = app_constants::default_brightness_percent;
  bool _alarmBoost = false;
  bool _settingsBoost = false;
  bool _pwmAttached = false;
  void backlightInit();
  void backlightForceMax();

  BacklightConfig _cfg{};
  uint32_t _maxDuty = 1023;

  const uint8_t* _levels = nullptr;
  size_t _count = 0;
  size_t _idx = 0;

  static Backlight* _self;
};
