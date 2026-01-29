#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>

#ifndef NS_LOGI
  #define NS_LOGI(...) do { Serial.printf(__VA_ARGS__); Serial.print("\n"); } while(0)
#endif
#ifndef NS_LOGW
  #define NS_LOGW(...) do { Serial.printf(__VA_ARGS__); Serial.print("\n"); } while(0)
#endif
#ifndef NS_LOGE
  #define NS_LOGE(...) do { Serial.printf(__VA_ARGS__); Serial.print("\n"); } while(0)
#endif

struct NSConfig {
  String  baseUrl;
  bool    preferHttpsIfNoScheme = true;
  uint32_t httpTimeoutMs = 8000;
  bool    tlsInsecure = true;
};

struct NightscoutData {
  bool okEntries = false;
  bool okProps   = false;
  int  httpEntries = 0;
  int  httpProps   = 0;

  long long rawtime_ms = 0;
  time_t    sensTime   = 0;
  float     sgv_mgdl   = 0.0f;
  float     sgv_mmol   = 0.0f;
  char      dir[32]    = "N/A";
  char      dev[64]    = "N/A";
  float     last10mmol[10] = {0};

  float iob = 0.0f;
  float cob = 0.0f;
  int   delta_mgdl = 0;
  float delta_mmol = 0.0f;
  char  delta_display[16] = "N/A";
  char  loop_label[16] = "N/A";
  char  loop_code[16]  = "N/A";
  char  basal_display[16] = "N/A";

  // --- Pump battery (from devicestatus) ---
  bool   okDevStatus   = false;
  int    httpDevStatus = 0;

  bool   pumpBatValid  = false;
  int    pumpBatPct    = -1;     // 0..100, -1 если неизвестно
  float  pumpBatV      = 0.0f;   // может быть 0 если не приходит
  time_t pumpTime      = 0;      // mills/1000 если есть

  uint16_t zoneColor = TFT_DARKGREY; // кеш цвета зоны
  bool     zoneValid = false;        // валиден ли цвет (есть sgv)
};

class NightscoutClient {
public:
  void begin(const NSConfig& cfg);
  void requestUpdate();
  bool tick(NightscoutData& out);

private:
  NSConfig _cfg;

  // helpers
  String buildBase(String url) const;

  String makeEntriesPath(const String& base) const;
  String makePropsPath(const String& base) const;
  String makeDevStatusPath(const String& base) const;

  void sanitizeJson(String& s) const;

  static float mgdlToMmol(float mgdl) { return mgdl / 18.0f; }

  bool parseEntries(const String& json, NightscoutData& out, ArduinoJson::DynamicJsonDocument& doc) const;
  bool parseProps(const String& json, NightscoutData& out, ArduinoJson::DynamicJsonDocument& doc) const;
  bool parseDevStatus(const String& json, NightscoutData& out, ArduinoJson::DynamicJsonDocument& doc) const;

  // FSM
  enum class ReqKind : uint8_t { None, Entries, Props, DevStatus };
  enum class Phase  : uint8_t { Idle, Connect, Send, ReadHeaders, ReadBody, Done, Error };

  struct Req {
    ReqKind kind = ReqKind::None;
    Phase   ph   = Phase::Idle;

    bool https = false;
    String host;
    uint16_t port = 0;
    String path;

    int httpCode = 0;
    int contentLen = -1;
    bool chunked = false;

    String headerLine;
    String body;

    uint32_t tStart = 0;
    uint32_t tLastIO = 0;

    WiFiClientSecure cs;
    WiFiClient cp;
  };

  Req _r;
  bool _wantUpdate = false;

  bool _entriesDone = false;
  bool _propsDone   = false;
  bool _devDone     = false;

  void startReq(ReqKind k, const String& base);
  bool stepReq(NightscoutData& out);
  void abortReq();
  bool splitUrl(const String& fullUrl, bool& https, String& host, uint16_t& port, String& path) const;

  // staging: синхронный коммит entries+props
  NightscoutData _stage;
  bool _stageHasEntries = false;
  bool _stageHasProps   = false;
  bool _committedThisCycle = false;

  // ← НОВОЕ: фатальная ошибка host/коннекта (DNS / SSL и т.п.)
  bool _hostError = false;

  void resetStageFrom(const NightscoutData& cur) {
    _stage = cur;
    _stageHasEntries = false;
    _stageHasProps = false;
    _committedThisCycle = false;
  }

  bool commitIfReady(NightscoutData& out) {
    if (!_committedThisCycle && _stageHasEntries && _stageHasProps) {
      out = _stage;
      _committedThisCycle = true;
      return true;
    }
    return false;
  }

  static constexpr size_t READ_CHUNK = 512;
  static constexpr size_t BODY_MAX   = 24 * 1024; 
};
