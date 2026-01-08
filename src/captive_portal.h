// ===== captive_portal.h =====
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

  void tick(); // вызывать в loop()

  // String apSsid() const { return _apSsid; }
  // String apPass() const { return _apPass; }
  const String& apSsid() const { return _apSsid; }
  const String& apPass() const { return _apPass; }
  IPAddress apIP() const { return _apIP; }

private:
  bool _active = false;

  WebServer _srv{80};
  DNSServer _dns;

  LogBuffer* _logs = nullptr;
  SettingsStore* _store = nullptr;
  AppSettings* _settings = nullptr;

  IPAddress _apIP = IPAddress(192, 168, 4, 1);
  String _apSsid = "TDisplay-Setup";
  String _apPass = "12345678";

  bool _evtInstalled = false;

  void logI(const String& s);
  void logE(const String& s);

  // HTML + handlers
  String htmlPage() const;
  void handleCfg();
  void handleSave();

  // helpers
  static float toF(const String& s);
};
