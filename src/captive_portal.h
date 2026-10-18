// Точка доступа и веб-настройки: DNS-перенаправление и сохранение параметров.
#pragma once
#include "app_constants.h"
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
  void scheduleRestart() { _restartPending = true; _restartAt = millis(); }
  const String& apSsid() const { return _apSsid; }
  const String& apPass() const { return _apPass; }
  IPAddress apIP() const { return _apIP; }

private:
  bool _active = false;
  bool _restartPending = false;
  uint32_t _restartAt = 0;

  WebServer _srv{80};
  DNSServer _dns;

  LogBuffer* _logs = nullptr;
  SettingsStore* _store = nullptr;
  AppSettings* _settings = nullptr;

  IPAddress _apIP = IPAddress(192, 168, 4, 1);
  String _apSsid = app_constants::portal_ssid;
  String _apPass = "12345678";

  bool _evtInstalled = false;

  void logI(const String& s);
  void logE(const String& s);

  String htmlPage() const;
  void handleCfg();
  void handleSave();
  bool readConnectionForm(AppSettings& settings);

};
