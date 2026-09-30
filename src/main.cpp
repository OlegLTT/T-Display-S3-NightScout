// Запуск устройства и общий цикл: экраны, кнопки, сеть, тревога и подсветка.
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
#include "nightscout_poll.h"
#include "backlight.h"
#include "ui_overlay.h"
#include "logs.h"
#include "wifi_manager.h"
#include "alarm_state.h"
#include "glucose_zone.h"

#if defined(BOARD_TDISPLAY_S3)
  #define UB_DEB_TIME 50
  #define UB_HOLD_TIME 600
  #define UB_STEP_TIME 400
  #define UB_STEP_PRD 100
  #define UB_CLICK_TIME 500
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

static NightscoutClient g_ns;
static NightscoutData   nsData;

// Последний успешный ответ остаётся на экране при временных ошибках сети.
static NightscoutData   lastGood;
static bool             haveLastGood = false;

static constexpr uint32_t UI_REFRESH_INTERVAL_MS = 15000;

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
static constexpr time_t NTP_SYNC_PERIOD = 24 * 60 * 60;

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
static uint32_t g_pendingAnimTime = 250;
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

static void requestScreen(ScreenId id, lv_scr_load_anim_t anim, uint32_t time=250, uint32_t delay=0) {
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
    requestScreen(currentScreen, LV_SCR_LOAD_ANIM_MOVE_LEFT, 220);
  } else {
    currentScreen = ScreenId::SCREEN_MAIN;
    requestScreen(currentScreen, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 220);
  }
}

// Переход в режим точки доступа отключает обычное подключение Wi-Fi.
static void enterSettings() {
  currentScreen = ScreenId::SCREEN_SETTINGS;
  WiFi.disconnect(true);
  portal.begin(&g_logs, &settingsStore, &appCfg);
  needUpdateScreen = true;
}

// Сохраняется выбранная яркость, а не временные 100% во время тревоги.
static void changeBacklight() {
  Backlight::nextStatic();
  uint8_t p = Backlight::currentPercentStatic();
  brOverlay.show(p, 2000);
  appCfg.brightness = p;
  settingsStore.save(appCfg);
  needUpdateScreen = true;
}

#if defined(BOARD_M5CORE_S3) || defined(BOARD_TDISPLAY_S3)
static AlarmState alarmState;
static MissingDataAlarm missingDataAlarm;
static void tickAlarm();
static void publishAlarm(uint32_t now);

#endif

#if defined(BOARD_M5CORE_S3)
static void onTouchAction(ui_navigation::Action action) {
  switch (action) {
    case ui_navigation::Action::CycleAlarmSnooze: {
      tickAlarm();
      const uint32_t now = millis();
      alarmState.cycle_snooze(now);
      publishAlarm(now);
      break;
    }
    case ui_navigation::Action::SwitchLeft:
    case ui_navigation::Action::SwitchRight:
      if (portal.active() || (currentScreen != ScreenId::SCREEN_MAIN
          && currentScreen != ScreenId::SCREEN_CLOCK)) return;
      currentScreen = currentScreen == ScreenId::SCREEN_MAIN
          ? ScreenId::SCREEN_CLOCK : ScreenId::SCREEN_MAIN;
      requestScreen(currentScreen, action == ui_navigation::Action::SwitchLeft
          ? LV_SCR_LOAD_ANIM_MOVE_LEFT : LV_SCR_LOAD_ANIM_MOVE_RIGHT, 220);
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
  switch (gfx::Display::instance().take_button_event()) {
    case Event::click: changeBacklight(); break;
    case Event::double_click:
      if (currentScreen == ScreenId::SCREEN_LOGS) exitLogs();
      else enterLogs();
      break;
    case Event::hold:
      if (!portal.active()) enterSettings();
      else {
        currentScreen = ScreenId::SCREEN_SETTINGS;
        requestScreen(currentScreen, LV_SCR_LOAD_ANIM_NONE);
      }
      break;
    case Event::none: break;
  }
}
#endif

#if defined(BOARD_TDISPLAY_S3)

static bool g_bothHoldLatched = false;
static uint32_t g_lastScrollMs = 0;
static constexpr uint32_t SCROLL_PERIOD_MS = 90;

static void cycleTDisplayAlarmSnooze() {
  tickAlarm();
  const uint32_t now = millis();
  if (!alarmState.enabled()) {
    logs::logW("[BUTTON] snooze ignored: both alarms disabled");
    return;
  }
  alarmState.cycle_snooze(now);
  publishAlarm(now);
  logs::logIf("[BUTTON] snooze=%u min, remaining=%lu sec",
      unsigned(alarmState.snooze_minutes()),
      (unsigned long)alarmState.remaining_seconds(now));
}

// В журнале кнопки прокручивают записи; удержание обеих открывает журнал.
static void tickButtons() {
  const uint32_t now = millis();
  bool uTick = btnUser.tick();
  bool bTick = btnBoot.tick();
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
    if (now - g_lastScrollMs < SCROLL_PERIOD_MS) return;
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
      logs::logI("[BUTTON] GPIO14 click: snooze");
      cycleTDisplayAlarmSnooze();
    }
    if (btnUser.hold()) changeScreen();
  }

  // GPIO0: короткое нажатие — яркость, удержание — настройки.
  if (bTick) {

    if (btnBoot.click()) {
      logs::logI("[BUTTON] GPIO0 click: brightness");
      changeBacklight();
    }
    if (btnBoot.hold() && !portal.active()) enterSettings();
  }
}
#endif

static void initWifi() {
#if defined(BOARD_M5CORE_S3)
  wifiCfg.hostname = "m5cores3";
#else
  wifiCfg.hostname = "t-display-s3";
#endif
  wifiCfg.connectTimeoutMs = 20000;
  wifiCfg.retryDelayMs = 500;
  wifiCfg.enableAutoReconnect = true;

  wifiCfg.beginAttempts = 3;
  wifiCfg.reconnectPeriodMs = 3000;
  wifiCfg.reconnectTryWindowMs = 8000;

  wifiMgr.setConfig(wifiCfg);
  wifiMgr.setLogger(&g_logs);

  logs::logIf("[WIFI] begin hostname=%s", wifiCfg.hostname ? wifiCfg.hostname : "(null)");

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
    logs::logE("[WIFI] no networks configured");
    bootWaitNsSecond = true;
    currentScreen = ScreenId::SCREEN_LOGS;
    needUpdateScreen = true;
    return;
  }

  wifiMgr.addNetworks(list, cnt);

  bool wifiOk = wifiMgr.begin();
  logs::logIf("[WIFI] begin ok=%d", (int)wifiOk);

  currentScreen = ScreenId::SCREEN_LOGS;
  needUpdateScreen = true;

  bootWaitNsSecond = true;

  applyTimeConfig(appCfg.tzMinutes, appCfg.dstEnable);

  NSConfig c;
  c.baseUrl       = String(appCfg.hostSite);
  c.token         = String(appCfg.nsToken);
  c.httpTimeoutMs = 15000;
  c.tlsInsecure   = true;
  for (int i = 0; i < 4; ++i) c.sgvBounds[i] = appCfg.sgvBounds[i];

  c.retryDelayMs = 1500;
  c.warnRssiDbm  = -78;

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
      logs::logIf("[SYS] %s age=%ds -> request update",
                  nightscout_poll::warning_zone(zone) ? "yellow/red" : "stale", (int)ageSec);
      g_ns.requestUpdate();
    }
  }

  if (nowMs - lastTickMs >= UI_REFRESH_INTERVAL_MS) {
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

  if (bootWaitNsSecond && nsChanged && nsData.ok) {
    bootWaitNsSecond = false;
    currentScreen = desiredUiScreen;
    needUpdateScreen = true;
    logs::logI("[SYS] NS connected");
  }
}

// Периодическая синхронизация времени при доступном Wi-Fi.
static void tickTimeResync() {
  if (WiFi.status() != WL_CONNECTED) return;
  time_t now = time(nullptr);
  const bool valid = (now > 100000);

  if (valid && !g_ntpWasValid) {
    g_ntpWasValid = true;
    logs::logI("[TIME] NTP synced");
    lastNtpSync = now;
  }

  if (valid && lastNtpSync != 0 && (now - lastNtpSync) >= NTP_SYNC_PERIOD) {
    logs::logI("[TIME] Daily NTP resync");
    applyTimeConfig(appCfg.tzMinutes, appCfg.dstEnable);
    time_t now2 = time(nullptr);
    if (now2 > 100000) lastNtpSync = now2;
    else lastNtpSync = now;
  }
}

static void tickPortal() {

  portal.tick();

  static uint32_t lastKick = 0;
  uint32_t nowMs = millis();
  if (nowMs - lastKick > 500) {
    needUpdateScreen = true;
    lastKick = nowMs;
  }

  if (g_rebootRequested) {
    g_rebootRequested = false;
    delay(150);
    ESP.restart();
  }
}

#include "nvs_flash.h"
#include "esp_err.h"

static void initSettingsSafe() {
  delay(1500);
  logs::logI("[SYS] init settings");

  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    logs::logW("[SYS] NVS reset");
    nvs_flash_erase();
    err = nvs_flash_init();
  }

  if (err != ESP_OK) {
    logs::logEf("[SYS] NVS init error=%s", esp_err_to_name(err));
  } else {
    logs::logI("[SYS] NVS init OK");
  }

  bool ok = settingsStore.begin();
  if (!ok) {
    logs::logW("[SYS] settings load failed -> defaults");
    appCfg = AppSettings();
    return;
  }

  settingsStore.load(appCfg);
  logs::logI("[SYS] settings loaded");
  Serial.printf("[HEAP] after settings load: %u\n", (unsigned)ESP.getFreeHeap());
}

void setup() {

  Serial.begin(115200);

  gfx::LVGL_Display::instance().init();

  g_scr_logs  = ui_logs_create();
  ui_logs_set_buffer(&g_logs);

  logs::bind(&g_logs, true);
  logs::logI("[SYS] boot");

  g_scr_main  = ui_main_create();
  g_scr_clock = ui_clock_create();
  g_scr_settings = ui_settings_create();

  brOverlay.init(lv_layer_top());

  lv_screen_load(g_scr_logs);
  for (int i = 0; i < 10; i++) {
    gfx::LVGL_Display::instance().loop();
    delay(5);
  }

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

  logs::logI("[WIFI] connecting");
  initWifi();
}

static uint32_t last_ms = 0;

// Неизвестное время или метка из будущего возвращают INT32_MAX.
static int32_t computeAgeSec(uint64_t update_ms) {
  time_t nowSec = time(nullptr);
  if (nowSec <= 100000 || update_ms == 0) return INT32_MAX;
  time_t sensTime = (time_t)(update_ms / 1000ULL);
  if (sensTime <= 0 || nowSec < sensTime) return INT32_MAX;
  return (int32_t)(nowSec - sensTime);
}

#if defined(BOARD_M5CORE_S3) || defined(BOARD_TDISPLAY_S3)

// Реальная тревога управляет подсветкой; тест звука не изменяет отсрочку.
static void publishAlarm(uint32_t now) {
  backlight.setAlarmBoost(alarmState.should_sound(now));
  auto& display = gfx::Display::instance();
  static bool testing = false;
  static uint32_t testStarted = 0;
  static uint8_t testVolume = 0;
  if (portal.takeSoundTest(testVolume)) {
    display.set_alarm_sound(false);
    testing = true;
    testStarted = now;
  }

  // После теста возвращается сохранённая громкость и обычное состояние тревоги.
  if (testing && (!portal.active() || uint32_t(now - testStarted) >= 600)) {
    display.set_alarm_sound(false);
    testing = false;
  }
  display.set_alarm_volume(testing ? testVolume : appCfg.alarmVolume);
  display.set_alarm_sound(testing || alarmState.should_sound(now));
  ui_navigation::set_alarm_status(alarmState.enabled(), alarmState.data_valid(),
      alarmState.should_sound(now), display.alarm_sound_available(),
      alarmState.remaining_seconds(now), alarmState.snooze_minutes(), alarmState.data_missing());
  static bool audioErrorLogged = false;
  if (alarmState.enabled() && !display.alarm_sound_available() && !audioErrorLogged) {
    logs::logE("[ALARM] speaker unavailable");
    audioErrorLogged = true;
  }
}

// Одна настройка включает красную зону и отсутствие свежих данных 15 минут.
static void tickAlarm() {
  const auto& data = haveLastGood ? lastGood : nsData;
  const int32_t age = computeAgeSec(data.update_time_ms);
  const GlucoseZone zone = glucose_zone(data.sgv_mmol, appCfg.sgvBounds);
  const bool valid = data.ok && age >= 0 && age < 15 * 60 && zone != GlucoseZone::Invalid;
  const uint32_t now = millis();
  const bool missing = missingDataAlarm.update(now, data.ok ? age : INT32_MAX);
  alarmState.update(now, appCfg.soundAlarmEnabled, valid, glucose_zone_is_red(zone),
                    appCfg.soundAlarmEnabled, missing);
  publishAlarm(now);
}
#endif

void loop() {

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
    delay(1);
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

    const int32_t ageSec = computeAgeSec(uiData.update_time_ms);
    uiData.ageSec  = ageSec;
    uiData.stale5  = (ageSec != INT32_MAX && ageSec >= 5 * 60);
    uiData.stale15 = (ageSec != INT32_MAX && ageSec >= 15 * 60);

    if (uiData.stale15) {
      uiData.sgv_mmol = NAN;
      uiData.delta_mmol = NAN;
      uiData.iob = NAN;
      uiData.cob = NAN;
      uiData.uploaderBatPct = -1;
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

  uint32_t now = millis();
  uint32_t diff = now - last_ms;
  if (diff) {
    last_ms = now;
    lv_tick_inc(diff);
  }

  gfx::LVGL_Display::instance().loop();
  delay(1);
}
