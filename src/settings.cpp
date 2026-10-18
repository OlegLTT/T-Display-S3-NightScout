// Параметры устройства и их загрузка, сохранение и сброс в пространстве NVS appcfg.
#include "app_constants.h"
#include "settings.h"
#include <nvs_flash.h>

// Пустое поле удаляется из NVS: putString не различает успех пустой записи и ошибку.
static bool writeStr(Preferences& p, const char* key, const char* s, size_t maxLen) {
  if (!s) s = "";
  char tmp[128];
  size_t n = strlcpy(tmp, s, min(maxLen, sizeof(tmp)));
  (void)n;

  if (!tmp[0]) return !p.isKey(key) || p.remove(key);
  return p.putString(key, tmp) == strlen(tmp);
}

static void readStr(Preferences& p, const char* key, char* out, size_t maxLen, const char* def="") {
  String v = p.getString(key, def);
  strlcpy(out, v.c_str(), maxLen);
}

bool SettingsStore::begin() {
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {

    nvs_flash_erase();
    err = nvs_flash_init();
  }
  if (err != ESP_OK) {
    return false;
  }
  return _prefs.begin("appcfg", false);
}

void SettingsStore::end() {
  _prefs.end();
}

bool SettingsStore::load(AppSettings& out) {

  for (int i = 0; i < MAX_WIFI; i++) {
    char k1[16], k2[16];
    snprintf(k1, sizeof(k1), "w%d_ssid", i);
    snprintf(k2, sizeof(k2), "w%d_pass", i);
    readStr(_prefs, k1, out.wifi[i].ssid, sizeof(out.wifi[i].ssid), "");
    readStr(_prefs, k2, out.wifi[i].pass, sizeof(out.wifi[i].pass), "");
  }

  out.tzMinutes = (int16_t)_prefs.getShort("tzMin", app_constants::default_timezone_minutes);
  out.dstEnable = _prefs.getBool("dst", app_constants::default_dst);

  out.lastUiScreen = _prefs.getUChar("uiScr", 0);
  if (out.lastUiScreen > 1) out.lastUiScreen = 0;

  for (int i = 0; i < 4; i++) {
    char k[12];
    snprintf(k, sizeof(k), "b%d", i);
    out.sgvBounds[i] = _prefs.getFloat(k, NAN);
  }

  readStr(_prefs, "host", out.hostSite, sizeof(out.hostSite), "");
  readStr(_prefs, "nsToken", out.nsToken, sizeof(out.nsToken), "");

  out.onboardingDone = _prefs.getBool("onbDone", false);
  out.alarmVolume = _prefs.getUChar("alarmVolume", app_constants::default_volume_percent);
  readStr(_prefs, "boundsOrigin", out.boundsOrigin, sizeof(out.boundsOrigin), "");
  // Новый маркер исключает ручные/заводские значения всех предыдущих прошивок.
  if (!ns_alarm::cached_bounds_ready(out.sgvBounds,out.boundsOrigin,out.hostSite,
                                    _prefs.getBool("boundsServer",false))) out.clearBounds();
  const uint8_t batterySource = _prefs.getUChar("batterySource",
      static_cast<uint8_t>(app_constants::default_battery_source));
  out.batterySource = valid_battery_source(batterySource)
      ? static_cast<BatterySource>(batterySource) : app_constants::default_battery_source;
  if (out.alarmVolume > 100) out.alarmVolume = 100;

  out.brightness = _prefs.getUChar("br", out.brightness);
  if (out.brightness < 1 || out.brightness > 100) {
    out.brightness = app_constants::default_brightness_percent;
  }

  return true;
}

// Возвращает ошибку при неудачной записи. Сохранение отдельных ключей не является транзакцией.
bool SettingsStore::save(const AppSettings& s) {
  bool ok = true;

  for (int i = 0; i < MAX_WIFI; i++) {
    char k1[16], k2[16];
    snprintf(k1, sizeof(k1), "w%d_ssid", i);
    snprintf(k2, sizeof(k2), "w%d_pass", i);
    ok = writeStr(_prefs, k1, s.wifi[i].ssid, sizeof(s.wifi[i].ssid)) && ok;
    ok = writeStr(_prefs, k2, s.wifi[i].pass, sizeof(s.wifi[i].pass)) && ok;
  }

  ok = (_prefs.putShort("tzMin", s.tzMinutes) > 0) && ok;
  ok = (_prefs.putBool("dst", s.dstEnable) > 0) && ok;

  uint8_t ui = s.lastUiScreen;
  if (ui > 1) ui = 0;
  ok = (_prefs.putUChar("uiScr", ui) > 0) && ok;

  ok = saveBounds(s) && ok;

  ok = writeStr(_prefs, "host", s.hostSite, sizeof(s.hostSite)) && ok;
  ok = writeStr(_prefs, "nsToken", s.nsToken, sizeof(s.nsToken)) && ok;

  ok = (_prefs.putBool("onbDone", s.onboardingDone) > 0) && ok;
  ok = (_prefs.putUChar("alarmVolume", s.alarmVolume) > 0) && ok;
  ok = (_prefs.putUChar("batterySource", static_cast<uint8_t>(s.batterySource)) > 0) && ok;
  if (ok && _prefs.isKey("missingAlarm")) ok = _prefs.remove("missingAlarm");
  if (ok && _prefs.isKey("alarmEnable")) ok = _prefs.remove("alarmEnable");

  ok = (_prefs.putUChar("br", s.brightness) > 0) && ok;

  return ok;
}

// Сохранение серверного снимка пишет только пороги, не затрагивая локальные настройки.
bool SettingsStore::saveBounds(const AppSettings& s) {
  // Незавершённая запись не должна стать смесью старых и новых серверных порогов.
  if (_prefs.putBool("boundsServer",false) == 0) return false;
  const bool haveBounds=s.hasServerBounds();
  bool ok = true;
  for (unsigned i=0; i<4; ++i) {
    char key[12];
    snprintf(key,sizeof(key),"b%u",i);
    if (haveBounds) ok = (_prefs.putFloat(key,s.sgvBounds[i]) > 0) && ok;
    else if (_prefs.isKey(key)) ok = _prefs.remove(key) && ok;
  }
  ok = writeStr(_prefs,"boundsOrigin",haveBounds?s.boundsOrigin:"",sizeof(s.boundsOrigin)) && ok;
  if (_prefs.isKey("boundsOverride")) ok = _prefs.remove("boundsOverride") && ok;
  if (_prefs.isKey("nsAlarms")) ok = _prefs.remove("nsAlarms") && ok;
  if (ok && haveBounds) ok = _prefs.putBool("boundsServer",true) > 0;
  return ok;
}

bool SettingsStore::saveBrightness(uint8_t percent) {
  return percent>=1 && percent<=100 && _prefs.putUChar("br",percent)==sizeof(percent);
}

// После очистки NVS пороги загружаются заново из Nightscout.
bool SettingsStore::factoryReset(AppSettings& out) {

  if (!_prefs.clear()) return false;

  out = AppSettings();

  out.onboardingDone = false;

  return save(out);
}

