#pragma once
#include <Arduino.h>
#include <Preferences.h>

static constexpr int MAX_WIFI = 5;

struct WifiSlot {
  char ssid[33] = {0};   // 32 + '\0'
  char pass[65] = {0};   // 64 + '\0'
};

struct AppSettings {
  WifiSlot wifi[MAX_WIFI];

  // timezone: минуты относительно UTC (например UTC+5 => +300)
  int16_t tzMinutes = 0;
  bool    dstEnable = false; // просто +1 час, НЕ “умное” DST по странам

  // 4 границы (mmol/L) для подсветки SGV
  // [0]=R->Y, [1]=Y->G, [2]=G->Y, [3]=Y->R
  float sgvBounds[4] = {3.9f, 4.5f, 8.5f, 10.0f};

  // Nightscout host/site (без схемы или со схемой)
  // например: "jxxxxxxx.nightscout-jino.ru" или "https://...."
  char hostSite[96] = {0};

  bool isValidBounds() const {
    return sgvBounds[0] < sgvBounds[1] &&
           sgvBounds[1] < sgvBounds[2] &&
           sgvBounds[2] < sgvBounds[3];
  }

    // последний экран UI: 0=Main, 1=Clock (экран логов НЕ сохраняем)
  uint8_t lastUiScreen = 0;

};

class SettingsStore {
public:
  bool begin();
  void end();

  bool load(AppSettings& out);
  bool save(const AppSettings& s);

private:
  Preferences _prefs;
};

extern AppSettings appCfg;