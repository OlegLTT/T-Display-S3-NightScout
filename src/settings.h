// Параметры устройства и их загрузка, сохранение и сброс в пространстве NVS appcfg.
#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include "glucose_zone.h"

static constexpr int MAX_WIFI = 5;

struct WifiSlot {
  char ssid[33] = {0};
  char pass[65] = {0};
};

struct AppSettings {
  WifiSlot wifi[MAX_WIFI];

  // Смещение относительно UTC в минутах; положительное значение соответствует востоку.
  int16_t tzMinutes = 0;
  bool    dstEnable = false;

  // Четыре возрастающие границы глюкозы в ммоль/л.
  float sgvBounds[4] = {3.9f, 4.5f, 8.5f, 10.0f};

  char hostSite[96] = {0};
  char nsToken[96] = {0};

  bool onboardingDone = false;
  bool soundAlarmEnabled = false;

  // Громкость в процентах: 0 отключает звук, 100 задаёт максимум.
  uint8_t alarmVolume = 31;

  bool isValidBounds() const {
    return valid_glucose_bounds(sgvBounds);
  }

  uint8_t lastUiScreen = 0;

  uint8_t brightness = 100;

};

class SettingsStore {
public:
  bool begin();
  void end();

  bool load(AppSettings& out);
  bool save(const AppSettings& s);

  bool factoryReset(AppSettings& out);

private:
  Preferences _prefs;
};

extern AppSettings appCfg;
