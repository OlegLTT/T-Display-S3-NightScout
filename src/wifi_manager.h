#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiMulti.h>

#include "logs.h"   // ваш LogBuffer/LogLevel

struct WifiCred {
  const char* ssid;
  const char* pass;   // nullptr или "" -> открытая сеть
};

struct WifiManagerConfig {
  const char* hostname           = "t-display-s3";
  uint32_t connectTimeoutMs      = 20000;
  uint32_t retryDelayMs          = 500;
  bool enableAutoReconnect       = true;

  bool enableFallbackAP          = false;
  const char* fallbackApSsid     = "TDisplayS3-Setup";
  const char* fallbackApPass     = "12345678";
  IPAddress apIp                 = IPAddress(192,168,4,1);
  IPAddress apGw                 = IPAddress(192,168,4,1);
  IPAddress apMask               = IPAddress(255,255,255,0);
};

class WifiManager {
public:
  WifiManager() = default;

  void setConfig(const WifiManagerConfig& cfg) { _cfg = cfg; }

  // Подключить ваш буфер логов (можно nullptr)
  void setLogger(LogBuffer* logBuf) { _logs = logBuf; }

  // Добавить список сетей
  void addNetworks(const WifiCred* list, size_t count);

  // Запуск STA подключения
  bool begin();

  bool isStaConnected() const;
  bool isApMode() const;

  IPAddress staIP() const { return WiFi.localIP(); }
  IPAddress apIP() const  { return WiFi.softAPIP(); }

private:
  WifiManagerConfig _cfg{};
  WiFiMulti _multi;
  LogBuffer* _logs = nullptr;
  bool _apMode = false;

  void logI(const String& s);
  void logW(const String& s);
  void logE(const String& s);

  void logStatus(const char* prefix = nullptr);
  void startFallbackAP();
};
