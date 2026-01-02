#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

// ---- Подключите ваш логгер сюда, если есть ----
// Если у вас есть logs.add(LogLevel::Info, "..."), вы можете подключить его,
// либо оставить дефолтный Serial-лог.
#ifndef NS_LOGI
  #define NS_LOGI(...) do { Serial.printf(__VA_ARGS__); Serial.print("\n"); } while(0)
#endif
#ifndef NS_LOGE
  #define NS_LOGE(...) do { Serial.printf(__VA_ARGS__); Serial.print("\n"); } while(0)
#endif

// Конфиг максимально близко к вашему cfg.*
struct NSConfig {
  String  baseUrl;        // cfg.nightscout_url
  String  token;          // cfg.nightscout_token (может быть пустой)
  bool    sgvOnly   = false; // cfg.sgv_only
  bool    showMgdl  = false; // cfg.show_mgdl (или аналог)
  uint8_t infoLine  = 2;     // cfg.info_line (2 или 3)
  bool    preferHttpsIfNoScheme = true; // добавлять https:// если без схемы
  uint32_t httpTimeoutMs = 15000;
  bool    tlsInsecure = true; // как у вас часто на ESP: setInsecure()
};

// Результат — единая “состояние Nightscout” как у вас в проекте
struct NightscoutData {
  bool okEntries = false;
  bool okProps   = false;
  int  httpEntries = 0;
  int  httpProps   = 0;

  // entries
  long long rawtime_ms = 0;
  time_t    sensTime   = 0;
  float     sgv_mgdl   = 0.0f;
  float     sgv_mmol   = 0.0f;
  char      dir[32]    = "N/A";
  char      dev[64]    = "N/A";
  float     last10mmol[10] = {0};

  // props
  float iob = 0.0f;
  float cob = 0.0f;

  int   delta_mgdl = 0;
  float delta_mmol = 0.0f;
  char  delta_display[16] = "N/A";

  char  loop_label[16] = "N/A";
  char  loop_code[16]  = "N/A";
  char  basal_display[16] = "N/A";
};

class NightscoutClient {
public:
  NightscoutClient() = default;

  void begin(const NSConfig& cfg);

  // как “readNightscout()”: обновляет entries + props
  bool update(NightscoutData& out);

private:
  NSConfig _cfg;

  String buildBase(String url) const;
  String makeEntriesUrl(const String& base) const;
  String makePropsUrl(const String& base) const;

  bool httpGet(const String& url, String& body, int& httpCode) const;
  void sanitizeJson(String& s) const;

  bool parseEntries(const String& json, NightscoutData& out, ArduinoJson::DynamicJsonDocument& doc) const;
  bool parseProps(const String& json, NightscoutData& out, ArduinoJson::DynamicJsonDocument& doc) const;

  static float mgdlToMmol(float mgdl) { return mgdl / 18.0f; }
};
