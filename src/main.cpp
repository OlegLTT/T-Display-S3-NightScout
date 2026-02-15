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
#include "ui_main.h"
#include "ui_logs.h"
#include "ui_clock.h"
#include "ui_first.h"
#include "ui_settings.h"

#include "nightscout_client.h"
#include "backlight.h"
#include "ui_overlay.h"
#include "logs.h"
#include "wifi_manager.h"

#if defined(BOARD_TDISPLAY_S3)
  #define UB_DEB_TIME 50      // дебаунс
  #define UB_HOLD_TIME 600    // время до перехода в состояние "удержание"
  #define UB_STEP_TIME 400    // время до перехода в состояние "импульсное удержание"
  #define UB_STEP_PRD 100     // период импульсов
  #define UB_CLICK_TIME 500   // ожидание кликов
  #include <uButton.h>
  #include <uButtonMulti.h>
#endif

// LVGL display port (как у тебя)
#if defined(BOARD_ESP32S3_DEV)
  #include "lcd_bsp.h"
  #include "FT3168.h"
#endif

// ======================================================
//                     SETTINGS / PORTAL
// ======================================================
static SettingsStore settingsStore;
AppSettings appCfg;
static CaptivePortal portal;

// ======================================================
//                         WiFi
// ======================================================
static WifiManager wifiMgr;
static WifiManagerConfig wifiCfg;

// ======================================================
//                         LOGS
// ======================================================
static LogBuffer g_logs;
static uint32_t lastLogVersion = 0;

// ======================================================
//                     Nightscout
// ======================================================
static NightscoutClient g_ns;
static NightscoutData   nsData;

static constexpr uint32_t POLL_INTERVAL = 15000;  // 15 сек
static bool bootWaitNsSecond = true;              // при старте ждём успешные Entries+Props

// ======================================================
//                 Brightness overlay + backlight
// ======================================================
static BrightnessOverlay brOverlay;
static Backlight backlight;

// ======================================================
//                         Buttons
// ======================================================
#if defined(BOARD_TDISPLAY_S3)
  static uButton btnBoot(PIN_BUTTON_A);
  static uButton btnUser(PIN_BUTTON_B);
  static uButtonMulti btnBoth;
#endif

static volatile bool g_rebootRequested = false;

// --- Clock screen: instant redraw on minute change ---
static int lastClockMin  = -1;
static int lastClockHour = -1;
static int lastClockYday = -1;

// ---- Daily NTP sync ----
static time_t lastNtpSync = 0;
static bool g_ntpWasValid = false;
static constexpr time_t NTP_SYNC_PERIOD = 24 * 60 * 60; // 24 часа

// ======================================================
//                       Screens state
// ======================================================
enum class ScreenId : uint8_t { SCREEN_FIRST, SCREEN_MAIN, SCREEN_CLOCK, SCREEN_LOGS, SCREEN_SETTINGS };
static lv_obj_t* g_scr_first = nullptr;
static lv_obj_t* g_scr_main  = nullptr;
static lv_obj_t* g_scr_clock = nullptr;
static lv_obj_t* g_scr_logs  = nullptr;
static lv_obj_t* g_scr_settings = nullptr;
static ScreenId currentScreen = ScreenId::SCREEN_MAIN;
static bool needUpdateScreen  = true;

static ScreenId desiredUiScreen = ScreenId::SCREEN_MAIN;   // что надо восстановить (0=main,1=clock)
static bool onboardingActive = false;

static ScreenId g_pendingScreen = ScreenId::SCREEN_MAIN;
static ScreenId prevScreenBeforeLogs = ScreenId::SCREEN_MAIN; 
static bool g_havePendingAnim = false;
static lv_scr_load_anim_t g_pendingAnim = LV_SCR_LOAD_ANIM_NONE;
static uint32_t g_pendingAnimTime = 250;
static uint32_t g_pendingAnimDelay = 0;

static lv_obj_t* screenById(ScreenId id) {
  switch (id) {
    case ScreenId::SCREEN_MAIN:     return g_scr_main;
    case ScreenId::SCREEN_CLOCK:    return g_scr_clock;
    case ScreenId::SCREEN_LOGS:     return g_scr_logs;
    case ScreenId::SCREEN_FIRST:    return g_scr_first;
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

// ======================================================
//                      Button callbacks
// ======================================================
static void onUserSingle() {
  if (portal.active()) {
    g_rebootRequested = true;
    return;
  }

  // logs -> main без анимации (или тоже можно)
  if (currentScreen == ScreenId::SCREEN_LOGS) {
    currentScreen = ScreenId::SCREEN_MAIN;
    requestScreen(currentScreen, LV_SCR_LOAD_ANIM_NONE);
    Serial.println("onUserSingle");
    return;
  }

  if (currentScreen == ScreenId::SCREEN_MAIN) {
    currentScreen = ScreenId::SCREEN_CLOCK;
    requestScreen(currentScreen, LV_SCR_LOAD_ANIM_MOVE_LEFT, 220);
  } else {
    currentScreen = ScreenId::SCREEN_MAIN;
    requestScreen(currentScreen, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 220);
  }

  Serial.println("onUserSingle");
}

static void onUserLong() {
  currentScreen = ScreenId::SCREEN_SETTINGS;
  WiFi.disconnect(true);
  portal.begin(&g_logs, &settingsStore, &appCfg);
  // ui_load(currentScreen);
  
  Serial.println("onUserLong");
  needUpdateScreen = true;
}

static void onBootSingle() {
  Backlight::nextStatic();
  uint8_t p = Backlight::currentPercentStatic();
  brOverlay.show(p, 2000);

  appCfg.brightness = p;
  settingsStore.save(appCfg);
  needUpdateScreen = true;
}

static void onBootLong() {
  // вход
  prevScreenBeforeLogs = currentScreen;
  currentScreen = ScreenId::SCREEN_LOGS;
  requestScreen(currentScreen, LV_SCR_LOAD_ANIM_NONE);
  needUpdateScreen = true;
}

static bool g_bothHoldLatched = false;     // чтобы enterLogs() сработал один раз
static uint32_t g_lastScrollMs = 0;        // троттлинг скролла
static constexpr uint32_t SCROLL_PERIOD_MS = 90;

static void tickButtons() {
  const uint32_t now = millis();

  // 1) тикаем одиночные кнопки
  bool uTick = btnUser.tick();
  bool bTick = btnBoot.tick();

  // 2) тикаем мульти (две кнопки одновременно)
  btnBoth.tick(btnBoot, btnUser);

  // ---- 2 кнопки вместе ----
  // Вход в LOG: обе кнопки долго (hold), но только один раз на одно удержание
  if (btnBoth.hold()) {
    if (!g_bothHoldLatched) {
      g_bothHoldLatched = true;
      enterLogs();
    }
    return; // не даём одиночным событиям срабатывать в тот же цикл
  }

  // Сбрасываем защёлку, когда комбо отпустили
  if (btnBoth.release() || btnBoth.releaseHold() || btnBoth.releaseStep()) {
    g_bothHoldLatched = false;
  }

  // Выход из LOG: обе кнопки коротко
  if (btnBoth.click()) {
    if (currentScreen == ScreenId::SCREEN_LOGS) {
      exitLogs();
      return;
    }
  }

  // ---- ЛОГ-экран: автоскролл импульсами (ТОЛЬКО step, без click) ----
  if (currentScreen == ScreenId::SCREEN_LOGS) {
    // троттлим любые скроллы
    if (now - g_lastScrollMs < SCROLL_PERIOD_MS) return;

    bool scrolled = false;

    if (uTick && btnUser.step()) { ui_logs_scroll(-1); scrolled = true; } 
    else if (bTick && btnBoot.step()) { ui_logs_scroll(+1); scrolled = true; }

    // Одношаговый click можно оставить, но тогда НЕ вместе с step.
    // Лучше так: click только если step не сработал.
    if (!scrolled) {
      if (uTick && btnUser.click()) { ui_logs_scroll(-1); scrolled = true; }
      else if (bTick && btnBoot.click()) { ui_logs_scroll(+1); scrolled = true; }
    }

    if (scrolled) g_lastScrollMs = now;
    return;
  }

  // ---- Обычный режим (НЕ логи) ----
  if (uTick) {
    if (btnUser.click()) onUserSingle();
    if (btnUser.hold())  onUserLong();
  }

  if (bTick) {
    if (btnBoot.click()) onBootSingle();
  }
}


// ======================================================
//                         initWifi
// ======================================================
static void initWifi() {
  wifiCfg.hostname = "t-display-s3";
  wifiCfg.connectTimeoutMs = 20000;
  wifiCfg.retryDelayMs = 500;
  wifiCfg.enableAutoReconnect = true;

  wifiMgr.setConfig(wifiCfg);
  wifiMgr.setLogger(&g_logs);

  logs::logIf("[WIFI] begin hostname=%s", wifiCfg.hostname ? wifiCfg.hostname : "(null)");
  // Добавляем сети из настроек
  WifiCred list[MAX_WIFI];
  size_t cnt = 0;

  for (int i = 0; i < MAX_WIFI; i++) {
    if (appCfg.wifi[i].ssid[0] == 0) continue;
    list[cnt].ssid = appCfg.wifi[i].ssid;
    list[cnt].pass = appCfg.wifi[i].pass;
    cnt++;
  }
  wifiMgr.addNetworks(list, cnt);

  bool wifiOk = wifiMgr.begin();
  logs::logIf("[WIFI] begin ok=%d", (int)wifiOk);

  // ВСЕГДА стартуем с логов
  currentScreen = ScreenId::SCREEN_LOGS;
  needUpdateScreen = true;

  // Ждём Nightscout только если Wi-Fi реально поднялся
  bootWaitNsSecond = wifiOk;

  // применяем TZ/DST
  applyTimeConfig(appCfg.tzMinutes, appCfg.dstEnable);

  // Nightscout
  NSConfig c;
  c.baseUrl       = String(appCfg.hostSite);
  c.httpTimeoutMs = 15000;
  c.tlsInsecure   = true;

  g_ns.begin(c);
  g_ns.requestUpdate();
}

// ======================================================
//                  Nightscout tick (как было)
// ======================================================
static void tickNightscout() {
  static uint32_t lastTickMs = 0;
  static uint8_t  rcnt = 4;

  const uint32_t nowMs = millis();
  if (nowMs - lastTickMs >= POLL_INTERVAL) {
    lastTickMs = nowMs;

    time_t nowSec = time(nullptr);
    int32_t ageSec = INT32_MAX;

    // было: nsData.sensTime
    // стало: update_time_ms (ms epoch) -> секунды
   static bool hadValidTime = false; // чтобы stale age=2147483647 не триггерил пока нет данных
    const time_t sensTime = (time_t)(nsData.update_time_ms / 1000ULL);
    if (sensTime > 0) hadValidTime = true;

    if (nowSec > 100000 && sensTime > 0 && nowSec >= sensTime) {
      ageSec = (int32_t)(nowSec - sensTime);
    }

    static constexpr int32_t STALE_SEC  = 305;
    static constexpr uint8_t MIN_TICKS  = 4;

    if (hadValidTime && ageSec > STALE_SEC && rcnt >= MIN_TICKS) {
      rcnt = 0;
      logs::logIf("[SYS] stale age=%ds -> request update", (int)ageSec);
      g_ns.requestUpdate();
    }
    rcnt++;

    if (currentScreen == ScreenId::SCREEN_MAIN || currentScreen == ScreenId::SCREEN_CLOCK) {
      needUpdateScreen = true;
    }
  }

  bool nsChanged = g_ns.tick(nsData);
  if (nsChanged) needUpdateScreen = true;

  // ----- переход с LOGS на основной экран только после первого успешного NS -----
  if (bootWaitNsSecond) {
    // было: nsData.okEntries && nsData.okProps
    // стало: один запрос -> nsData.ok
    if (nsData.ok) {
      bootWaitNsSecond = false;
      currentScreen = (ScreenId)desiredUiScreen;
      needUpdateScreen = true;
      logs::logI("[SYS] NS connected");
    }
  }
}


// ======================================================
//                  Daily NTP resync (как было)
// ======================================================
static void tickTimeResync() {
  if (WiFi.status() != WL_CONNECTED) return;

  time_t now = time(nullptr);
  const bool valid = (now > 100000);

  // 1) Однократный лог появления валидного времени
  if (valid && !g_ntpWasValid) {
    g_ntpWasValid = true;
    logs::logI("[TIME] NTP synced");
    lastNtpSync = now;              // <-- важно: считаем, что синк случился "сейчас"
  }

  // 2) Ежедневный ресинк
  if (valid && lastNtpSync != 0 && (now - lastNtpSync) >= NTP_SYNC_PERIOD) {
    logs::logI("[TIME] Daily NTP resync");
    applyTimeConfig(appCfg.tzMinutes, appCfg.dstEnable);

    // взять актуальное время после configTime()
    time_t now2 = time(nullptr);
    if (now2 > 100000) lastNtpSync = now2;
    else lastNtpSync = now; // fallback на всякий случай
  }
}

// ======================================================
//                     Portal tick (LVGL)
// ======================================================
static void tickPortal() {
  tickButtons();
  portal.tick();

  // В портале нам обычно надо обновлять экран чуть чаще
  // (если у тебя есть отдельный portal LVGL screen — обновляй его тут)
  // Сейчас: просто просим перерисовать LOGS/portal-экран периодически
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


// ======================================================
//                            SETUP
// ======================================================

#include "nvs_flash.h"
#include "esp_err.h"

static void initSettingsSafe() {
  // Дай время подключить монитор порта (особенно если USB CDC)
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

  // --- отладка (оставляем в Serial) ---
  Serial.printf("[HEAP] after settings load: %u\n", (unsigned)ESP.getFreeHeap());
}


void setup() {
  Serial.begin(115200);

  // ---- LVGL display init ----
  gfx::LVGL_Display::instance().init();
  g_scr_logs  = ui_logs_create();
  ui_logs_set_buffer(&g_logs);
  

  logs::bind(&g_logs, true);  // true = также писать в Serial
  logs::logI("[SYS] boot");

  g_scr_main  = ui_main_create();
  g_scr_clock = ui_clock_create();
  g_scr_first = ui_first_create();
  g_scr_settings = ui_settings_create();
  brOverlay.init(lv_layer_top());
  
  // 1) Явно загрузим стартовый экран (например LOGS) до включения подсветки
  lv_screen_load(g_scr_logs);

  // 2) Прокачаем несколько циклов, чтобы flush реально ушёл в дисплей
  for (int i = 0; i < 10; i++) {
    gfx::LVGL_Display::instance().loop();
    delay(5);
  }

  // ---- Settings ----
  initSettingsSafe();

  // ---- Backlight (через твой Backlight класс) ----
  Backlight::Use(backlight);
  Backlight::beginStatic();
  backlight.restoreFromPercent(appCfg.brightness);

  // Onboarding (первый запуск)
  if (!appCfg.onboardingDone) {
      onboardingActive = true;
      currentScreen = ScreenId::SCREEN_FIRST;
      needUpdateScreen = true;
      Serial.printf("ScreenId::SCREEN_FIRST");
      return;
  }

  // // ---- Стартуем с LOGS (как раньше) ----
  currentScreen = ScreenId::SCREEN_LOGS;
  needUpdateScreen = true;

  // Первое сообщение в логи — до Wi-Fi, чтобы уже было что показать
  logs::logI("[WIFI] connecting");

  // === Wi-Fi + NTP + Nightscout ===
  initWifi();
  // currentScreen = ScreenId::SCREEN_MAIN;
  // lv_screen_load(g_scr_main);

  // lv_screen_load(g_scr_main);
  // for (int i = 0; i < 20; i++) { gfx::LVGL_Display::instance().loop(); delay(5); }

  //needUpdateScreen = true;

  // ---- LVGL timer с логикой “как старый tickScreen” ----
 // lv_timer_create(screens_tick_cb, 250, nullptr);
}

// ======================================================
//                             LOOP
// ======================================================
static uint32_t last_ms = 0;

void loop() {
  // 1) Если активен портал — своя логика
  if (portal.active()) {
    // показываем settings-экран один раз
    if (needUpdateScreen) {
      currentScreen = ScreenId::SCREEN_SETTINGS;
      lv_screen_load(g_scr_settings);
      needUpdateScreen = false;
    

    ui_settings_update_from_portal(portal); // <- обновляем ssid/pass/ip + qr по мере надобности
  }
    tickPortal();
    gfx::LVGL_Display::instance().loop();
    delay(1);
    return;
  //   tickPortal();
  //   //lv_timer_handler();
  //   gfx::LVGL_Display::instance().loop();
  //   delay(1);
  //   return;
  }

  // 2) Обычный режим
  tickButtons();
  tickNightscout();

  // базовый экран
  if (needUpdateScreen) {
    lv_obj_t* target = screenById(currentScreen);

    // если есть заявка на анимацию — используем её при смене экрана
    if (lv_scr_act() != target) {
      if (g_havePendingAnim) {
        lv_scr_load_anim(target, g_pendingAnim, g_pendingAnimTime, g_pendingAnimDelay, false);
      } else {
        lv_screen_load(target);
      }
    }

    // обновления контента
    if (currentScreen == ScreenId::SCREEN_MAIN) {
      main_update_full(nsData);
    } else if (currentScreen == ScreenId::SCREEN_CLOCK) {
      clock_update_full(nsData);  
    } else if (currentScreen == ScreenId::SCREEN_SETTINGS) {
      // ...
    } else if (currentScreen == ScreenId::SCREEN_LOGS) {
      // ...
    } else if (currentScreen == ScreenId::SCREEN_FIRST) {
      // ...
    }

    // сброс заявки
    g_havePendingAnim = false;
    g_pendingAnim = LV_SCR_LOAD_ANIM_NONE;

    needUpdateScreen = false;
  }
  tickTimeResync();

  // LVGL
  uint32_t now = millis();
  uint32_t diff = now - last_ms;
  if (diff) {
    last_ms = now;
    lv_tick_inc(diff);      // безопасно, не ISR
  }
  //lv_timer_handler();
  gfx::LVGL_Display::instance().loop();
  delay(1);
}
// void loop() {
//   static uint32_t t0 = 0;
//   static bool on = false;

//   if (millis() - t0 > 500) {
//     t0 = millis();
//     on = !on;
//     lv_color_t c = on ? lv_color_make(255, 0, 0) : lv_color_make(0, 0, 0);
//     lv_obj_set_style_bg_color(box, c, 0);
//     lv_obj_invalidate(box);
//     Serial.println(on ? "RED" : "BLACK");
//   }

//   gfx::LVGL_Display::instance().loop();
//   delay(1);
// }
