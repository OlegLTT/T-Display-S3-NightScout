#include "settings.h"

static void writeStr(Preferences& p, const char* key, const char* s, size_t maxLen) {
  if (!s) s = "";
  char tmp[128];
  size_t n = strlcpy(tmp, s, min(maxLen, sizeof(tmp)));
  (void)n;
  p.putString(key, tmp);
}

static void readStr(Preferences& p, const char* key, char* out, size_t maxLen, const char* def="") {
  String v = p.getString(key, def);
  strlcpy(out, v.c_str(), maxLen);
}

bool SettingsStore::begin() {
  return _prefs.begin("appcfg", false); // RW
}
void SettingsStore::end() {
  _prefs.end();
}

bool SettingsStore::load(AppSettings& out) {
  // WiFi slots
  for (int i = 0; i < MAX_WIFI; i++) {
    char k1[16], k2[16];
    snprintf(k1, sizeof(k1), "w%d_ssid", i);
    snprintf(k2, sizeof(k2), "w%d_pass", i);
    readStr(_prefs, k1, out.wifi[i].ssid, sizeof(out.wifi[i].ssid), "");
    readStr(_prefs, k2, out.wifi[i].pass, sizeof(out.wifi[i].pass), "");
  }

  out.tzMinutes = (int16_t)_prefs.getShort("tzMin", 0);
  out.dstEnable = _prefs.getBool("dst", false);

  out.lastUiScreen = _prefs.getUChar("uiScr", 0);
  if (out.lastUiScreen > 1) out.lastUiScreen = 0; // только 0/1


  // bounds
  for (int i = 0; i < 4; i++) {
    char k[12];
    snprintf(k, sizeof(k), "b%d", i);
    out.sgvBounds[i] = _prefs.getFloat(k, out.sgvBounds[i]);
  }

  // если кто-то записал мусор — вернём дефолт
  if (!out.isValidBounds()) {
    out.sgvBounds[0] = 3.9f;
    out.sgvBounds[1] = 4.5f;
    out.sgvBounds[2] = 8.5f;
    out.sgvBounds[3] = 10.0f;
  }

  // host site
  readStr(_prefs, "host", out.hostSite, sizeof(out.hostSite), "");

  return true;
}

bool SettingsStore::save(const AppSettings& s) {
  // WiFi slots
  for (int i = 0; i < MAX_WIFI; i++) {
    char k1[16], k2[16];
    snprintf(k1, sizeof(k1), "w%d_ssid", i);
    snprintf(k2, sizeof(k2), "w%d_pass", i);
    writeStr(_prefs, k1, s.wifi[i].ssid, sizeof(s.wifi[i].ssid));
    writeStr(_prefs, k2, s.wifi[i].pass, sizeof(s.wifi[i].pass));
  }

  _prefs.putShort("tzMin", s.tzMinutes);
  _prefs.putBool("dst", s.dstEnable);

  uint8_t ui = s.lastUiScreen;
  if (ui > 1) ui = 0;
  _prefs.putUChar("uiScr", ui);

  for (int i = 0; i < 4; i++) {
    char k[12];
    snprintf(k, sizeof(k), "b%d", i);
    _prefs.putFloat(k, s.sgvBounds[i]);
  }

  // host site
  writeStr(_prefs, "host", s.hostSite, sizeof(s.hostSite));

  return true;
}
