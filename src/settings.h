// Параметры устройства и их загрузка, сохранение и сброс в пространстве NVS appcfg.
#pragma once
#include "app_constants.h"
#include <Arduino.h>
#include <Preferences.h>
#include "glucose_zone.h"
#include "nightscout_alarm_state.h"

static constexpr int MAX_WIFI = app_constants::wifi_slots;

struct WifiSlot {
  char ssid[33] = {0};
  char pass[65] = {0};
};

struct AppSettings {
  WifiSlot wifi[MAX_WIFI];

  // Смещение относительно UTC в минутах; положительное значение соответствует востоку.
  int16_t tzMinutes = app_constants::default_timezone_minutes;
  bool    dstEnable = app_constants::default_dst;

  // Четыре возрастающие границы глюкозы в ммоль/л.
  float sgvBounds[4] = {app_constants::default_glucose_bounds[0], app_constants::default_glucose_bounds[1], app_constants::default_glucose_bounds[2], app_constants::default_glucose_bounds[3]};

  char hostSite[96] = {0};
  char nsToken[96] = {0};

  bool onboardingDone = false;
  bool soundAlarmEnabled = app_constants::default_alarm_enabled;

  // Громкость в процентах: 0 отключает звук, 100 задаёт максимум.
  uint8_t alarmVolume = app_constants::default_volume_percent;
  ns_alarm::Preferences nsAlarms;
  BatterySource batterySource = app_constants::default_battery_source;

  bool isValidBounds() const {
    return valid_glucose_bounds(sgvBounds);
  }

  uint8_t lastUiScreen = 0;

  uint8_t brightness = app_constants::default_brightness_percent;

};

class SettingsStore {
public:
  bool begin();
  void end();

  bool load(AppSettings& out);
  bool save(const AppSettings& s);
  bool saveBrightness(uint8_t percent);

  bool factoryReset(AppSettings& out);

private:
  Preferences _prefs;
};

extern AppSettings appCfg;
