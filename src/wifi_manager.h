// Первичное подключение к доступной сети и переподключение к выбранному SSID.
#pragma once
#include "app_constants.h"
#include <Arduino.h>
#include <WiFi.h>
#include "logs.h"

struct WifiCred {
  const char* ssid;
  const char* pass;
};

struct WifiManagerConfig {
  const char* hostname           = app_constants::wifi_default_hostname;
  uint32_t connectTimeoutMs      = app_constants::wifi_connect_timeout_ms;
  uint32_t retryDelayMs          = app_constants::wifi_retry_delay_ms;
  bool enableAutoReconnect       = app_constants::wifi_auto_reconnect;

  uint8_t  beginAttempts         = app_constants::wifi_begin_attempts;

  uint32_t reconnectPeriodMs     = app_constants::wifi_reconnect_period_ms;
  uint32_t reconnectTryWindowMs  = app_constants::wifi_reconnect_window_ms;
};

class WifiManager {
public:
  WifiManager() = default;

  void setConfig(const WifiManagerConfig& cfg) { _cfg = cfg; }
  void setLogger(LogBuffer* logBuf) { _logs = logBuf; }

  void addNetworks(const WifiCred* list, size_t count);

  // Первый выбор сети через WiFiMulti; успешный SSID фиксируется для повторных подключений.
  bool begin();

  // Переподключение выполняется только к последней выбранной сети.
  void tick();

  const char* lastSsid() const { return _lockedSsid.c_str(); }
  bool isLocked() const { return !_lockedSsid.isEmpty(); }

private:
  WifiManagerConfig _cfg{};
  size_t _networkIndex = 0;
  bool _reportedConnected = false;
  LogBuffer* _logs = nullptr;

  struct CredItem {
    String ssid;
    String pass;
  };
  CredItem _creds[app_constants::wifi_credential_capacity];
  size_t _credCount = 0;

  String _lockedSsid;
  String _lockedPass;

  uint32_t _lastReconnectAttemptMs = 0;
  uint32_t _windowStartMs = 0;

  void logStatus(const char* prefix = nullptr);
  void lockCurrentSsid();
  bool findPassForSsid(const String& ssid, String& outPass) const;
};
