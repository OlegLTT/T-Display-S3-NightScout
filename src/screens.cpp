#include "screens.h"
#include <TimeLib.h>
#include "time_utils.h"

#include "font_roninset2_180.h"
#include "font_comfortaa_bold_18.h"
#include "font_volkswagen_medium_36.h"
#include "font_corporate_s_w10_16.h"


#include "logs.h"
#include "captive_portal.h"
#include <QRCode.h>   // ricmoo/QRCode
#include "settings.h"


#define RGB565(r, g, b) (((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3))

static uint16_t colorBySgvBounds(float sgv) {
  const float* b = appCfg.sgvBounds;

  // red:  sgv <= b0  OR sgv >= b3
  if (sgv <= b[0] || sgv >= b[3]) return TFT_RED;

  // yellow: (b0 < sgv <= b1) OR (b2 <= sgv < b3)
  if ((sgv > b[0] && sgv <= b[1]) || (sgv >= b[2] && sgv < b[3])) return TFT_YELLOW;

  // green: b1 < sgv < b2
  if (sgv > b[1] && sgv < b[2]) return TFT_GREEN;

  return TFT_YELLOW;
}

static void drawArrow(int x, int y, int angle, int lenght, int width, int tip, int dx, int wl, int w0, TFT_eSprite& sprite, uint16_t color)
{
  // начало стрелки
  float x0 =  0;
  float y0 = lenght/2;
  // конец стрелки
  float xc = 0;
  float yc = -lenght/2;
  // конец левой линии
  float xl = -width/2;
  float yl = lenght/2 - tip;
  // конец правой линии
  float xr = width/2;
  float yr = yl;
  
  float c = cos((180-angle)*PI/180);
  float s = sin((180-angle)*PI/180);

  int x0r = int( x0*c + y0*s)+x+dx/2;
  int y0r = int(-x0*s + y0*c)+y;

  int xcr = int( xc*c + yc*s)+x+dx/2;
  int ycr = int(-xc*s + yc*c)+y;

  int xlr = int( xl*c + yl*s)+x+dx/2;
  int ylr = int(-xl*s + yl*c)+y;

  int xrr = int( xr*c + yr*s)+x+dx/2;
  int yrr = int(-xr*s + yr*c)+y;

  sprite.drawWedgeLine(xcr, ycr, x0r, y0r, wl, w0, color);
  sprite.drawWedgeLine(xlr, ylr, x0r, y0r, wl, w0, color);
  sprite.drawWedgeLine(xrr, yrr, x0r, y0r, wl, w0, color);
  if (dx > 0){
    sprite.drawWedgeLine(xcr-dx, ycr, x0r-dx, y0r, wl, w0, color);
    sprite.drawWedgeLine(xlr-dx, ylr, x0r-dx, y0r, wl, w0, color);
    sprite.drawWedgeLine(xrr-dx, yrr, x0r-dx, y0r, wl, w0, color);
  }
}

void drawMainScreen(TFT_eSprite& sprite, const NightscoutData& ns) {
  tm tml;
  bool okTime = getLocalTime(&tml, 50); // 50ms таймаут

  const bool okE = ns.okEntries;
  const bool okP = ns.okProps;

  String sgv = (okE && !isnan(ns.sgv_mmol))   ? String(ns.sgv_mmol, 1)   : "--";

  String dir = okE ? String(ns.dir) : "--";

  String dlt = (okP && !isnan(ns.delta_mmol))? ns.delta_mmol>=0?"+"+String(ns.delta_mmol, 1):String(ns.delta_mmol, 1) : "--";
  String iob = (okP && !isnan(ns.iob)) ? String(ns.iob, 2) : "--";
  String cob = (okP && !isnan(ns.cob)) ? String(ns.cob, 1) : "--";

  int ageMin = 0;
  if (okE && ns.sensTime > 0) {
    time_t tnow = time(nullptr); //now();
    if (tnow > ns.sensTime) ageMin = (tnow - ns.sensTime) / 60;
  }
  String ageStr = okE ? String(ageMin) : "--";

  uint16_t bg_color = RGB565(40, 40, 40);
  sprite.fillSprite(bg_color);

  // Время HH:MM
  char buf[6] = "--:--";
  if (okTime) {
    snprintf(buf, sizeof(buf), "%02d:%02d", tml.tm_hour, tml.tm_min);
  }

  sprite.setTextDatum(TC_DATUM);
  sprite.setTextColor(TFT_DARKGREY, bg_color);
  sprite.loadFont(comfortaa_bold_18);
  sprite.drawString(buf, 160, 3);

  // IOB/COB
  sprite.setTextDatum(BL_DATUM);
  sprite.setTextColor(TFT_LIGHTGREY, bg_color);
  sprite.drawString("I: " + iob, 5, 170);
  sprite.drawString("C: " + cob, 120, 170);

  // Age
  sprite.setTextDatum(TR_DATUM);
  sprite.setTextColor(okE ? TFT_DARKGREEN : TFT_DARKGREY, bg_color);
  sprite.drawString(ageStr + " мин", 315, 3);

    // Батарея помпы
  sprite.setTextDatum(TC_DATUM);
  sprite.drawRect(5, 2, 42, 18, TFT_DARKGREY);
  sprite.drawRect(2, 6, 3, 10, TFT_DARKGREY);
  if (ns.okDevStatus && ns.pumpBatValid) {
     sprite.drawString(String(ns.pumpBatPct) + "%", 48-42/2, 4);
  }
  sprite.unloadFont();

  // sgv крупно
  sprite.setTextDatum(MC_DATUM);
  sprite.loadFont(roninset2_180);
  uint16_t sgvCol = TFT_DARKGREY;
  if (okE && !isnan(ns.sgv_mmol)) {
    sgvCol = colorBySgvBounds(ns.sgv_mmol);
  }
  sprite.setTextColor(sgvCol, bg_color);
  sprite.drawString(sgv, 120, 100);
  sprite.unloadFont();

  // Стрелка тренда 
  int x0 = 320 - 42;
  int y0 = 170 - 42;
  if (okE) {
    int a = 0;
    int dx = 0;
    if (dir == "Flat")               a = 90;
    else if (dir == "FortyFiveUp")   a = 45;
    else if (dir == "FortyFiveDown") a = 135;
    else if (dir == "SingleUp")      a = 0;
    else if (dir == "SingleDown")    a = 180;
    else if (dir == "DoubleUp") {    a = 0;   dx = 24; } 
    else if (dir == "DoubleDown") {  a = 180; dx = 24; }
    drawArrow(x0, y0, a, 60, 16, 15, dx, 4, 2, sprite, TFT_SKYBLUE);
  }
  // Delta
  sprite.setTextDatum(BC_DATUM);
  sprite.loadFont(volkswagen_medium_36);
  sprite.setTextColor(okP ? TFT_SKYBLUE : TFT_DARKGREY, bg_color);
  sprite.drawString(dlt, x0, y0 - 50);

  sprite.unloadFont();

  sprite.pushSprite(0, 0);
}



////
static const char* MONTHS_RU[] = { "января", "февраля",    "марта",  "апреля",    "мая",    "июня", 
                                     "июля", "августа", "сентября", "октября", "ноября", "декабря"};

static const char* WEEKDAYS_RU[] = {  "Воскресенье", "Понедельник", "Вторник", "Среда", "Четверг", "Пятница", "Суббота"};

void drawClockScreen(TFT_eSprite& sprite, const NightscoutData& ns) {
  const bool okE = ns.okEntries;
  const bool okP = ns.okProps;

  String sgv = (okE && !isnan(ns.sgv_mmol))   ? String(ns.sgv_mmol, 1)   : "--";
  String dir = okE ? String(ns.dir) : "--";
  String dlt = (okP && !isnan(ns.delta_mmol))? ns.delta_mmol>=0?"+"+String(ns.delta_mmol, 1):String(ns.delta_mmol, 1) : "--";
  // String iob = (okP && !isnan(ns.iob)) ? String(ns.iob, 2) : "--";
  // String cob = (okP && !isnan(ns.cob)) ? String(ns.cob, 1) : "--";

  int ageMin = 0;
  if (okE && ns.sensTime > 0) {
    time_t tnow = time(nullptr); //now();
    if (tnow > ns.sensTime) ageMin = (tnow - ns.sensTime) / 60;
  }
  String ageStr = okE ? String(ageMin) : "--";

  uint16_t zoneCol = TFT_DARKGREY;
  if (okE && !isnan(ns.sgv_mmol)) {
    zoneCol = colorBySgvBounds(ns.sgv_mmol);
  }
///-----
  uint16_t bg_color = RGB565(40, 40, 40);
  sprite.fillSprite(bg_color);

  tm tml;
  bool okTime = getLocalTime(&tml, 50); // 50ms таймаут

  // ===== Время =====
  char timeStr[6] = "--:--";
  char dateStr[64];
  char wdStr[64];
  if (okTime) {
    snprintf(timeStr, sizeof(timeStr), "%02d:%02d", tml.tm_hour, tml.tm_min);
    snprintf(dateStr, sizeof(dateStr), "%d %s %d", tml.tm_mday, MONTHS_RU[tml.tm_mon], tml.tm_year + 1900);
    snprintf(wdStr, sizeof(wdStr), "%s", WEEKDAYS_RU[tml.tm_wday]);
  }

  // Время крупно
  sprite.setTextDatum(MC_DATUM);
  sprite.setTextColor(zoneCol, bg_color);
  sprite.loadFont(roninset2_180);
  sprite.drawString(timeStr, 160, 100);
  sprite.unloadFont();

  // Дата ниже помельче
  sprite.setTextDatum(BL_DATUM);
  sprite.setTextColor(TFT_LIGHTGREY, bg_color);
  sprite.loadFont(comfortaa_bold_18);
  sprite.drawString(dateStr, 5, 170);

  sprite.setTextDatum(BR_DATUM);
  sprite.drawString(wdStr, 315, 170);

  // данные ns
  sprite.setTextDatum(TL_DATUM);
  sprite.setTextColor(zoneCol, bg_color);
  sprite.drawString(sgv, 6, 3);

  sprite.setTextDatum(TR_DATUM);
  sprite.setTextColor(zoneCol, bg_color);
  sprite.drawString(dlt + " " + dir, 160, 3);

  int x0 = 170;
  int y0 = 3+8;
  int a = 0;
  int dx = 0;
  if (dir == "Flat")               a = 90;
  else if (dir == "FortyFiveUp")   a = 45;
  else if (dir == "FortyFiveDown") a = 135;
  else if (dir == "SingleUp")      a = 0;
  else if (dir == "SingleDown")    a = 180;
  else if (dir == "DoubleUp") {    a = 0; dx = 8; } 
  else if (dir == "DoubleDown") {  a = 180; dx = 8; }
  drawArrow(x0, y0, a, 16, 16, 8, dx, 1, 1, sprite, zoneCol);

    // Age
  sprite.setTextDatum(TR_DATUM);
  sprite.setTextColor(okE ? TFT_DARKGREEN : TFT_DARKGREY, bg_color);
  sprite.drawString(ageStr + " мин", 315, 3);
  sprite.unloadFont();

  sprite.pushSprite(0, 0);
}

////

static int drawWrappedLine(TFT_eSprite& sprite, const String& text, int x, int y,
                           int maxW, int lineH, int maxLines)
{
  int linesUsed = 0;
  int start = 0;
  const int n = (int)text.length();

  while (start < n && linesUsed < maxLines) {
    // пропустить ведущие пробелы
    while (start < n && text[start] == ' ') start++;
    if (start >= n) break;

    int end = start;
    int lastSpace = -1;

    // расширяем до тех пор, пока влезает
    while (end < n) {
      char c = text[end];
      if (c == ' ') lastSpace = end;

      String candidate = text.substring(start, end + 1);
      if (sprite.textWidth(candidate) > maxW) break;
      end++;
    }

    int cut;
    if (end == start) {
      // даже 1 символ не влез — аварийный сдвиг
      cut = start + 1;
    } else if (end >= n) {
      // всё влезло
      cut = n;
    } else {
      // не влезло на end
      if (lastSpace >= start) {
        cut = lastSpace; // перенос по пробелу
      } else {
        cut = end;       // перенос внутри слова
      }
    }

    String line = text.substring(start, cut);
    line.trim(); // убрать пробелы по краям
    sprite.drawString(line, x, y + linesUsed * lineH);

    linesUsed++;

    // следующий кусок: после пробела (если перенос по пробелу)
    start = cut;
    while (start < n && text[start] == ' ') start++;
  }

  return linesUsed;
}

void drawLogScreen(TFT_eSprite& sprite, const LogBuffer& logs) {
  const uint16_t bg = TFT_BLACK;
  sprite.fillSprite(bg);
  
  sprite.unloadFont();   // ← ОБЯЗАТЕЛЬНО
  sprite.setTextFont(1);
  sprite.setTextSize(1);
  sprite.setTextDatum(TL_DATUM);

  const int lineH = sprite.fontHeight();
  const int screenW = 320;
  const int screenH = 170;
  int linesTotal = screenH / lineH;

  struct Item {
    LogEntry e;
    int lines;
  };

  Item items[32];   // запас (у тебя LogBuffer = 25)
  int itemCount = 0;
  int usedLines = 0;

  // 1) Считаем с КОНЦА (самые новые), пока влезает
  for (int i = 0; i < (int)logs.count(); i++) {
    LogEntry e = logs.getNewestFirst(i);

    String msg = String(e.ms / 1000) + " " + e.msg;
    int lines = max(1, (sprite.textWidth(msg) + screenW - 1) / screenW);

    if (usedLines + lines > linesTotal) break;

    items[itemCount++] = { e, lines };
    usedLines += lines;
  }

  // 2) Рисуем в ОБРАТНОМ порядке → старые сверху
  int y = 0;
  for (int i = itemCount - 1; i >= 0; i--) {
    const LogEntry& e = items[i].e;

    uint16_t col = TFT_GREEN;
    if (e.level == LogLevel::Warn)  col = TFT_YELLOW;
    if (e.level == LogLevel::Error) col = TFT_RED;

    sprite.setTextColor(col, bg);

    String msg = String(e.ms / 1000) + " " + e.msg;

    int used = drawWrappedLine(
      sprite,
      msg,
      0,
      y,
      screenW,
      lineH,
      linesTotal
    );

    y += used * lineH;
  }

  sprite.pushSprite(0, 0);
}


void drawPortalHelpScreen(TFT_eSprite& sprite, const CaptivePortal& portal) {
  const uint16_t bg = TFT_BLACK;
  sprite.fillSprite(bg);

  const int W = 320;
  const int H = 170;

  const int margin = 6;
  const int gap    = 6;

  // ===== Layout =====
  const int qrBig   = 101;  // WiFi QR width 
  const int qrSmall = 58;   // URL QR width

  // Big QR top-left
  const int qr1X = margin;
  const int qr1Y = margin;

  // Text block to the right of big QR
  const int t1X = qr1X + qrBig + gap;
  const int t1Y = margin;
  const int t1W = W - margin - t1X;
  (void)t1W;

  // Small QR bottom-right
  const int qr2X = W - margin - qrSmall;
  const int qr2Y = H - margin - qrSmall;

  // Text left of small QR (aligned with qr2Y)
  const int t2X = margin;
  const int t2Y = qr2Y;
  const int t2W = qr2X - gap - t2X;
  (void)t2W;

  // ===== Helpers =====
  auto escWifi = [](const String& s) {
    String o; o.reserve(s.length() + 8);
    for (size_t i = 0; i < s.length(); i++) {
      char c = s[i];
      if (c == '\\' || c == ';' || c == ',' || c == ':') o += '\\';
      o += c;
    }
    return o;
  };

  auto drawQr = [&](int bx, int by, int box, const String& payload,
                    // cache:
                    String& lastPayload, QRCode& qr, uint8_t* qrData, int qrDataCap,
                    bool& ok, int& usedVer)
  {
    // рамка
    sprite.drawRect(bx - 2, by - 2, box + 4, box + 4, TFT_DARKGREY);

    // rebuild only when payload changed
    if (payload != lastPayload) {
      lastPayload = payload;
      ok = false;
      usedVer = 0;

      for (int ver = 3; ver <= 12; ver++) { 
        int need = qrcode_getBufferSize(ver);
        if (need > qrDataCap) continue;

        qrcode_initText(&qr, qrData, ver, ECC_MEDIUM, payload.c_str());
        ok = true;
        usedVer = ver;
        break;
      }

    }

    if (!ok) {
      sprite.setTextFont(1);
      sprite.setTextColor(TFT_RED, bg);
      sprite.drawString("QR err", bx + 6, by + 6);
      return;
    }

    const int size = qr.size;           // modules count
    int scale = box / size;
    if (scale < 1) scale = 1;

    const int px = size * scale;
    const int x0 = bx + (box - px) / 2;
    const int y0 = by + (box - px) / 2;

    // белое поле + пиксели
    sprite.fillRect(x0, y0, px, px, TFT_WHITE);
    for (int j = 0; j < size; j++) {
      for (int i = 0; i < size; i++) {
        if (qrcode_getModule(&qr, i, j)) {
          sprite.fillRect(x0 + i * scale, y0 + j * scale, scale, scale, TFT_BLACK);
        }
      }
    }
  };

  // ===== Payloads =====
  const String ssid = portal.apSsid();
  const String pass = portal.apPass();
  const String wifiPayload = "WIFI:T:WPA;S:" + escWifi(ssid) + ";P:" + escWifi(pass) + ";;";

  // URL
  const String urlPayload = String("http://") + portal.apIP().toString() + "/"; // будет 192.168.4.1/

  // ===== QR caches (2 separate) =====
  static String lastWifi, lastUrl;

  static QRCode  qrWifi;
  static uint8_t qrWifiData[512];
  static bool    qrWifiOk = false;
  static int     qrWifiVer = 0;

  static QRCode  qrUrl;
  static uint8_t qrUrlData[256];
  static bool    qrUrlOk = false;
  static int     qrUrlVer = 0;

  // ===== Draw QR #1 (WiFi) =====
  drawQr(
    qr1X, qr1Y+18, qrBig, wifiPayload,
    lastWifi, qrWifi, qrWifiData, (int)sizeof(qrWifiData),
    qrWifiOk, qrWifiVer
  );

  // ===== Text #1 (right of WiFi QR) =====
  sprite.loadFont(comfortaa_bold_18);

  int y = 2;
  sprite.setTextDatum(TC_DATUM);
  sprite.setTextColor(TFT_RED, bg);
  sprite.drawString("ИНСТРУКЦИЯ", 160, y); 
  sprite.unloadFont();
  y += 18;

  sprite.loadFont(corporate_s_w10_16);
  sprite.setTextDatum(TL_DATUM);
  sprite.setTextColor(TFT_GREENYELLOW, bg);
  sprite.drawString("1) Подключитесь к Wi-Fi:", t1X, y); 
  y += 18;
  // sprite.drawString("к Wi-Fi сети:", t1X, y); 
  // y += 18;

  sprite.setTextColor(TFT_ORANGE, bg);
  sprite.drawString("SSID:", t1X, y);
  sprite.setTextColor(TFT_SKYBLUE, bg);
  sprite.drawString(ssid,  t1X + 50, y); 
  y += 18;

  sprite.setTextColor(TFT_ORANGE, bg);
  sprite.drawString("PASS:", t1X, y);
  sprite.setTextColor(TFT_SKYBLUE, bg);
  sprite.drawString(pass,  t1X + 50, y);

   sprite.unloadFont();

  // ===== Draw QR #2 (URL) =====
  drawQr(
    qr2X, qr2Y, qrSmall, urlPayload,
    lastUrl, qrUrl, qrUrlData, (int)sizeof(qrUrlData),
    qrUrlOk, qrUrlVer
  );

  // ===== Text #2 (left of bottom-right QR) =====
  sprite.setTextDatum(BR_DATUM);
  sprite.loadFont(corporate_s_w10_16);

  // 2 строки, чтобы точно влезло в t2W
  sprite.setTextColor(TFT_GREENYELLOW, bg);
  sprite.drawString("2) Если портал не открылся,", 320 - qrSmall - 10, 170 - 2 -18);
  sprite.drawString("перейдите на:", 320 - qrSmall - 10 - 18*8, 170-2);
  sprite.setTextColor(TFT_SKYBLUE, bg);
  sprite.drawString(urlPayload, 320 - qrSmall - 10, 170-2);

  sprite.unloadFont();

  sprite.pushSprite(0, 0);
}
