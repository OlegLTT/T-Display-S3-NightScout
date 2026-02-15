#include "wifi_manager.h"

void WifiManager::addNetworks(const WifiCred* list, size_t count) {
  for (size_t i = 0; i < count; i++) {
    if (!list[i].ssid || list[i].ssid[0] == '\0') continue;

    const char* pass = list[i].pass;
    if (!pass || pass[0] == '\0') {
      logs::logIf("[WiFi] addAP: '%s' (open)", list[i].ssid);
      _multi.addAP(list[i].ssid, nullptr);
    } else {
      logs::logIf("[WiFi] addAP: '%s' (password set)", list[i].ssid);
      _multi.addAP(list[i].ssid, pass);
    }
  }
}

bool WifiManager::begin() {
  logs::logI("[WiFi] ---- begin ----");
  logs::logIf("[WiFi] hostname: %s", _cfg.hostname ? _cfg.hostname : "(null)");
  logs::logIf("[WiFi] timeout : %lu ms", (unsigned long)_cfg.connectTimeoutMs);

  WiFi.mode(WIFI_STA);
  if (_cfg.hostname && _cfg.hostname[0]) WiFi.setHostname(_cfg.hostname);

  uint32_t t0 = millis();
  while (millis() - t0 < _cfg.connectTimeoutMs) {
    if (_multi.run() == WL_CONNECTED) {
      logs::logIf("[WiFi] CONNECTED: SSID='%s'", WiFi.SSID().c_str());
      logs::logIf("[WiFi] IP       : %s", WiFi.localIP().toString().c_str());

      int rssi = WiFi.RSSI();
      logs::logIf("[WiFi] RSSI     : %d dBm", rssi);
      if (rssi < -75) {
        logs::logWf("[WiFi] Weak signal: %d dBm", rssi);
      }

      logs::logI("[WiFi] ---- ok ----");
      return true;
    }
    delay(_cfg.retryDelayMs);
    
  }

  logs::logEf("[WiFi] CONNECT TIMEOUT");
  return false;
}


void WifiManager::startFallbackAP() {
  logs::logW("[WiFi] starting fallback AP mode...");

  WiFi.enableAP(true);
  WiFi.mode(WIFI_AP);

  bool cfgOk = WiFi.softAPConfig(_cfg.apIp, _cfg.apGw, _cfg.apMask);
  logs::logIf("[WiFi] softAPConfig: %s", cfgOk ? "OK" : "FAIL");

  bool apOk = WiFi.softAP(_cfg.fallbackApSsid, _cfg.fallbackApPass);
  logs::logIf("[WiFi] softAP start : %s", apOk ? "OK" : "FAIL");

  logs::logIf("[WiFi] AP SSID      : %s", _cfg.fallbackApSsid ? _cfg.fallbackApSsid : "(null)");
  logs::logIf("[WiFi] AP IP        : %s", WiFi.softAPIP().toString().c_str());

  _apMode = apOk;

  if (!apOk) {
    logs::logE("[WiFi] AP mode failed");
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

  logs::logIf("%s%s (mode=%d)", p, s, (int)WiFi.getMode());
}

