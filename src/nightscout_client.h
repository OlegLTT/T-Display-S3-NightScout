// Получение данных Nightscout по HTTP/HTTPS с повторными попытками и разбором JSON.
#pragma once
#include "app_constants.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "logs.h"

#define GSON_ENABLE_HASH
#include <GSON.h>
#include <StringUtils.h>

struct NSConfig {
  BatterySource batterySource = app_constants::default_battery_source;
  String   baseUrl;
  String   token;

  bool     preferHttpsIfNoScheme = app_constants::prefer_https;

  uint32_t httpTimeoutMs = app_constants::http_timeout_ms;
  uint32_t hostErrorBackoffMs = app_constants::host_error_backoff_ms;
  bool     tlsInsecure = app_constants::tls_insecure;

  float    sgvBounds[4] = {app_constants::default_glucose_bounds[0], app_constants::default_glucose_bounds[1], app_constants::default_glucose_bounds[2], app_constants::default_glucose_bounds[3]};

  uint32_t retryDelayMs = app_constants::ns_retry_delay_ms;

  // Нулевое значение означает неограниченное число повторных попыток.
  uint8_t  retryMax = app_constants::ns_retry_max;

  int      warnRssiDbm = app_constants::ns_warn_rssi_dbm;
  uint32_t weakRssiLogPeriodMs = app_constants::weak_rssi_log_ms;

  uint32_t noWifiLogPeriodMs = app_constants::no_wifi_log_ms;
};

struct NightscoutData {
  bool ok = false;
  int  http = 0;

  float    sgv_mmol = NAN;
  float    delta_mmol = NAN;
  uint64_t update_time_ms = 0;
  char     trend[24] = "N/A";

  float    iob = NAN;
  float    cob = NAN;

  // Заряд выбранного источника в процентах; -1 — нет корректных данных.
  int      batteryPct = -1;
  float    pumpBatV   = 0.0f;

  float    sgvBounds[4] = {app_constants::default_glucose_bounds[0], app_constants::default_glucose_bounds[1], app_constants::default_glucose_bounds[2], app_constants::default_glucose_bounds[3]};

  // Возраст вычисляется перед отрисовкой; INT32_MAX означает неизвестное время.
  int32_t  ageSec = INT32_MAX;
  bool     stale5 = false;
  bool     stale15 = false;
};

class NightscoutClient {
public:
  void begin(const NSConfig& cfg);
  void requestUpdate();
  bool tick(NightscoutData& out);

  bool isBusy() const { return _r.ph != Phase::Idle && _r.ph != Phase::WaitWiFi && _r.ph != Phase::RetryWait; }
  bool isUpdateWanted() const { return _wantUpdate; }
  uint32_t lastRequestAtMs() const { return _r.tStart; }

private:
  NSConfig _cfg{};

  String buildBase(String url) const;
  String makePropsPath(const String& base) const;
  void sanitizeJson(String& s) const;
  bool splitUrl(const String& fullUrl, bool& https, String& host, uint16_t& port, String& path) const;
  bool parseV2Properties(const String& json, NightscoutData& out) const;

  enum class Phase : uint8_t {
    Idle,
    WaitWiFi,
    RetryWait,
    Connect,
    Send,
    ReadHeaders,
    ReadBody,
    Done,
    Error
  };

  struct Req {
    Phase   ph = Phase::Idle;
    bool    https = false;
    String  host;
    uint16_t port = 0;
    String  path;

    int     httpCode = 0;
    int     contentLen = -1;

    String  headerLine;
    String  body;

    uint32_t tStart = 0;
    uint32_t tLastIO = 0;

    WiFiClientSecure cs;
    WiFiClient       cp;
  };

  Req _r{};

  bool     _wantUpdate = false;

  uint8_t  _retryCount = 0;
  uint32_t _retryAtMs  = 0;

  uint32_t _hostErrorAtMs = 0;
  uint32_t _hostBackoffMs = 0;

  uint32_t _lastNoWifiLogMs = 0;
  uint32_t _lastWeakLogMs   = 0;

  void applyBounds(NightscoutData& out) const;

  void startReq(const String& base);
  bool stepReq(NightscoutData& out);
  void abortReq();

  void scheduleRetry();
  void warnIfWeakRssi();

  static constexpr size_t READ_CHUNK = app_constants::http_read_chunk_bytes;
  static constexpr size_t BODY_MAX   = app_constants::http_body_max_bytes;
};
