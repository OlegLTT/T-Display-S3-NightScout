#include <Arduino.h>
#include <TFT_eSPI.h>
#include <WiFi.h>

#include "credentials.h"
#include "nightscout_client.h"
#include "screens.h"
#include "buttons.h"
#include "backlight.h"
#include "overlay.h"
#include "logs.h"
#include "wifi_manager.h"

// ===== Wi-Fi =====
static WifiManager wifiMgr;
static WifiManagerConfig wifiCfg;

// пример списка сетей (можно вынести в credentials.h)
static const WifiCred WIFI_LIST[] = {
  {WIFI_SSID, WIFI_PASS},
  // {"MyWiFi2", "MyPass2"},
  // {"OpenWiFi", ""}, // пример открытой
};

// ===== Display =====
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite sprite = TFT_eSprite(&tft);

// ===== Screens state =====
static uint32_t lastPoll = 0;
static constexpr uint32_t POLL_INTERVAL = 15000; // 10 сек, как у вас было
static int currentScreen = 0;
static constexpr int numScreens = 3; // 0=main, 1=clock, 2=logs
static bool needUpdateScreen = true;

static LogBufferT<25, 96> logs;

// ===== Nightscout =====
static NightscoutClient ns;
static NightscoutData   nsData;

// ===== Brightness overlay =====
static BrightnessOverlay brOverlay;
static Backlight backlight;

// ----- Button callbacks -----
static Button btnUser;
static Button btnBoot;

static void onUserSingle() {
  //currentScreen = (currentScreen + 1) % numScreens;
  if (currentScreen == 2) {
    currentScreen = 0;
  } else {
    currentScreen = (currentScreen == 0) ? 1 : 0;  // только Main <-> Clock
  }
  needUpdateScreen = true;
}
static void onUserLong() {
  for(int i=0; i < 200;++i){
    logs.addf(LogLevel::Info,  "USER long i=%d some text...", i);
    logs.addf(LogLevel::Error, "USER long i=%d error text...", i);
   }
  needUpdateScreen = true;
}

static void onBootSingle() {
  Backlight::nextStatic();
  brOverlay.show(Backlight::currentPercentStatic(), 2000);
  needUpdateScreen = true;
}
static void onBootLong() {
  // Переходим на экран логов
  currentScreen = 2;
  needUpdateScreen = true;

  logs.add(LogLevel::Info, "BOOT long -> logs");
}

void setup() {
  Serial.begin(115200);

   // стартуем с лог-экрана
  currentScreen = 2;        // Logs
  needUpdateScreen = true;

  // Лог: начало подключения
  logs.add(
    LogLevel::Info,
    String("Connecting to Wi-Fi")
  );
  drawLogScreen(sprite, logs);   // сразу показать

  // Display
  tft.init();
  tft.setRotation(3);
  sprite.createSprite(320, 170);
  brOverlay.begin(tft, 320, 170);

  // нарисовать лог-экран до ожидания WiFi
  drawLogScreen(sprite, logs);
  needUpdateScreen = true;

  // Buttons
  btnUser.begin(14);
  btnUser.onSingle = onUserSingle;
  //btnUser.onDouble = onUserDouble;
  btnUser.onLong   = onUserLong;
  //btnUser.onRepeat = onUserRepeat;
  btnBoot.begin(0);
  btnBoot.onSingle = onBootSingle;
  btnBoot.onLong   = onBootLong;
  Buttons::attach(&btnUser);
  Buttons::attach(&btnBoot);
  Buttons::beginTask();

  // Backlight
  Backlight::Use(backlight);
  Backlight::beginStatic();

// WiFi manager
  wifiCfg.hostname = "t-display-s3";
  wifiCfg.connectTimeoutMs = 20000;
  wifiCfg.retryDelayMs = 500;
  wifiCfg.enableAutoReconnect = true;
  // если хотите fallback AP:
  // wifiCfg.enableFallbackAP = false; // включите при необходимости
  // wifiCfg.fallbackApSsid = "TDisplayS3-Setup";
  // wifiCfg.fallbackApPass = "12345678";
  wifiMgr.setConfig(wifiCfg);
  wifiMgr.setLogger(&logs);
  wifiMgr.addNetworks(WIFI_LIST, sizeof(WIFI_LIST)/sizeof(WIFI_LIST[0]));

  bool wifiOk = wifiMgr.begin();
  if (wifiOk) {
    currentScreen = 0;        // экран 0 
  } else {
    currentScreen = 2;        // остаёмся на логах
  }
  needUpdateScreen = true;
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");

  NSConfig c;
  c.baseUrl = HOST_SITE; //cfg.nightscout_url;     // подставьте ваши имена
  c.token   = ""; //cfg.nightscout_token;
  c.sgvOnly = 0;//cfg.sgv_only;
  c.showMgdl = 0; //cfg.show_mgdl;
  c.infoLine = 3; //cfg.info_line;         // 2 или 3
  c.httpTimeoutMs = 15000;
  c.tlsInsecure = true;

  ns.begin(c);
}

void loop() {
  uint32_t nowMs = millis();

  // Nightscout poll
  if (millis() - lastPoll >= POLL_INTERVAL) {
    lastPoll = millis();
    ns.update(nsData);
    needUpdateScreen = true;
  }

   // update overlay timer
  bool overlayChanged = brOverlay.update();
 
  // redraw screen
  if (needUpdateScreen) {
    if (currentScreen == 0) {
      drawMainScreen(sprite, nsData);
    } else if (currentScreen == 1) {
      drawClockScreen(sprite);
    } else {
      drawLogScreen(sprite, logs);
    }
    needUpdateScreen = false;
  }
  
  // If overlay is visible, draw it on top (even if base wasn't redrawn)
  if (brOverlay.visible()) {
    brOverlay.render();
  } else if (overlayChanged) {
    needUpdateScreen = true; // overlay just disappeared -> need to redraw base once to clear it
  }

}
