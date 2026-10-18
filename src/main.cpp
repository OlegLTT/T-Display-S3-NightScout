// Запуск устройства и общий цикл: экраны, кнопки, сеть, тревога и подсветка.
#include "app_constants.h"
#include <Arduino.h>
#include "board_config.h"
#include <WiFi.h>
#include <time.h>
#include <Wire.h>

#include "settings.h"
#include "captive_portal.h"
#include "time_utils.h"

#include <lvgl.h>
#include "gfx/LVGL_Display.h"
#include "gfx/Display.h"

#include "ui_main.h"
#include "ui_logs.h"
#include "ui_clock.h"
#include "ui_settings.h"
#include "ui_navigation.h"

#include "nightscout_client.h"
#include "nightscout_session.h"
#include "nightscout_poll.h"
#include "backlight.h"
#include "buzzer.h"
#include "ui_overlay.h"
#include "logs.h"
#include "wifi_manager.h"
#include "alarm_state.h"
#include "snooze_lan.h"
#include "glucose_zone.h"

#if defined(BOARD_TDISPLAY_S3)
  #define UB_DEB_TIME app_constants::button_debounce_ms
  #define UB_HOLD_TIME app_constants::button_hold_ms
  #define UB_STEP_TIME app_constants::button_step_delay_ms
  #define UB_STEP_PRD app_constants::button_step_period_ms
  #define UB_CLICK_TIME app_constants::button_click_ms
  #include <uButton.h>
  #include <uButtonMulti.h>
#endif

#if defined(BOARD_ESP32S3_DEV)
  #include "lcd_bsp.h"
  #include "FT3168.h"
#endif

static SettingsStore settingsStore;
AppSettings appCfg;
static CaptivePortal portal;

static WifiManager wifiMgr;
static WifiManagerConfig wifiCfg;

static LogBuffer g_logs;

#if defined(BOARD_TDISPLAY_S3) || defined(BOARD_M5CORE_S3)
static NightscoutSession g_ns;
static ns_alarm::Snapshot nsAlarmSnapshot;
static ns_alarm::Kind nsAlarmKind = ns_alarm::Kind::None;
static uint32_t nsImportedRevision = 0;
#else
static NightscoutClient g_ns;
#endif
static NightscoutData   nsData;

// Последний успешный ответ остаётся на экране при временных ошибках сети.
static NightscoutData   lastGood;
static bool             haveLastGood = false;

// При запуске журнал отображается до первого успешного ответа Nightscout.
static bool bootWaitNsSecond = true;

static BrightnessOverlay brOverlay;
static Backlight backlight;

#if defined(BOARD_TDISPLAY_S3)
  static uButton btnBoot(PIN_BUTTON_A);
  static uButton btnUser(PIN_BUTTON_B);
  static uButtonMulti btnBoth;
#endif

static volatile bool g_rebootRequested = false;

static time_t lastNtpSync = 0;
static bool g_ntpWasValid = false;

enum class ScreenId : uint8_t { SCREEN_MAIN, SCREEN_CLOCK, SCREEN_LOGS, SCREEN_SETTINGS };

static lv_obj_t* g_scr_main  = nullptr;
static lv_obj_t* g_scr_clock = nullptr;
static lv_obj_t* g_scr_logs  = nullptr;
static lv_obj_t* g_scr_settings = nullptr;

static ScreenId currentScreen = ScreenId::SCREEN_MAIN;
static bool needUpdateScreen  = true;
static ScreenId desiredUiScreen = ScreenId::SCREEN_MAIN;

static ScreenId g_pendingScreen = ScreenId::SCREEN_MAIN;
static bool g_havePendingAnim = false;
static lv_scr_load_anim_t g_pendingAnim = LV_SCR_LOAD_ANIM_NONE;
static uint32_t g_pendingAnimTime = app_constants::default_animation_ms;
static uint32_t g_pendingAnimDelay = 0;

static lv_obj_t* screenById(ScreenId id) {
  switch (id) {
    case ScreenId::SCREEN_MAIN:     return g_scr_main;
    case ScreenId::SCREEN_CLOCK:    return g_scr_clock;
    case ScreenId::SCREEN_LOGS:     return g_scr_logs;
    case ScreenId::SCREEN_SETTINGS: return g_scr_settings;
    default:                        return g_scr_main;
  }
}

static void requestScreen(ScreenId id, lv_scr_load_anim_t anim, uint32_t time=app_constants::default_animation_ms, uint32_t delay=0) {
  g_pendingScreen = id;
  g_pendingAnim = anim;
  g_pendingAnimTime = time;
  g_pendingAnimDelay = delay;
  g_havePendingAnim = (anim != LV_SCR_LOAD_ANIM_NONE);
  needUpdateScreen = true;
}

static ScreenId prevScreenBeforeLogs = ScreenId::SCREEN_MAIN;

static void enterLogs() {
  if (currentScreen == ScreenId::SCREEN_LOGS) return;
  prevScreenBeforeLogs = currentScreen;
  currentScreen = ScreenId::SCREEN_LOGS;
  requestScreen(currentScreen, LV_SCR_LOAD_ANIM_NONE);
  needUpdateScreen = true;
  ui_logs_refresh();
}

static void exitLogs() {
  if (currentScreen != ScreenId::SCREEN_LOGS) return;
  currentScreen = prevScreenBeforeLogs;
  requestScreen(currentScreen, LV_SCR_LOAD_ANIM_NONE);
  needUpdateScreen = true;
}

// В режиме портала смена экрана запрашивает перезагрузку для выхода из настроек.
static void changeScreen() {
  if (portal.active()) {
    g_rebootRequested = true;
    return;
  }
  if (currentScreen == ScreenId::SCREEN_MAIN) {
    currentScreen = ScreenId::SCREEN_CLOCK;
    requestScreen(currentScreen, LV_SCR_LOAD_ANIM_MOVE_LEFT, app_constants::screen_animation_ms);
  } else {
    currentScreen = ScreenId::SCREEN_MAIN;
    requestScreen(currentScreen, LV_SCR_LOAD_ANIM_MOVE_RIGHT, app_constants::screen_animation_ms);
  }
}

// AP+STA сохраняет соединение с Nightscout, пока открыты настройки.
static void enterSettings() {
  currentScreen = ScreenId::SCREEN_SETTINGS;
  backlight.setSettingsBoost(true);
  portal.begin(&g_logs, &settingsStore, &appCfg);
  needUpdateScreen = true;
}

// Сохраняется выбранная яркость, а не временные 100% во время тревоги.
static void changeBacklight() {
  Backlight::nextStatic();
  uint8_t p = Backlight::currentPercentStatic();
  brOverlay.show(p);
  appCfg.brightness = p;
  if (!settingsStore.saveBrightness(p)) logs::logE("[SYS] Failed to save brightness to NVS");
  needUpdateScreen = true;
}

#if defined(BOARD_M5CORE_S3) || defined(BOARD_TDISPLAY_S3)
static AlarmState alarmState;
static MissingDataAlarm missingDataAlarm;
static SnoozeLan snoozeLan;
static void tickAlarm();
static void publishAlarm(uint32_t now);
static uint64_t snoozeEpoch(uint32_t now);

static void cycleAlarmSnooze() {
  tickAlarm();
  if (portal.active() || !alarmState.enabled()) return;
  const uint32_t now = millis();
  alarmState.cycle_snooze(now);
  snoozeLan.announce(alarmState, snoozeEpoch(now), now);
  publishAlarm(now);
  logs::logIf("[SNOOZE] %u min", unsigned(alarmState.snooze_minutes()));
}

#endif

#if defined(BOARD_M5CORE_S3)
static void onTouchAction(ui_navigation::Action action) {
  switch (action) {
    case ui_navigation::Action::CycleAlarmSnooze:
      cycleAlarmSnooze();
      break;
    case ui_navigation::Action::SwitchLeft:
    case ui_navigation::Action::SwitchRight:
      if (portal.active() || (currentScreen != ScreenId::SCREEN_MAIN
          && currentScreen != ScreenId::SCREEN_CLOCK)) return;
      currentScreen = currentScreen == ScreenId::SCREEN_MAIN
          ? ScreenId::SCREEN_CLOCK : ScreenId::SCREEN_MAIN;
      requestScreen(currentScreen, action == ui_navigation::Action::SwitchLeft
          ? LV_SCR_LOAD_ANIM_MOVE_LEFT : LV_SCR_LOAD_ANIM_MOVE_RIGHT, app_constants::screen_animation_ms);
      break;
    case ui_navigation::Action::ScrollUp:
      if (currentScreen == ScreenId::SCREEN_LOGS) ui_logs_scroll_page(+1);
      break;
    case ui_navigation::Action::ScrollDown:
      if (currentScreen == ScreenId::SCREEN_LOGS) ui_logs_scroll_page(-1);
      break;
  }
}

// POWER: нажатие — яркость, двойное — журнал, удержание — настройки.
static void tickCoreButton() {
  using Event = gfx::Display::button_event;
  const auto event = gfx::Display::instance().take_button_event();
  if (portal.active()) {
    if (event != Event::none) g_rebootRequested = true;
    return;
  }
  switch (event) {
    case Event::click: changeBacklight(); break;
    case Event::double_click:
      if (currentScreen == ScreenId::SCREEN_LOGS) exitLogs();
      else enterLogs();
      break;
    case Event::hold:
      enterSettings();
      break;
    case Event::none: break;
  }
}
#endif

#if defined(BOARD_TDISPLAY_S3)

static bool g_bothHoldLatched = false;
static uint32_t g_lastScrollMs = 0;

// В журнале кнопки прокручивают записи; удержание обеих открывает журнал.
static void tickButtons() {
  const uint32_t now = millis();
  bool uTick = btnUser.tick();
  bool bTick = btnBoot.tick();
  // Реагируем на новое нажатие до обработки комбинаций; удержание входа не перезапускает устройство.
  if (portal.active()) {
    if (btnUser.press() || btnBoot.press()) g_rebootRequested = true;
    return;
  }
  btnBoth.tick(btnBoot, btnUser);

  if (btnBoth.hold()) {
    if (!g_bothHoldLatched) {
      g_bothHoldLatched = true;
      enterLogs();
    }
    return;
  }

  if (btnBoth.release() || btnBoth.releaseHold() || btnBoth.releaseStep()) {
    g_bothHoldLatched = false;
  }

  if (btnBoth.click()) {
    if (currentScreen == ScreenId::SCREEN_LOGS) {
      exitLogs();
      return;
    }
  }

  if (currentScreen == ScreenId::SCREEN_LOGS) {
    if (now - g_lastScrollMs < app_constants::log_scroll_ms) return;
    bool scrolled = false;

    if (uTick && btnUser.step()) { ui_logs_scroll(-1); scrolled = true; }
    else if (bTick && btnBoot.step()) { ui_logs_scroll(+1); scrolled = true; }

    if (!scrolled) {
      if (uTick && btnUser.click()) { ui_logs_scroll(-1); scrolled = true; }
      else if (bTick && btnBoot.click()) { ui_logs_scroll(+1); scrolled = true; }
    }

    if (scrolled) g_lastScrollMs = now;
    return;
  }

  // GPIO14: короткое нажатие — отсрочка, удержание — смена основного экрана.
  if (uTick) {

    if (btnUser.click()) {
      logs::logI("[BUTTON] GPIO14: cycle snooze");
      cycleAlarmSnooze();
    }
    if (btnUser.hold()) { changeScreen(); }
  }

  // GPIO0: короткое нажатие — яркость, удержание — настройки.
  if (bTick) {

    if (btnBoot.click()) {
      logs::logI("[BUTTON] GPIO0: change brightness");
      changeBacklight();
    }
    if (btnBoot.hold() && !portal.active()) { enterSettings(); }
  }
}
#endif

static void initWifi() {
#if defined(BOARD_M5CORE_S3)
  wifiCfg.hostname = app_constants::wifi_cores3_hostname;
#else
  wifiCfg.hostname = app_constants::wifi_tdisplay_hostname;
#endif

  wifiMgr.setConfig(wifiCfg);
  wifiMgr.setLogger(&g_logs);

  logs::logIf("[Wi-Fi] Starting, hostname: %s", wifiCfg.hostname ? wifiCfg.hostname : "(none)");

  WifiCred list[MAX_WIFI];
  size_t cnt = 0;
  for (int i = 0; i < MAX_WIFI; i++) {
    if (appCfg.wifi[i].ssid[0] == 0) continue;
    if (cnt < MAX_WIFI) {
      list[cnt].ssid = appCfg.wifi[i].ssid;
      list[cnt].pass = appCfg.wifi[i].pass;
      cnt++;
    }
  }

  if (cnt == 0) {
    logs::logE("[Wi-Fi] No saved networks");
    bootWaitNsSecond = true;
    currentScreen = ScreenId::SCREEN_LOGS;
    needUpdateScreen = true;
    return;
  }

  wifiMgr.addNetworks(list, cnt);

  bool wifiOk = wifiMgr.begin();
  logs::logIf("[Wi-Fi] Connection start: %s", wifiOk ? "OK" : "failed");

  currentScreen = ScreenId::SCREEN_LOGS;
  needUpdateScreen = true;

  bootWaitNsSecond = true;

  applyTimeConfig(appCfg.tzMinutes, appCfg.dstEnable);

  NSConfig c;
  c.batterySource = appCfg.batterySource;
  c.baseUrl       = String(appCfg.hostSite);
  c.token         = String(appCfg.nsToken);

  for (int i = 0; i < 4; ++i) c.sgvBounds[i] = appCfg.sgvBounds[i];

  g_ns.begin(c);

  g_ns.requestUpdate();

  (void)wifiOk;
}

static int32_t computeAgeSec(uint64_t update_ms);

// Опрос сервера и обновление экрана имеют независимые интервалы.
static void tickNightscout() {
  static uint32_t lastTickMs = 0;

  const uint32_t nowMs = millis();

  if (!g_ns.isBusy() && !g_ns.isUpdateWanted()) {
    const auto& data = haveLastGood ? lastGood : nsData;
    const GlucoseZone zone = glucose_zone(data.sgv_mmol, appCfg.sgvBounds);
    const int32_t ageSec = computeAgeSec(data.update_time_ms);
    if (nightscout_poll::due(nowMs, g_ns.lastRequestAtMs(), data.update_time_ms > 0,
                            zone, ageSec)) {
      logs::logIf("[SYS] %s, age=%d s: requesting data",
                  nightscout_poll::warning_zone(zone) ? "Yellow/red zone" : "Stale data", (int)ageSec);
      g_ns.requestUpdate();
    }
  }

  if (nowMs - lastTickMs >= app_constants::ui_refresh_ms) {
    lastTickMs = nowMs;
    if (currentScreen == ScreenId::SCREEN_MAIN || currentScreen == ScreenId::SCREEN_CLOCK) {
      needUpdateScreen = true;
    }
  }

  bool nsChanged = g_ns.tick(nsData);
  if (nsChanged) {
    needUpdateScreen = true;
    if (nsData.ok) {
      lastGood = nsData;
      haveLastGood = true;
    }
  }

#if defined(BOARD_TDISPLAY_S3) || defined(BOARD_M5CORE_S3)
  nsAlarmSnapshot=g_ns.thresholds();
  if (ns_alarm::server_bounds_ready(nsAlarmSnapshot,appCfg.hostSite)
      && nsImportedRevision!=nsAlarmSnapshot.revision) {
    nsImportedRevision=nsAlarmSnapshot.revision;
    const bool changed=memcmp(appCfg.sgvBounds,nsAlarmSnapshot.bounds,sizeof(appCfg.sgvBounds))!=0
        || strcmp(appCfg.boundsOrigin,nsAlarmSnapshot.origin)!=0;
    memcpy(appCfg.sgvBounds,nsAlarmSnapshot.bounds,sizeof(appCfg.sgvBounds));
    strlcpy(appCfg.boundsOrigin,nsAlarmSnapshot.origin,sizeof(appCfg.boundsOrigin));
    if (changed && !settingsStore.saveBounds(appCfg)) logs::logE("[NS ALARM] Failed to save thresholds to NVS");
    if (changed) logs::logIf("[NS ALARM] Server thresholds: %.1f / %.1f / %.1f / %.1f mmol/L",
        appCfg.sgvBounds[0],appCfg.sgvBounds[1],appCfg.sgvBounds[2],appCfg.sgvBounds[3]);
    needUpdateScreen=true;
  }
#endif

  if (bootWaitNsSecond && nsChanged && nsData.ok && !portal.active()) {
    bootWaitNsSecond = false;
    currentScreen = desiredUiScreen;
    needUpdateScreen = true;
    logs::logI("[SYS] Nightscout connected");
  }
}

// Периодическая синхронизация времени при доступном Wi-Fi.
static void tickTimeResync() {
  if (WiFi.status() != WL_CONNECTED) return;
  time_t now = time(nullptr);
  const bool valid = (now > app_constants::valid_epoch_seconds);

  if (valid && !g_ntpWasValid) {
    g_ntpWasValid = true;
    logs::logI("[TIME] NTP synchronized");
    lastNtpSync = now;
  }

  if (valid && lastNtpSync != 0 && (now - lastNtpSync) >= static_cast<time_t>(app_constants::ntp_resync_seconds)) {
    logs::logI("[TIME] Daily NTP resync");
    applyTimeConfig(appCfg.tzMinutes, appCfg.dstEnable);
    time_t now2 = time(nullptr);
    if (now2 > app_constants::valid_epoch_seconds) lastNtpSync = now2;
    else lastNtpSync = now;
  }
}

static void tickPortal() {

  portal.tick();
#if defined(BOARD_TDISPLAY_S3) || defined(BOARD_M5CORE_S3)
  wifiMgr.tick();
  tickNightscout();
#endif

  static uint32_t lastKick = 0;
  uint32_t nowMs = millis();
  if (nowMs - lastKick > app_constants::portal_refresh_ms) {
    needUpdateScreen = true;
    lastKick = nowMs;
  }

  if (g_rebootRequested) {
    g_rebootRequested = false;
    portal.scheduleRestart();
  }
}

#include "nvs_flash.h"
#include "esp_err.h"

static void initSettingsSafe() {
  logs::logI("[SYS] Loading settings");

  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    logs::logW("[SYS] Resetting NVS");
    nvs_flash_erase();
    err = nvs_flash_init();
  }

  if (err != ESP_OK) {
    logs::logEf("[SYS] NVS initialization error: %s", esp_err_to_name(err));
  } else {
    logs::logI("[SYS] NVS ready");
  }

  bool ok = settingsStore.begin();
  if (!ok) {
    logs::logW("[SYS] Settings load failed: using defaults");
    appCfg = AppSettings();
    return;
  }

  settingsStore.load(appCfg);
  logs::logI("[SYS] Settings loaded");
  Serial.printf("[HEAP] Free after loading settings: %u bytes\n", (unsigned)ESP.getFreeHeap());
}

void setup() {
#if defined(BOARD_TDISPLAY_S3)
  // Инициализируем GPIO-зуммер до запуска дисплея и сети.
  buzzer::begin();
#endif
  Serial.begin(app_constants::serial_baud);

  gfx::LVGL_Display::instance().init();

  g_scr_logs  = ui_logs_create();
  ui_logs_set_buffer(&g_logs);

  logs::bind(&g_logs, true);
  logs::logI("[SYS] Boot");

  g_scr_main  = ui_main_create();
  g_scr_clock = ui_clock_create();
  g_scr_settings = ui_settings_create();

  brOverlay.init(lv_layer_top());

  lv_screen_load(g_scr_logs);
  gfx::LVGL_Display::instance().loop();

  initSettingsSafe();

  Backlight::Use(backlight);
  Backlight::beginStatic();
  backlight.restoreFromPercent(appCfg.brightness);

#if defined(BOARD_M5CORE_S3)

  ui_navigation::init(onTouchAction);
  ui_navigation::update(true, false);
  gfx::Display::instance().reset_button_events();
#endif

#if defined(BOARD_TDISPLAY_S3)
  ui_navigation::init(nullptr);
  ui_navigation::update(true, false);
#endif

  if (!appCfg.onboardingDone) {
    enterSettings();
    return;
  }

  currentScreen = ScreenId::SCREEN_LOGS;
  needUpdateScreen = true;

  logs::logI("[Wi-Fi] Connecting");
  initWifi();
}

// Неизвестное время или метка из будущего возвращают INT32_MAX.
static int32_t computeAgeSec(uint64_t update_ms) {
  time_t nowSec = time(nullptr);
#if defined(BOARD_TDISPLAY_S3) || defined(BOARD_M5CORE_S3)
  if (nowSec<=app_constants::valid_epoch_seconds) nowSec=ns_alarm::server_now(nsAlarmSnapshot,millis())/1000;
#endif
  if (nowSec <= app_constants::valid_epoch_seconds || update_ms == 0) return INT32_MAX;
  time_t sensTime = (time_t)(update_ms / 1000ULL);
  if (sensTime <= 0 || nowSec < sensTime) return INT32_MAX;
  return (int32_t)(nowSec - sensTime);
}

#if defined(BOARD_M5CORE_S3) || defined(BOARD_TDISPLAY_S3)

static uint64_t snoozeEpoch(uint32_t now) {
  const time_t epoch = time(nullptr);
  if (epoch > app_constants::valid_epoch_seconds) return uint64_t(epoch);
  return ns_alarm::server_now(nsAlarmSnapshot, now) / 1000;
}

// Единственная настройка звука — громкость; Snooze подавляет оба условия тревоги.
static void publishAlarm(uint32_t now) {
  const uint8_t severity=ns_alarm::level(nsAlarmKind);
  const uint32_t remaining=alarmState.remaining_seconds(now);
  const bool enabled=appCfg.alarmVolume>0;
  const bool sounding=alarmState.should_sound(now);
  backlight.setAlarmBoost(sounding);
  auto& display = gfx::Display::instance();
  display.set_alarm_volume(appCfg.alarmVolume);
#if defined(BOARD_TDISPLAY_S3)
  buzzer::setAlarm(sounding, severity==2);
#else
  display.set_alarm_sound(sounding);
#endif
  const auto& data=haveLastGood?lastGood:nsData;
  const int32_t age=computeAgeSec(data.update_time_ms);
  ui_navigation::set_alarm_status(enabled,
      data.ok && age>=0 && age<ns_alarm::max_age_seconds, sounding, display.alarm_sound_available(),
      remaining, alarmState.snooze_minutes(), alarmState.data_missing(), severity==2);
  static bool audioErrorLogged = false;
  if (enabled && !display.alarm_sound_available() && !audioErrorLogged) {
    logs::logE("[ALARM] Audio device unavailable");
    audioErrorLogged = true;
  }
}

// Красные зоны по свежим данным либо 15 минут без измерений, независимо от флагов Nightscout.
static void tickAlarm() {
  const auto& data = haveLastGood ? lastGood : nsData;
  const int32_t age = computeAgeSec(data.update_time_ms);
  const auto previous=nsAlarmKind;
  nsAlarmKind=data.ok ? ns_alarm::classify(data.sgv_mmol,age,appCfg.sgvBounds) : ns_alarm::Kind::None;
  static const char* const names[]={"none", "critical low", "low", "high", "critical high"};
  if (nsAlarmKind!=previous) logs::logIf("[NS ALARM] %s; glucose=%.1f, age=%ld s",
      names[unsigned(nsAlarmKind)],data.sgv_mmol,(long)age);
  const uint32_t now = millis();
  const bool missing=missingDataAlarm.update(now,data.ok ? age : INT32_MAX);
  const bool enabled=appCfg.alarmVolume>0;
  alarmState.update(now, enabled, data.ok && age >= 0 && age < ns_alarm::max_age_seconds,
                    nsAlarmKind != ns_alarm::Kind::None, enabled, missing);
  snoozeLan.tick(alarmState, appCfg.hostSite, snoozeEpoch(now), now);
  publishAlarm(now);
}
#endif

void loop() {
  backlight.setSettingsBoost(portal.active());

#if defined(BOARD_M5CORE_S3) || defined(BOARD_TDISPLAY_S3)
  tickAlarm();
#endif
#if defined(BOARD_M5CORE_S3)
  tickCoreButton();
  ui_navigation::update(currentScreen == ScreenId::SCREEN_LOGS, portal.active());
  ui_navigation::poll();
#endif

  #if defined(BOARD_TDISPLAY_S3)
  tickButtons();
  ui_navigation::update(currentScreen == ScreenId::SCREEN_LOGS, portal.active());
  #endif

  if (portal.active()) {
    if (needUpdateScreen) {

      if (currentScreen != ScreenId::SCREEN_LOGS) currentScreen = ScreenId::SCREEN_SETTINGS;
      lv_obj_t* target = screenById(currentScreen);
      if (lv_screen_active() != target) lv_screen_load(target);
      needUpdateScreen = false;
      if (currentScreen == ScreenId::SCREEN_SETTINGS) ui_settings_update_from_portal(portal);
    }
    tickPortal();
#if defined(BOARD_M5CORE_S3) || defined(BOARD_TDISPLAY_S3)
    tickAlarm();
#endif
    gfx::LVGL_Display::instance().loop();
    vTaskDelay(1); // Отдаём процессор задачам сети и idle; время LVGL задаёт его tick callback.
    return;
  }

  wifiMgr.tick();

  tickNightscout();
#if defined(BOARD_M5CORE_S3) || defined(BOARD_TDISPLAY_S3)
  tickAlarm();
#endif

  if (needUpdateScreen) {
    lv_obj_t* target = screenById(currentScreen);
    if (lv_scr_act() != target) {
      if (g_havePendingAnim) {
        lv_scr_load_anim(target, g_pendingAnim, g_pendingAnimTime, g_pendingAnimDelay, false);
      } else {
        lv_screen_load(target);
      }
    }

    NightscoutData uiData = haveLastGood ? lastGood : nsData;
    memcpy(uiData.sgvBounds,appCfg.sgvBounds,sizeof(uiData.sgvBounds));

    const int32_t ageSec = computeAgeSec(uiData.update_time_ms);
    uiData.ageSec  = ageSec;
    uiData.stale5  = (ageSec != INT32_MAX && ageSec >= app_constants::fresh_age_minutes * 60);
    uiData.stale15 = (ageSec != INT32_MAX && ageSec >= app_constants::data_expiry_seconds);

    if (uiData.stale15) {
      uiData.sgv_mmol = NAN;
      uiData.delta_mmol = NAN;
      uiData.iob = NAN;
      uiData.cob = NAN;
      uiData.batteryPct = -1;
      uiData.pumpBatV = 0.0f;
      strlcpy(uiData.trend, "N/A", sizeof(uiData.trend));
    }

    if (currentScreen == ScreenId::SCREEN_MAIN) {
      main_update_full(uiData);
    } else if (currentScreen == ScreenId::SCREEN_CLOCK) {
      clock_update_full(uiData);
    } else if (currentScreen == ScreenId::SCREEN_SETTINGS) {

    } else if (currentScreen == ScreenId::SCREEN_LOGS) {

    }

    g_havePendingAnim = false;
    g_pendingAnim = LV_SCR_LOAD_ANIM_NONE;
    needUpdateScreen = false;
  }

  tickTimeResync();

  gfx::LVGL_Display::instance().loop();
  vTaskDelay(1);
}
