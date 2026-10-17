// Первичное подключение к доступной сети и переподключение к выбранному SSID.
#include "app_constants.h"
#include "wifi_manager.h"

static const char* wlStatusStr(wl_status_t st) {
  switch (st) {
    case WL_IDLE_STATUS:      return "idle";
    case WL_NO_SSID_AVAIL:    return "network not found";
    case WL_SCAN_COMPLETED:   return "scan complete";
    case WL_CONNECTED:        return "connected";
    case WL_CONNECT_FAILED:   return "connection failed";
    case WL_CONNECTION_LOST:  return "connection lost";
    case WL_DISCONNECTED:     return "disconnected";
    default:                  return "unknown";
  }
}

void WifiManager::addNetworks(const WifiCred* list, size_t count) {
  _credCount = 0;
  for (size_t i = 0; i < count; i++) {
    if (!list[i].ssid || list[i].ssid[0] == '\0') continue;

    const char* pass = list[i].pass;
    const bool open = (!pass || pass[0] == '\0');

    logs::logIf("[Wi-Fi] Network '%s' (%s)", list[i].ssid, open ? "open" : "password set");

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

  logs::logIf("[Wi-Fi] Selected network '%s'", _lockedSsid.c_str());
}

void WifiManager::logStatus(const char* prefix) {
  const char* p = prefix ? prefix : "";
  wl_status_t st = WiFi.status();
  logs::logIf("%s%s (mode=%d)", p, wlStatusStr(st), (int)WiFi.getMode());
}

bool WifiManager::begin() {
  if (!_credCount) return false;
  const bool ap = WiFi.getMode() & WIFI_MODE_AP;
  WiFi.mode(ap ? WIFI_AP_STA : WIFI_STA);
  if (_cfg.hostname && _cfg.hostname[0]) WiFi.setHostname(_cfg.hostname);
  _lockedSsid = ""; _lockedPass = ""; _networkIndex = 0;
  _reportedConnected = false;
  _lastReconnectAttemptMs = millis();
  String password;
  if (WiFi.status() == WL_CONNECTED && findPassForSsid(WiFi.SSID(), password)) {
    lockCurrentSsid();
    return true;
  }
  WiFi.disconnect(false);
  const auto& network = _creds[_networkIndex];
  WiFi.begin(network.ssid.c_str(), network.pass.isEmpty() ? nullptr : network.pass.c_str());
  logs::logIf("[Wi-Fi] Connecting to '%s'", network.ssid.c_str());
  return true;
}

void WifiManager::tick() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!_reportedConnected) {
      _reportedConnected = true;
      lockCurrentSsid();
      logs::logIf("[Wi-Fi] Connected: %s, RSSI=%d", WiFi.SSID().c_str(), WiFi.RSSI());
    }
    return;
  }
  if (!_cfg.enableAutoReconnect || !_credCount) return;
  if (_reportedConnected) { logs::logW("[Wi-Fi] Connection lost"); _reportedConnected=false; }
  const uint32_t now = millis();
  if (uint32_t(now - _lastReconnectAttemptMs) < _cfg.connectTimeoutMs) return;
  _lastReconnectAttemptMs = now;
  if (!_lockedSsid.isEmpty()) {
    WiFi.begin(_lockedSsid.c_str(), _lockedPass.isEmpty() ? nullptr : _lockedPass.c_str());
  } else {
    _networkIndex = (_networkIndex + 1) % _credCount;
    const auto& network = _creds[_networkIndex];
    WiFi.begin(network.ssid.c_str(), network.pass.isEmpty() ? nullptr : network.pass.c_str());
  }
  logs::logW("[Wi-Fi] Reconnecting");
}
