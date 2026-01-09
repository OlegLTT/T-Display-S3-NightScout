#include <Arduino.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <time.h>
#include "settings.h"
#include "captive_portal.h"
#include "time_utils.h"

#include "nightscout_client.h"
#include "screens.h"
#include "buttons.h"
#include "backlight.h"
#include "overlay.h"
#include "logs.h"
#include "wifi_manager.h"

// == settings ====
static SettingsStore settingsStore;
AppSettings appCfg;
static CaptivePortal portal;

// ===== Wi-Fi =====
static WifiManager wifiMgr;
static WifiManagerConfig wifiCfg;

// ===== Display =====
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite sprite = TFT_eSprite(&tft);

// ===== Screens state =====
static constexpr int SCREEN_MAIN   = 0;
static constexpr int SCREEN_CLOCK  = 1;
static constexpr int SCREEN_LOGS   = 2;
static constexpr int SCREEN_FIRST  = 3; // экран приветствия
static constexpr uint32_t POLL_INTERVAL = 15000; // 15 сек, как у вас было
static int currentScreen = SCREEN_MAIN;
static uint8_t desiredUiScreen = SCREEN_MAIN;   // 0/1 то, что надо восстановить
static constexpr int numScreens = 3; // 0=main, 1=clock, 2=logs
static bool onboardingActive = false;
static uint32_t lastLogVersion = 0;

static bool needUpdateScreen = true;
static bool bootWaitNsSecond = true;   // при старте ждём успешные Entries+Props

static LogBufferT<25, 96> logs;

static volatile bool g_rebootRequested = false;

// ===== Nightscout =====
static NightscoutClient ns;
static NightscoutData   nsData;
// если была фатальная ошибка Nightscout (в т.ч. DNS Failed) — выключаем его
// static bool nsFatalError   = false;
// static bool nsErrorLogged  = false;

// ===== Brightness overlay =====
static BrightnessOverlay brOverlay;
static Backlight backlight;

// ----- Button callbacks -----
static Button btnUser;
static Button btnBoot;

// --- Clock screen: instant redraw on minute change ---
static int lastClockMin = -1;
static int lastClockHour = -1;
static int lastClockYday = -1; // на случай смены даты

// ---- Daily NTP sync ----
static time_t lastNtpSync = 0;
static constexpr time_t NTP_SYNC_PERIOD = 24 * 60 * 60; // 24 часа

static void saveUiScreenIfNeeded() {
  if (currentScreen == SCREEN_MAIN || currentScreen == SCREEN_CLOCK) {
    desiredUiScreen = (uint8_t)currentScreen;

    appCfg.lastUiScreen = desiredUiScreen;
    settingsStore.save(appCfg); // да, можно сохранять сразу (редко нажимаешь)
    Serial.printf("[UI] save last screen=%d\n", (int)desiredUiScreen);
  }
}

static void onUserSingle() {
  //  если сейчас captive portal активен — single = reboot/exit
  if (portal.active()) {
    g_rebootRequested = true;
    return;
  }
   // в режиме приветствия игнорируем одиночное нажатие
  if (onboardingActive) {
    return;
  }
  if (currentScreen == SCREEN_LOGS) {
    currentScreen = SCREEN_MAIN;
  } else {
    currentScreen = (currentScreen == SCREEN_MAIN) ? SCREEN_CLOCK : SCREEN_MAIN;
  }
  saveUiScreenIfNeeded();
  needUpdateScreen = true;
}

static void onUserLong() {
  // В РЕЖИМЕ ПРИВЕТСТВИЯ: сразу открываем captive-portal для настроек
  if (onboardingActive) {
    WiFi.disconnect(true);
    portal.begin(&logs, &settingsStore, &appCfg);
    needUpdateScreen = true;
    return;
  }

  // включаем режим настройки
  currentScreen = SCREEN_LOGS;
  // остановить обычный WiFi менеджер (не обязателен, но лучше)
  WiFi.disconnect(true);
  portal.begin(&logs, &settingsStore, &appCfg);
  needUpdateScreen = true;
}

static void onBootSingle() {
  Backlight::nextStatic();
  uint8_t p = Backlight::currentPercentStatic();
  brOverlay.show(p, 2000);
  // запоминаем в настройках
  appCfg.brightness = p;
  settingsStore.save(appCfg);
  needUpdateScreen = true;
}

static void onBootLong() {
  // в режиме приветствия boot-кнопка ничего не делает
  if (onboardingActive) return;

  // Переходим на экран логов
  currentScreen = SCREEN_LOGS;
  needUpdateScreen = true;
}

static void initDisplay() {
  // Display
  tft.init();
  tft.setRotation(3);
  sprite.createSprite(320, 170);

  // Overlay яркости
  brOverlay.begin(tft, 320, 170);

  // Backlight 
  Backlight::Use(backlight);
  Backlight::beginStatic();
  backlight.restoreFromPercent(appCfg.brightness);
}

static void initButtons() {
  // USER button
  btnUser.begin(14);
  btnUser.onSingle = onUserSingle;
  //btnUser.onDouble = onUserDouble;
  btnUser.onLong   = onUserLong;
  //btnUser.onRepeat = onUserRepeat;

  // BOOT button
  btnBoot.begin(0);
  btnBoot.onSingle = onBootSingle;
  btnBoot.onLong   = onBootLong;

  Buttons::attach(&btnUser);
  Buttons::attach(&btnBoot);
  //Buttons::beginTask();  // по прежнему не используем
}

static void initWifi() {
  // WiFi manager config
  wifiCfg.hostname = "t-display-s3";
  wifiCfg.connectTimeoutMs = 20000;
  wifiCfg.retryDelayMs = 500;
  wifiCfg.enableAutoReconnect = true;
  // wifiCfg.enableFallbackAP оставляем по умолчанию (false)
  wifiMgr.setConfig(wifiCfg);
  wifiMgr.setLogger(&logs);

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

  // Подключение к Wi-Fi
  bool wifiOk = wifiMgr.begin();

  // ВСЕГДА стартуем с логов
  currentScreen = 2;
  needUpdateScreen = true;

  // Ждём Nightscout только если Wi-Fi реально поднялся
  bootWaitNsSecond = wifiOk;

  // применяем TZ/DST (настройки уже загружены ранее)
  applyTimeConfig(appCfg.tzMinutes, appCfg.dstEnable);

  // Nightscout
  NSConfig c;
  c.baseUrl       = String(appCfg.hostSite);
  c.httpTimeoutMs = 15000;
  c.tlsInsecure   = true;

  // ВАЖНО: по умолчанию используем HTTP, а не HTTPS.
  // Если нужно HTTPS — пользователь явно пишет "https://..."
  //c.preferHttpsIfNoScheme = false;

  ns.begin(c);
  ns.requestUpdate();
}

void setup() {
  Serial.begin(115200);

  // === Settings ===
  settingsStore.begin();
  settingsStore.load(appCfg);

  if (appCfg.brightness < 1 || appCfg.brightness > 100) {
    appCfg.brightness = 100;
  }

  desiredUiScreen = (appCfg.lastUiScreen <= 1) ? appCfg.lastUiScreen : 0;

  // Стартуем с экрана логов
  currentScreen = SCREEN_LOGS;        // Logs
  needUpdateScreen = true;

  // === Hardware/UI init ===
  initDisplay();   // TFT, sprite, overlay, backlight
  initButtons();   // USER/BOOT + callbacks

  // Onboarding (первый запуск)
  if (!appCfg.onboardingDone) {
      onboardingActive = true;
      currentScreen = SCREEN_FIRST;
      needUpdateScreen = true;
      return;
  }

  // Первое сообщение в логи — до Wi-Fi, чтобы уже было что показать
  logs.add(LogLevel::Info, String("Connecting to Wi-Fi"));

  // Нарисовать лог-экран сразу
  drawLogScreen(sprite, logs);
  needUpdateScreen = true;

  // === Wi-Fi + NTP + Nightscout ===
  initWifi();
}


// ===== Tasks =====

static void tickPortal() {
  // работаем только если портал активен
  Buttons::tick();
  portal.tick();

  static uint32_t lastDraw = 0;
  uint32_t nowMs = millis();

  // оверлей яркости поверх портала
  bool overlayChanged = brOverlay.update();

  if (needUpdateScreen || (nowMs - lastDraw) > 500) {
    drawPortalHelpScreen(sprite, portal);
    needUpdateScreen = false;
    lastDraw = nowMs;
  }

  if (brOverlay.visible()) {
    brOverlay.render();
  } else if (overlayChanged) {
    // только что спрятался оверлей — перерисовать базу
    needUpdateScreen = true;
  }

  if (g_rebootRequested) {
    g_rebootRequested = false;
    delay(150);
    ESP.restart();
  }

  // даём поработать WiFi/RTOS
  yield();
}

static void tickNightscout() {
  // 2) Если фатал уже пометили из main (на всякий) — тоже выходим
  // if (nsFatalError) {
  //   return;
  // }

  // ---- 15s tick + "fetch only if stale" ----
  static uint32_t lastTickMs = 0;
  static uint8_t  rcnt = 4;

  const uint32_t nowMs = millis();
  if (nowMs - lastTickMs >= POLL_INTERVAL) {
    lastTickMs = nowMs;

    time_t nowSec = time(nullptr);
    int32_t ageSec = INT32_MAX;

    if (nowSec > 100000 && nsData.sensTime > 0 && nowSec >= nsData.sensTime) {
      ageSec = (int32_t)(nowSec - nsData.sensTime);
    }

    static constexpr int32_t STALE_SEC = 305;
    static constexpr uint8_t MIN_TICKS = 4;

    if (ageSec > STALE_SEC && rcnt >= MIN_TICKS) {
      rcnt = 0;
      Serial.printf("[NS] stale age=%ds -> requestUpdate()\n", (int)ageSec);
      ns.requestUpdate();
    }
    rcnt++;

    if (currentScreen == SCREEN_MAIN || currentScreen == SCREEN_CLOCK) {
      needUpdateScreen = true;
    }
  }

  // один шаг FSM
  bool nsChanged = ns.tick(nsData);
  if (nsChanged)
    needUpdateScreen = true;

  // ----- переход с LOGS на основной экран только после первого успешного NS -----
  if (bootWaitNsSecond) {
    if (nsData.okEntries && nsData.okProps) {
      bootWaitNsSecond = false;
      currentScreen = desiredUiScreen;
      needUpdateScreen = true;
      Serial.println("[BOOT] NS entries+props OK -> show Main");
    }
  }
}


static void tickScreen() {
  // ----- Clock screen: force redraw when minute changes -----
  if (currentScreen == SCREEN_CLOCK) {
    tm tml;
    if (getLocalTime(&tml, 10)) {
      if (tml.tm_min  != lastClockMin ||
          tml.tm_hour != lastClockHour ||
          tml.tm_yday != lastClockYday) {
        lastClockMin  = tml.tm_min;
        lastClockHour = tml.tm_hour;
        lastClockYday = tml.tm_yday;
        needUpdateScreen = true;
      }
    }
  } else {
    // ушли с часов — сброс, чтобы при возврате сразу обновилось
    lastClockMin = lastClockHour = lastClockYday = -1;
  }

  // overlay timer
  bool overlayChanged = brOverlay.update();

  // если активен экран логов – обновляем только при изменении версии
  if (currentScreen == SCREEN_LOGS) {
    uint32_t v = logs.version();
    if (v != lastLogVersion) {
      lastLogVersion = v;
      needUpdateScreen = true;
    }
  }

  // базовый экран
  if (needUpdateScreen) {
    if (currentScreen == SCREEN_MAIN) {
      drawMainScreen(sprite, nsData);
    } else if (currentScreen == SCREEN_CLOCK) {
      drawClockScreen(sprite, nsData);
    } else if (currentScreen == SCREEN_LOGS) {
      drawLogScreen(sprite, logs);
    } else if (currentScreen == SCREEN_FIRST) {
      drawFirstScreen(sprite);
    }
    needUpdateScreen = false;
  }

  // оверлей поверх
  if (brOverlay.visible()) {
    brOverlay.render();
  } else if (overlayChanged) {
    // оверлей только что исчез -> надо один раз перерисовать базу,
    // чтобы убрать "след"
    needUpdateScreen = true;
  }
}

static void tickTimeResync() {
  // ----- Daily NTP resync (once per 24h) -----
  if (WiFi.status() == WL_CONNECTED) {
    time_t now = time(nullptr);

    // ждём, пока время станет валидным
    if (now > 100000) {
      if (lastNtpSync == 0) {
        // первая валидная точка времени после старта
        lastNtpSync = now;
      } else if ((now - lastNtpSync) >= NTP_SYNC_PERIOD) {
        Serial.println("[TIME] Daily NTP resync");

        applyTimeConfig(appCfg.tzMinutes, appCfg.dstEnable);

        lastNtpSync = now;
      }
    }
  }
}

void loop() {
  // 1) Если активен портал — вся логика внутри tickPortal()
  if (portal.active()) {
    tickPortal();
    return;
  }

  // 2) Обычный режим
  Buttons::tick();
  tickNightscout();
  tickScreen();
  tickTimeResync();

  yield(); // для WiFi/RTOS
  }

