#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include "logs.h" 

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

  uint8_t  beginAttempts         = 3;      // 3 попытки на старте
  uint32_t reconnectPeriodMs     = 3000;   // период попыток при обрыве
  uint32_t reconnectTryWindowMs  = 8000;   // окно ожидания WL_CONNECTED на одну попытку
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

private:
  WifiManagerConfig _cfg{};
  WiFiMulti _multi;
  LogBuffer* _logs = nullptr;
  bool _apMode = false;

  void logStatus(const char* prefix = nullptr);
  void startFallbackAP();
};
