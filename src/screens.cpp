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
#include "sgv_zone.h"


#define RGB565(r, g, b) (((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3))


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
  bool okTime = getLocalTime(&tml, 50);

  const bool okE = ns.okEntries;
  const bool okP = ns.okProps;

  // --- format buffers (NO String allocations) ---
  char timeBuf[6]   = "--:--";
  char sgvBuf[12]   = "--";
  char dltBuf[12]   = "--";
  char iobBuf[16]   = "--";
  char cobBuf[16]   = "--";
  char ageBuf[12]   = "--";

  if (okTime) {
    snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", tml.tm_hour, tml.tm_min);
  }

  if (okE && !isnan(ns.sgv_mmol)) {
    snprintf(sgvBuf, sizeof(sgvBuf), "%.1f", ns.sgv_mmol);
  }

  if (okP && !isnan(ns.delta_mmol)) {
    snprintf(dltBuf, sizeof(dltBuf), "%+.1f", ns.delta_mmol);
  }

  if (okP && !isnan(ns.iob)) {
    snprintf(iobBuf, sizeof(iobBuf), "%.2f", ns.iob);
  }

  if (okP && !isnan(ns.cob)) {
    snprintf(cobBuf, sizeof(cobBuf), "%.1f", ns.cob);
  }

  int ageMin = 0;
  if (okE && ns.sensTime > 0) {
    time_t tnow = time(nullptr);
    if (tnow > ns.sensTime) ageMin = (int)((tnow - ns.sensTime) / 60);
  }
  if (okE) snprintf(ageBuf, sizeof(ageBuf), "%d", ageMin);


  // --- draw ---
  const uint16_t bg_color = RGB565(40, 40, 40);
  sprite.fillSprite(bg_color);

  // Time HH:MM (top)
  sprite.setTextDatum(TC_DATUM);
  sprite.setTextColor(TFT_DARKGREY, bg_color);
  sprite.loadFont(comfortaa_bold_18);
  sprite.drawString(timeBuf, 160, 3);

  // IOB / COB bottom
  sprite.setTextDatum(BL_DATUM);
  sprite.setTextColor(TFT_LIGHTGREY, bg_color);

  char tmp[24];
  snprintf(tmp, sizeof(tmp), "I: %s", iobBuf);
  sprite.drawString(tmp, 5, 170);

  snprintf(tmp, sizeof(tmp), "C: %s", cobBuf);
  sprite.drawString(tmp, 120, 170);

  // Age
  sprite.setTextDatum(TR_DATUM);
  sprite.setTextColor(okE ? TFT_DARKGREEN : TFT_DARKGREY, bg_color);

  char ageLine[20];
  snprintf(ageLine, sizeof(ageLine), "%s мин", ageBuf);
  sprite.drawString(ageLine, 315, 3);

  // Pump battery (use tmp buffer too)
  sprite.setTextDatum(TC_DATUM);
  sprite.drawRect(5, 2, 42, 18, TFT_DARKGREY);
  sprite.drawRect(2, 6, 3, 10, TFT_DARKGREY);

  if (ns.okDevStatus && ns.pumpBatValid) {
    snprintf(tmp, sizeof(tmp), "%d%%", ns.pumpBatPct);
    sprite.drawString(tmp, 48 - 42 / 2, 4);
  }
  sprite.unloadFont();

  // SGV big
  const uint16_t zoneCol = ns.zoneColor;

  // sgv big
  sprite.setTextDatum(MC_DATUM);
  sprite.loadFont(roninset2_180);

  sprite.setTextColor(ns.zoneValid ? zoneCol : TFT_DARKGREY, bg_color);
  sprite.drawString(sgvBuf, 120, 100);
  sprite.unloadFont();

  // sprite.setTextDatum(MC_DATUM);
  // sprite.loadFont(roninset2_180);

  // uint16_t sgvCol = TFT_DARKGREY;
  // if (okE && !isnan(ns.sgv_mmol)) {
  //   sgvCol = colorBySgvBounds(ns.sgv_mmol);
  // }
  // sprite.setTextColor(sgvCol, bg_color);
  // sprite.drawString(sgvBuf, 120, 100);
  // sprite.unloadFont();

  // Trend arrow (NO String dir)
  const int x0 = 320 - 42;
  const int y0 = 170 - 42;

  if (okE) {
    int a = 0;
    int dx = 0;

    const char* dir = ns.dir; // char[32]
    if      (!strcmp(dir, "Flat"))         a = 90;
    else if (!strcmp(dir, "FortyFiveUp"))  a = 45;
    else if (!strcmp(dir, "FortyFiveDown"))a = 135;
    else if (!strcmp(dir, "SingleUp"))     a = 0;
    else if (!strcmp(dir, "SingleDown"))   a = 180;
    else if (!strcmp(dir, "DoubleUp"))   { a = 0;   dx = 24; }
    else if (!strcmp(dir, "DoubleDown")) { a = 180; dx = 24; }

    drawArrow(x0, y0, a, 60, 16, 15, dx, 4, 2, sprite, TFT_SKYBLUE);
  }

  // Delta
  sprite.setTextDatum(BC_DATUM);
  sprite.loadFont(volkswagen_medium_36);
  sprite.setTextColor(okP ? TFT_SKYBLUE : TFT_DARKGREY, bg_color);
  sprite.drawString(dltBuf, x0, y0 - 50);
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

  // --- format buffers ---
  char sgvBuf[12] = "--";
  char dltBuf[12] = "--";
  char ageBuf[12] = "--";

  if (okE && !isnan(ns.sgv_mmol)) {
    snprintf(sgvBuf, sizeof(sgvBuf), "%.1f", ns.sgv_mmol);
  }

  if (okP && !isnan(ns.delta_mmol)) {
    snprintf(dltBuf, sizeof(dltBuf), "%+.1f", ns.delta_mmol);
  }

  int ageMin = 0;
  if (okE && ns.sensTime > 0) {
    time_t tnow = time(nullptr);
    if (tnow > ns.sensTime) ageMin = (int)((tnow - ns.sensTime) / 60);
  }
  if (okE) snprintf(ageBuf, sizeof(ageBuf), "%d", ageMin);

  const uint16_t bg_color = RGB565(40, 40, 40);
  sprite.fillSprite(bg_color);

  tm tml;
  bool okTime = getLocalTime(&tml, 50);

  // Time / date strings
  char timeStr[6] = "--:--";
  char dateStr[64] = "";
  char wdStr[64]   = "";

  if (okTime) {
    snprintf(timeStr, sizeof(timeStr), "%02d:%02d", tml.tm_hour, tml.tm_min);
    snprintf(dateStr, sizeof(dateStr), "%d %s %d", tml.tm_mday, MONTHS_RU[tml.tm_mon], tml.tm_year + 1900);
    snprintf(wdStr, sizeof(wdStr), "%s", WEEKDAYS_RU[tml.tm_wday]);
  }

  // Big time
  sprite.setTextColor(TFT_DARKCYAN, bg_color);
  sprite.setTextDatum(MC_DATUM);
  sprite.loadFont(roninset2_180);
  sprite.drawString(timeStr, 160, 100);
  sprite.unloadFont();

  // Date line
  sprite.setTextDatum(BL_DATUM);
  sprite.setTextColor(TFT_LIGHTGREY, bg_color);
  sprite.loadFont(comfortaa_bold_18);
  sprite.drawString(dateStr, 5, 170);

  sprite.setTextDatum(BR_DATUM);
  sprite.drawString(wdStr, 315, 170);

  // NS line top
  const uint16_t zoneCol = ns.zoneColor;

  sprite.setTextDatum(TL_DATUM);
  sprite.setTextColor(zoneCol, bg_color);
  sprite.drawString(sgvBuf, 6, 3);

  // delta (right-aligned, NO String)
  char topRight[16];
  snprintf(topRight, sizeof(topRight), "%s", dltBuf);

  sprite.setTextDatum(TR_DATUM);
  sprite.setTextColor(zoneCol, bg_color);
  sprite.drawString(topRight, 150, 3);

  // Arrow
  int x0 = 170;
  int y0 = 3 + 8;
  int a = 0;
  int dx = 0;

  if (okE) {
    const char* dir =  ns.dir;
    if      (!strcmp(dir, "Flat"))          a = 90;
    else if (!strcmp(dir, "FortyFiveUp"))   a = 45;
    else if (!strcmp(dir, "FortyFiveDown")) a = 135;
    else if (!strcmp(dir, "SingleUp"))      a = 0;
    else if (!strcmp(dir, "SingleDown"))    a = 180;
    else if (!strcmp(dir, "DoubleUp"))    { a = 0;   dx = 8; }
    else if (!strcmp(dir, "DoubleDown"))  { a = 180; dx = 8; }
  }

  drawArrow(x0, y0, a, 16, 16, 8, dx, 1, 1, sprite, zoneCol);

  // Age
  sprite.setTextDatum(TR_DATUM);
  sprite.setTextColor(okE ? TFT_DARKGREEN : TFT_DARKGREY, bg_color);

  char ageLine[20];
  snprintf(ageLine, sizeof(ageLine), "%s мин", ageBuf);
  sprite.drawString(ageLine, 315, 3);

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
  int y = 4;

  // Small QR bottom-right
  const int qr2X = W - margin - qrSmall;
  const int qr2Y = H - margin - qrSmall;

  // ===== Helpers (NO String) =====
  auto wifiEscape = [](const char* in, char* out, size_t outLen) {
    // Escape for WIFI QR: \ ; , :
    // outLen must include '\0'
    if (!out || outLen == 0) return;
    size_t w = 0;
    if (!in) { out[0] = 0; return; }

    for (size_t i = 0; in[i] != '\0'; i++) {
      char c = in[i];
      bool needSlash = (c == '\\' || c == ';' || c == ',' || c == ':');
      if (needSlash) {
        if (w + 1 >= outLen) break;
        out[w++] = '\\';
      }
      if (w + 1 >= outLen) break;
      out[w++] = c;
    }
    out[w] = '\0';
  };

  auto buildUrl = [](const IPAddress& ip, char* out, size_t outLen) {
    // "http://192.168.4.1/"
    snprintf(out, outLen, "http://%u.%u.%u.%u/",
             (unsigned)ip[0], (unsigned)ip[1], (unsigned)ip[2], (unsigned)ip[3]);
  };

  auto drawQr = [&](int bx, int by, int box, const char* payload,
                    // cache:
                    char* lastPayload, size_t lastCap,
                    QRCode& qr, uint8_t* qrData, int qrDataCap,
                    bool& ok, int& usedVer)
  {
    sprite.drawRect(bx - 2, by - 2, box + 4, box + 4, TFT_DARKGREY);

    if (!payload) payload = "";

    // rebuild only when payload changed
    if (strncmp(payload, lastPayload, lastCap) != 0) {
      strlcpy(lastPayload, payload, lastCap);
      ok = false;
      usedVer = 0;

      for (int ver = 3; ver <= 12; ver++) {
        int need = qrcode_getBufferSize(ver);
        if (need > qrDataCap) continue;

        qrcode_initText(&qr, qrData, ver, ECC_MEDIUM, payload);
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

    const int size = qr.size;     // module count
    int scale = box / size;
    if (scale < 1) scale = 1;

    const int px = size * scale;
    const int x0 = bx + (box - px) / 2;
    const int y0 = by + (box - px) / 2;

    sprite.fillRect(x0, y0, px, px, TFT_WHITE);
    for (int j = 0; j < size; j++) {
      for (int i = 0; i < size; i++) {
        if (qrcode_getModule(&qr, i, j)) {
          sprite.fillRect(x0 + i * scale, y0 + j * scale, scale, scale, TFT_BLACK);
        }
      }
    }
  };

  // ===== Data (avoid copies) =====
  const String& ssidS = portal.apSsid();
  const String& passS = portal.apPass();
  const IPAddress ip  = portal.apIP();

  const char* ssid = ssidS.c_str();
  const char* pass = passS.c_str();

  // ===== Payloads in char buffers (NO String concat) =====
  // Worst case with escaping: each char may become "\x" => x2
  // ssid max 32, pass max 64 => need enough space
  char ssidEsc[96];
  char passEsc[192];
  wifiEscape(ssid, ssidEsc, sizeof(ssidEsc));
  wifiEscape(pass, passEsc, sizeof(passEsc));

  char wifiPayload[320];
  snprintf(wifiPayload, sizeof(wifiPayload),
           "WIFI:T:WPA;S:%s;P:%s;;",
           ssidEsc, passEsc);

  char urlPayload[32];
  buildUrl(ip, urlPayload, sizeof(urlPayload));

  // ===== QR caches (NO String) =====
  static char lastWifi[320] = {0};
  static char lastUrl[32]   = {0};

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
    qr1X, qr1Y + 18, qrBig, wifiPayload,
    lastWifi, sizeof(lastWifi),
    qrWifi, qrWifiData, (int)sizeof(qrWifiData),
    qrWifiOk, qrWifiVer
  );

  // ===== Text #1 (right of WiFi QR) =====
  sprite.loadFont(comfortaa_bold_18);
  sprite.setTextDatum(TC_DATUM);
  sprite.setTextColor(TFT_RED, bg);
  sprite.drawString("ИНСТРУКЦИЯ ПО НАСТРОЙКЕ", 160, y);
  sprite.unloadFont();
  y += 18;

  sprite.loadFont(corporate_s_w10_16);
  sprite.setTextDatum(TL_DATUM);

  sprite.setTextColor(TFT_GREENYELLOW, bg);
  sprite.drawString("1) Подключитесь к Wi-Fi:", t1X, y);
  y += 18;

  sprite.setTextColor(TFT_ORANGE, bg);
  sprite.drawString("SSID:", t1X, y);
  sprite.setTextColor(TFT_SKYBLUE, bg);
  sprite.drawString(ssid, t1X + 50, y);
  y += 18;

  sprite.setTextColor(TFT_ORANGE, bg);
  sprite.drawString("PASS:", t1X, y);
  sprite.setTextColor(TFT_SKYBLUE, bg);
  sprite.drawString(pass, t1X + 50, y);
  sprite.unloadFont();

  // ===== Draw QR #2 (URL) =====
  drawQr(
    qr2X, qr2Y, qrSmall, urlPayload,
    lastUrl, sizeof(lastUrl),
    qrUrl, qrUrlData, (int)sizeof(qrUrlData),
    qrUrlOk, qrUrlVer
  );

  // ===== Text #2 (left of bottom-right QR) =====
  sprite.setTextDatum(BR_DATUM);
  sprite.loadFont(corporate_s_w10_16);

  sprite.setTextColor(TFT_GREENYELLOW, bg);
  sprite.drawString("2) Если портал не открылся,", 320 - qrSmall - 10, 170 - 2 - 18);
  sprite.drawString("перейдите на ",               320 - qrSmall - 10 - 18 * 8, 170 - 2);

  sprite.setTextColor(TFT_SKYBLUE, bg);
  sprite.drawString(urlPayload, 320 - qrSmall - 10, 170 - 2);

  sprite.unloadFont();
  sprite.pushSprite(0, 0);
}

