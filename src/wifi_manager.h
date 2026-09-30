// Первичное подключение к доступной сети и переподключение к выбранному SSID.
#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include "logs.h"

struct WifiCred {
  const char* ssid;
  const char* pass;
};

struct WifiManagerConfig {
  const char* hostname           = "NS-Monitior";
  uint32_t connectTimeoutMs      = 20000;
  uint32_t retryDelayMs          = 500;
  bool enableAutoReconnect       = true;

  uint8_t  beginAttempts         = 3;

  uint32_t reconnectPeriodMs     = 3000;
  uint32_t reconnectTryWindowMs  = 8000;
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
  WiFiMulti _multi;
  LogBuffer* _logs = nullptr;

  struct CredItem {
    String ssid;
    String pass;
  };
  CredItem _creds[10];
  size_t _credCount = 0;

  String _lockedSsid;
  String _lockedPass;

  uint32_t _lastReconnectAttemptMs = 0;
  uint32_t _windowStartMs = 0;

  void logStatus(const char* prefix = nullptr);
  void lockCurrentSsid();
  bool findPassForSsid(const String& ssid, String& outPass) const;
};
