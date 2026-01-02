#include "wifi_manager.h"

void WifiManager::logI(const String& s) {
  Serial.println(s);
  if (_logs) _logs->add(LogLevel::Info, s);
}

void WifiManager::logW(const String& s) {
  Serial.println(s);
  if (_logs) _logs->add(LogLevel::Warn, s);
}

void WifiManager::logE(const String& s) {
  Serial.println(s);
  if (_logs) _logs->add(LogLevel::Error, s);
}

void WifiManager::addNetworks(const WifiCred* list, size_t count) {
  for (size_t i = 0; i < count; i++) {
    if (!list[i].ssid || list[i].ssid[0] == '\0') continue;

    const char* pass = list[i].pass;
    if (!pass || pass[0] == '\0') {
      logI(String("[WiFi] addAP: '") + list[i].ssid + "' (open)");
      _multi.addAP(list[i].ssid, nullptr);
    } else {
      logI(String("[WiFi] addAP: '") + list[i].ssid + "' (password set)");
      _multi.addAP(list[i].ssid, pass);
    }
  }
}

bool WifiManager::begin() {
  _apMode = false;

  logI("[WiFi] ---- begin ----");
  logI(String("[WiFi] hostname: ") + (_cfg.hostname ? _cfg.hostname : "(null)"));
  logI(String("[WiFi] timeout : ") + String((unsigned long)_cfg.connectTimeoutMs) + " ms");

  WiFi.mode(WIFI_STA);
  if (_cfg.hostname && _cfg.hostname[0]) WiFi.setHostname(_cfg.hostname);
  WiFi.setAutoReconnect(_cfg.enableAutoReconnect);

  // очистка предыдущих состояний
  WiFi.disconnect(true, true);
  delay(100);

  uint32_t t0 = millis();

  while ((millis() - t0) < _cfg.connectTimeoutMs) {
    uint8_t st = _multi.run();

    if (st == WL_CONNECTED) {
        logI(String("[WiFi] CONNECTED: SSID='") + WiFi.SSID() + "'");
        logI(String("[WiFi] IP       : ") + WiFi.localIP().toString());
        logI(String("[WiFi] RSSI     : ") + String(WiFi.RSSI()) + " dBm");
        logStatus("[WiFi] status  : ");
        logI("[WiFi] ---- ok ----");
        return true;
    }

    delay(_cfg.retryDelayMs);
  }


  logE("[WiFi] CONNECT TIMEOUT");
  logStatus("[WiFi] status  : ");

  if (_cfg.enableFallbackAP) {
    startFallbackAP();
  } else {
    logW("[WiFi] fallback AP disabled");
  }

  logE("[WiFi] ---- fail ----");
  return false;
}

bool WifiManager::isStaConnected() const {
  return (WiFi.status() == WL_CONNECTED) && (WiFi.getMode() & WIFI_STA);
}

bool WifiManager::isApMode() const {
  return _apMode && (WiFi.getMode() & WIFI_AP);
}

void WifiManager::startFallbackAP() {
  logW("[WiFi] starting fallback AP mode...");

  WiFi.enableAP(true);
  WiFi.mode(WIFI_AP);

  bool cfgOk = WiFi.softAPConfig(_cfg.apIp, _cfg.apGw, _cfg.apMask);
  logI(String("[WiFi] softAPConfig: ") + (cfgOk ? "OK" : "FAIL"));

  bool apOk = WiFi.softAP(_cfg.fallbackApSsid, _cfg.fallbackApPass);
  logI(String("[WiFi] softAP start : ") + (apOk ? "OK" : "FAIL"));

  logI(String("[WiFi] AP SSID      : ") + _cfg.fallbackApSsid);
  logI(String("[WiFi] AP IP        : ") + WiFi.softAPIP().toString());

  _apMode = apOk;

  if (!apOk) {
    logE("[WiFi] AP mode failed");
  }
}

void WifiManager::logStatus(const char* prefix) {
  const char* p = prefix ? prefix : "";

  wl_status_t st = WiFi.status();
  const char* s = "UNKNOWN";
  switch (st) {
    case WL_IDLE_STATUS:      s = "IDLE"; break;
    case WL_NO_SSID_AVAIL:    s = "NO_SSID_AVAIL"; break;
    case WL_SCAN_COMPLETED:   s = "SCAN_COMPLETED"; break;
    case WL_CONNECTED:        s = "CONNECTED"; break;
    case WL_CONNECT_FAILED:   s = "CONNECT_FAILED"; break;
    case WL_CONNECTION_LOST:  s = "CONNECTION_LOST"; break;
    case WL_DISCONNECTED:     s = "DISCONNECTED"; break;
    default:                  s = "UNKNOWN"; break;
  }

  logI(String(p) + s + " (mode=" + String((int)WiFi.getMode()) + ")");
}
