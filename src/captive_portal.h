// Точка доступа и веб-настройки: DNS-перенаправление, сохранение параметров и тест звука.
#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>

#include "settings.h"
#include "logs.h"

class CaptivePortal {
public:
  bool begin(LogBuffer* logs, SettingsStore* store, AppSettings* settings);
  void stop();
  bool active() const { return _active; }

  void tick();

  // Забирает запрос проверки звука один раз; выбранная для теста громкость не сохраняется.
  bool takeSoundTest(uint8_t& volume) {
    if (!_soundTestPending) return false;
    _soundTestPending = false;
    volume = _soundTestVolume;
    return true;
  }

  const String& apSsid() const { return _apSsid; }
  const String& apPass() const { return _apPass; }
  IPAddress apIP() const { return _apIP; }

private:
  bool _active = false;
  bool _soundTestPending = false;
  uint8_t _soundTestVolume = 0;

  WebServer _srv{80};
  DNSServer _dns;

  LogBuffer* _logs = nullptr;
  SettingsStore* _store = nullptr;
  AppSettings* _settings = nullptr;

  IPAddress _apIP = IPAddress(192, 168, 4, 1);
  String _apSsid = "NS-Monitior-Setup";
  String _apPass = "12345678";

  bool _evtInstalled = false;

  void logI(const String& s);
  void logE(const String& s);

  String htmlPage() const;
  void handleCfg();
  void handleSave();

  static float toF(const String& s);
};
