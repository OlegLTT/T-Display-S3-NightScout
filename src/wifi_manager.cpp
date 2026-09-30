// Первичное подключение к доступной сети и переподключение к выбранному SSID.
#include "wifi_manager.h"

static const char* wlStatusStr(wl_status_t st) {
  switch (st) {
    case WL_IDLE_STATUS:      return "IDLE";
    case WL_NO_SSID_AVAIL:    return "NO_SSID_AVAIL";
    case WL_SCAN_COMPLETED:   return "SCAN_COMPLETED";
    case WL_CONNECTED:        return "CONNECTED";
    case WL_CONNECT_FAILED:   return "CONNECT_FAILED";
    case WL_CONNECTION_LOST:  return "CONNECTION_LOST";
    case WL_DISCONNECTED:     return "DISCONNECTED";
    default:                  return "UNKNOWN";
  }
}

void WifiManager::addNetworks(const WifiCred* list, size_t count) {
  _credCount = 0;
  for (size_t i = 0; i < count; i++) {
    if (!list[i].ssid || list[i].ssid[0] == '\0') continue;

    const char* pass = list[i].pass;
    const bool open = (!pass || pass[0] == '\0');

    logs::logIf("[WiFi] addAP: '%s' (%s)", list[i].ssid, open ? "open" : "password set");
    _multi.addAP(list[i].ssid, open ? nullptr : pass);

    if (_credCount < (sizeof(_creds) / sizeof(_creds[0]))) {
      _creds[_credCount].ssid = String(list[i].ssid);
      _creds[_credCount].pass = open ? String("") : String(pass);
      _credCount++;
    }
  }
}

bool WifiManager::findPassForSsid(const String& ssid, String& outPass) const {
  for (size_t i = 0; i < _credCount; i++) {
    if (_creds[i].ssid == ssid) {
      outPass = _creds[i].pass;
      return true;
    }
  }
  return false;
}

void WifiManager::lockCurrentSsid() {
  String ssid = WiFi.SSID();
  if (ssid.isEmpty()) return;

  if (!_lockedSsid.isEmpty()) return;

  String pass;
  (void)findPassForSsid(ssid, pass);

  _lockedSsid = ssid;
  _lockedPass = pass;

  logs::logIf("[WiFi] last connected SSID='%s'", _lockedSsid.c_str());
}

void WifiManager::logStatus(const char* prefix) {
  const char* p = prefix ? prefix : "";
  wl_status_t st = WiFi.status();
  logs::logIf("%s%s (mode=%d)", p, wlStatusStr(st), (int)WiFi.getMode());
}

bool WifiManager::begin() {
  logs::logI("[WiFi] ---- begin ----");
  logs::logIf("[WiFi] hostname: %s", _cfg.hostname ? _cfg.hostname : "(null)");
  logs::logIf("[WiFi] timeout : %lu ms", (unsigned long)_cfg.connectTimeoutMs);

  WiFi.mode(WIFI_STA);
  if (_cfg.hostname && _cfg.hostname[0]) WiFi.setHostname(_cfg.hostname);

  _lastReconnectAttemptMs = 0;
  _windowStartMs = 0;

  for (uint8_t attempt = 1; attempt <= _cfg.beginAttempts; attempt++) {
    uint32_t t0 = millis();
    logs::logIf("[WiFi] attempt %u/%u", (unsigned)attempt, (unsigned)_cfg.beginAttempts);

    while (millis() - t0 < _cfg.connectTimeoutMs) {
      if (_multi.run() == WL_CONNECTED) {
        logs::logIf("[WiFi] CONNECTED: SSID='%s'", WiFi.SSID().c_str());
        logs::logIf("[WiFi] IP       : %s", WiFi.localIP().toString().c_str());
        int rssi = WiFi.RSSI();
        logs::logIf("[WiFi] RSSI     : %d dBm", rssi);

        if (rssi < -75) logs::logWf("[WiFi] Weak signal: %d dBm", rssi);

        lockCurrentSsid();

        logs::logI("[WiFi] ---- ok ----");
        return true;
      }
      delay(_cfg.retryDelayMs);
    }

    logs::logW("[WiFi] connect timeout (attempt failed)");
  }

  logs::logEf("[WiFi] CONNECT FAILED after %u attempts", (unsigned)_cfg.beginAttempts);
  return false;
}

void WifiManager::tick() {
  if (!_cfg.enableAutoReconnect) return;

  if (WiFi.status() == WL_CONNECTED) {

    lockCurrentSsid();
    return;
  }

  if (_lockedSsid.isEmpty()) return;

  const uint32_t now = millis();

  if (_windowStartMs == 0 || (now - _windowStartMs) > _cfg.reconnectTryWindowMs) {
    _windowStartMs = now;
    _lastReconnectAttemptMs = 0;
  }

  if (_lastReconnectAttemptMs != 0 && (now - _lastReconnectAttemptMs) < _cfg.reconnectPeriodMs) {
    return;
  }
  _lastReconnectAttemptMs = now;

  logs::logWf("[WiFi] reconnect -> SSID='%s'", _lockedSsid.c_str());

  WiFi.disconnect(false);
  delay(10);

  if (_lockedPass.isEmpty()) WiFi.begin(_lockedSsid.c_str());
  else WiFi.begin(_lockedSsid.c_str(), _lockedPass.c_str());
}
