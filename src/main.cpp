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
//#include "buttons.h"
#include "backlight.h"
#include "overlay.h"
#include "logs.h"
#include "wifi_manager.h"

#if defined(BOARD_TDISPLAY_S3)
  #include <uButton.h>
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
#endif

static volatile bool g_rebootRequested = false;

// --- Clock screen: instant redraw on minute change ---
static int lastClockMin  = -1;
static int lastClockHour = -1;
static int lastClockYday = -1;

// ---- Daily NTP sync ----
static time_t lastNtpSync = 0;
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
// ======================================================
//                    LVGL tick (1ms) timer
// ======================================================
//static hw_timer_t* g_lv_tick_timer = nullptr;

// static void IRAM_ATTR lv_tick_isr() {
//   lv_tick_inc(1);
// }

// static void lv_tick_init_1ms() {
//   // timer 0, prescaler 80 => 1us per tick (ESP32 APB 80MHz)
//   g_lv_tick_timer = timerBegin(0, 80, true);
//   timerAttachInterrupt(g_lv_tick_timer, &lv_tick_isr, true);
//   timerAlarmWrite(g_lv_tick_timer, 1000, true); // 1000 us => 1 ms
//   timerAlarmEnable(g_lv_tick_timer);
// }

// // ======================================================
// //                  UI save/restore helpers
// // ======================================================
// static void saveUiScreenIfNeeded() {
//   if (currentScreen == SCREEN_MAIN || currentScreen == SCREEN_CLOCK) {
//     desiredUiScreen = (uint8_t)currentScreen;
//     appCfg.lastUiScreen = desiredUiScreen;
//     settingsStore.save(appCfg);
//     Serial.printf("[UI] save last screen=%d\n", (int)desiredUiScreen);
//   }
// }

// static void ui_load(ScreenId s)
// {
//   switch(s) {
//     case SCREEN_MAIN:
//       if(g_scr_main) lv_screen_load(g_scr_main);
//       break;
//     case SCREEN_CLOCK:
//       if(g_scr_clock) lv_screen_load(g_scr_clock);
//       break;
//     case SCREEN_LOGS:
//       if(g_scr_logs) lv_screen_load(g_scr_logs);
//       break;
//     case SCREEN_FIRST:
//     default:
//       if(g_scr_first) lv_screen_load(g_scr_first);
//       else if(g_scr_main) lv_screen_load(g_scr_main);
//       break;
//   }
// }


// ======================================================
//                      Button callbacks
// ======================================================
static void onUserSingle() {
  // если сейчас captive portal активен — single = reboot/exit
  if (portal.active()) {
    g_rebootRequested = true;
    return;
  }

  // в режиме приветствия игнорируем одиночное нажатие
  // (если у тебя есть onboardingActive — подключи; я оставил поведение как было)
  // if (onboardingActive) return;

  if (currentScreen == ScreenId::SCREEN_LOGS) {
    currentScreen = ScreenId::SCREEN_MAIN;
  } else {
    currentScreen = (currentScreen == ScreenId::SCREEN_MAIN) ? ScreenId::SCREEN_CLOCK : ScreenId::SCREEN_MAIN;
  }
  // saveUiScreenIfNeeded();
  // ui_load(currentScreen);
  Serial.println("onUserSingle");
  needUpdateScreen = true;
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
  currentScreen = ScreenId::SCREEN_LOGS;
  needUpdateScreen = true;
}

static void tickButtons() {
  // tick() вернёт true при смене состояния :contentReference[oaicite:5]{index=5}
  if (btnUser.tick()) {
    if (btnUser.click()) onUserSingle();
    if (btnUser.hold())  onUserLong();
  }

  if (btnBoot.tick()) {
    if (btnBoot.click()) onBootSingle();
    if (btnBoot.hold())  onBootLong();
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
      Serial.printf("[NS] stale age=%ds -> requestUpdate()\n", (int)ageSec);
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
      Serial.println("[BOOT] NS OK -> show UI");
    }
  }
}


// ======================================================
//                  Daily NTP resync (как было)
// ======================================================
static void tickTimeResync() {
  if (WiFi.status() == WL_CONNECTED) {
    time_t now = time(nullptr);
    if (now > 100000) {
      if (lastNtpSync == 0) {
        lastNtpSync = now;
      } else if ((now - lastNtpSync) >= NTP_SYNC_PERIOD) {
        Serial.println("[TIME] Daily NTP resync");
        applyTimeConfig(appCfg.tzMinutes, appCfg.dstEnable);
        lastNtpSync = now;
      }
    }
  }
}

// static void first_update_full() {
//   // пока нет onboarding экрана в LVGL — просто показываем MAIN
//   main_update_full(nsData);
// }

// ======================================================
//      LVGL screens tick (ВАЖНО: логика как в старом tickScreen)
// ======================================================
// static void screens_tick_cb(lv_timer_t*) {
//   // ----- Clock: redraw when minute changes -----
//   if (currentScreen == SCREEN_CLOCK) {
//     tm tml;
//     if (getLocalTime(&tml, 10)) {
//       if (tml.tm_min  != lastClockMin ||
//           tml.tm_hour != lastClockHour ||
//           tml.tm_yday != lastClockYday) {
//         lastClockMin  = tml.tm_min;
//         lastClockHour = tml.tm_hour;
//         lastClockYday = tml.tm_yday;
//         needUpdateScreen = true;
//       }
//     }
//   } else {
//     lastClockMin = lastClockHour = lastClockYday = -1;
//   }

//   // overlay timer
//   bool overlayChanged = brOverlay.update();

//   // logs: update only if version changed
//   if (currentScreen == SCREEN_LOGS) {
//     uint32_t v = g_logs.version();
//     if (v != lastLogVersion) {
//       lastLogVersion = v;
//       needUpdateScreen = true;
//     }
//   }

//   // базовый экран (ВАЖНО: тут должны быть LVGL-апдейты, не sprite!)
//   if (needUpdateScreen) {
//     if (currentScreen == SCREEN_MAIN) {
//       main_update_full(nsData);
//     } else if (currentScreen == SCREEN_CLOCK) {
//       clock_update_full(nsData);
//     } else if (currentScreen == SCREEN_LOGS) {
//       logs_update_full(g_logs);
//     } else { // SCREEN_FIRST
//       first_update_full();
//     }
//     needUpdateScreen = false;
//   }

//   // overlay поверх
//   if (brOverlay.visible()) {
//     brOverlay.render();
//   } else if (overlayChanged) {
//     // overlay только что исчез -> один раз обновить базу, чтобы убрать "след"
//     needUpdateScreen = true;
//   }
// }


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
// static lv_obj_t* scr;
// static lv_obj_t* box;

// void setup() {
//   Serial.begin(115200);
//   delay(300);
//   Serial.println("SMOKE TEST START");

//   gfx::LVGL_Display::instance().init();

//   scr = lv_obj_create(nullptr);
//   lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
//   lv_obj_set_size(scr, LV_PCT(100), LV_PCT(100));
//   lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

//   box = lv_obj_create(scr);
//   lv_obj_set_size(box, LV_PCT(100), LV_PCT(100));
//   lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
//   lv_obj_set_style_border_width(box, 0, 0);

//   lv_screen_load(scr);

//   // Прокрутим несколько кадров, чтобы гарантированно ушёл flush
//   for (int i = 0; i < 50; i++) { gfx::LVGL_Display::instance().loop(); delay(10); }
// }
#include "nvs_flash.h"
#include "esp_err.h"

static void initSettingsSafe() {
  // Дай время подключить монитор порта (особенно если USB CDC)
  delay(1500);

  Serial.println("[BOOT] initSettingsSafe");

  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    Serial.printf("[NVS] init err=%s -> erase\n", esp_err_to_name(err));
    nvs_flash_erase();
    err = nvs_flash_init();
  }
  Serial.printf("[NVS] init=%s\n", esp_err_to_name(err));

  bool ok = settingsStore.begin();
  Serial.printf("[PREF] begin=%d\n", ok);

  if (!ok) {
    Serial.println("[PREF] begin failed -> using defaults");
    appCfg = AppSettings();
    return;
  }

  Serial.printf("[HEAP] before load: %u\n", (unsigned)ESP.getFreeHeap());
  settingsStore.load(appCfg);
  Serial.printf("[HEAP] after  load: %u\n", (unsigned)ESP.getFreeHeap());
}

void setup() {
  Serial.begin(115200);

  // ---- Settings ----
  initSettingsSafe();

  // esp_err_t err = nvs_flash_init();
  // if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
  //   Serial.printf("[NVS] init err=%s -> erase\n", esp_err_to_name(err));
  //   nvs_flash_erase();
  //   err = nvs_flash_init();
  // }
  // Serial.printf("[NVS] init=%s\n", esp_err_to_name(err));

  // bool ok = settingsStore.begin();
  // Serial.printf("[PREF] begin=%d\n", ok);

  // if (!ok) {
  //   Serial.println("[PREF] begin failed -> using defaults");
  //   appCfg = AppSettings();
  //   return;
  // }

  // Serial.printf("[HEAP] before load: %u\n", (unsigned)ESP.getFreeHeap());
  // settingsStore.load(appCfg);
  // Serial.printf("[HEAP] after  load: %u\n", (unsigned)ESP.getFreeHeap());

  // bool ok = settingsStore.begin();
  // if (!ok) {
  //   Serial.println("NVS init failed!");
  //   // можно хотя бы продолжить без настроек
  // } else {
  //  // settingsStore.load(appCfg);
  // }
  //settingsStore.load(appCfg);

  // if (appCfg.brightness < 1 || appCfg.brightness > 100) {
  //   appCfg.brightness = 100;
  // }

  //desiredUiScreen = (appCfg.lastUiScreen <= 1) ? appCfg.lastUiScreen : 0;

  // ---- Backlight (через твой Backlight класс) ----
  Backlight::Use(backlight);
  Backlight::beginStatic();
  backlight.restoreFromPercent(appCfg.brightness);

  // ---- LVGL display init (твоя обвязка) ----
  // ВАЖНО: это должно внутри делать lv_init(), регистрация дисплея, flush, input и т.п.
  //delay(1000);
  gfx::LVGL_Display::instance().init();

  g_scr_main  = ui_main_create();
  g_scr_clock = ui_clock_create();
  g_scr_logs  = ui_logs_create();
  g_scr_first = ui_first_create();
  g_scr_settings = ui_settings_create();

  // lv_obj_t* boot = lv_obj_create(nullptr);
  // lv_obj_set_style_bg_color(boot, lv_color_black(), 0);
  // lv_obj_set_style_bg_opa(boot, LV_OPA_COVER, 0);
  // lv_screen_load(boot);

  // gfx::LVGL_Display::instance().loop(); // один кадр чёрного
  // delay(50);

  // ---- LVGL tick 1ms ----
  //lv_tick_init_1ms();

// #if defined(BOARD_TDISPLAY_S3)
//   Serial.println();
//   Serial.println("Booting LVGL on T-Display S3");

//   // Создаём экраны (объекты)
//   g_scr_main  = ui_main_create();
//   g_scr_clock = ui_clock_create();
//   g_scr_logs  = ui_logs_create();

//   // currentScreen = ScreenId::SCREEN_MAIN;
//   // lv_screen_load(g_scr_main);

//  // needUpdateScreen = true;

// #elif defined(BOARD_ESP32S3_DEV)
//   delay(2000);
//   Serial.println("Hello from ESP32-S3 1.43 AMOLED");

//   pinMode(PIN_BACKLIGHT, OUTPUT);
//   digitalWrite(PIN_BACKLIGHT, HIGH);

//   pinMode(PIN_TFT_RST, OUTPUT);
//   digitalWrite(PIN_TFT_RST, LOW);
//   delay(20);
//   digitalWrite(PIN_TFT_RST, HIGH);
//   delay(20);

//   Serial.println("Init touch + LCD (LVGL demo)...");
//   Touch_Init();
//   lcd_lvgl_Init();
//   Serial.println("Init done.");

//   // Если на этой плате другие экраны/создание — добавь тут
// #endif

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

  // ---- WiFi + NTP + Nightscout ----
  g_logs.add(LogLevel::Info, String("Connecting to Wi-Fi"));
  initWifi();


  // === Hardware/UI init ===
  // initDisplay();   // TFT, sprite, overlay, backlight
  // initButtons();   // USER/BOOT + callbacks

  // Первое сообщение в логи — до Wi-Fi, чтобы уже было что показать
  g_logs.add(LogLevel::Info, String("Connecting to Wi-Fi"));

  // Нарисовать лог-экран сразу
  //drawLogScreen(sprite, logs);
  //needUpdateScreen = true;

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
    if (currentScreen == ScreenId::SCREEN_MAIN) {
          // 1) если экран ещё не загружен — загрузить
        if (lv_scr_act() != g_scr_main) {
          lv_screen_load(g_scr_main);
        }
        // 2) обновить данные
        main_update_full(nsData);
    } else if (currentScreen == ScreenId::SCREEN_CLOCK) {
      lv_screen_load(g_scr_clock);
    } else if (currentScreen == ScreenId::SCREEN_LOGS) {
      lv_screen_load(g_scr_logs);
    } else if (currentScreen == ScreenId::SCREEN_FIRST) {
      lv_screen_load(g_scr_first);
    } else if (currentScreen == ScreenId::SCREEN_SETTINGS) {
      lv_screen_load(g_scr_settings);
      Serial.printf("ScreenId::SCREEN_SETTINGS");
    }
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
