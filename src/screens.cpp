#include "screens.h"
#include <TimeLib.h>

#include "font_roninset2_180.h"
#include "font_comfortaa_bold_18.h"
#include "font_volkswagen_medium_36.h"

#include "logs.h"

#define RGB565(r, g, b) (((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3))

void drawMainScreen(TFT_eSprite& sprite, const NightscoutData& ns) {
  time_t datetimenow = now();

  const bool okE = ns.okEntries;
  const bool okP = ns.okProps;

  String sgv = (okE && !isnan(ns.sgv_mmol))   ? String(ns.sgv_mmol, 1)   : "--";
  

  String dir = okE ? String(ns.dir) : "--";

  String dlt = (okP && !isnan(ns.delta_mmol)) ? String(ns.delta_mmol, 1) : "--";
  String iob = (okP && !isnan(ns.iob)) ? String(ns.iob, 2) : "--";
  String cob = (okP && !isnan(ns.cob)) ? String(ns.cob, 1) : "--";

  int ageMin = 0;
  if (okE && ns.sensTime > 0) {
    time_t tnow = now();
    if (tnow > ns.sensTime) ageMin = (tnow - ns.sensTime) / 60;
  }
  String ageStr = okE ? String(ageMin) : "--";

  //String sgv  = ok ? ns.sgv       : "--";
  //int age     = okE ? ns.sensTime   : 0;
  //String iob  = ok ? ns.iob       : "--";
  //String cob  = ok ? ns.cob       : "--";
  //String dir  = ok ? ns.direction : "Flat";
  //String dlt  = ok ? ns.delta     : "--";

  uint16_t bg_color = RGB565(40, 40, 40);
  sprite.fillSprite(bg_color);

  sprite.drawRect(5, 2, 25, 16, TFT_DARKGREY);
  sprite.drawRect(2, 5, 3, 10, TFT_DARKGREY);

  // Время HH:MM
  char buf[6];
  snprintf(buf, sizeof(buf), "%02d:%02d", hour(datetimenow), minute(datetimenow));

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
  sprite.unloadFont();

  // sgv крупно
  sprite.setTextDatum(MC_DATUM);
  sprite.loadFont(roninset2_180);
  sprite.setTextColor(okE ? TFT_GREEN : TFT_DARKGREY, bg_color);
  sprite.drawString(sgv, 120, 100);
  sprite.unloadFont();

  // Стрелка тренда (как было)
  int x0 = 320 - 42;
  int y0 = 170 - 42;

  if (okE) {
    if (dir == "Flat") {
      sprite.drawWedgeLine(x0-30, y0, x0+30, y0, 4, 2, TFT_SKYBLUE);
      sprite.drawWedgeLine(x0+15, y0+8, x0+30, y0, 4, 2, TFT_SKYBLUE);
      sprite.drawWedgeLine(x0+15, y0-8, x0+30, y0, 4, 2, TFT_SKYBLUE);
    } else if (dir == "FortyFiveUp") {
      sprite.drawWedgeLine(x0-21, y0+21, x0+21, y0-21, 4, 2, TFT_SKYBLUE);
      sprite.drawWedgeLine(x0+16, y0-5, x0+21, y0-21, 4, 2, TFT_SKYBLUE);
      sprite.drawWedgeLine(x0+5, y0-16, x0+21, y0-21, 4, 2, TFT_SKYBLUE);
    } else if (dir == "FortyFiveDown") {
      sprite.drawWedgeLine(x0-21, y0-21, x0+21, y0+21, 4, 2, TFT_SKYBLUE);
      sprite.drawWedgeLine(x0+5, y0+16, x0+21, y0+21, 4, 2, TFT_SKYBLUE);
      sprite.drawWedgeLine(x0+16, y0+5, x0+21, y0+21, 4, 2, TFT_SKYBLUE);
    } else if (dir == "SingleUp") {
      sprite.drawWedgeLine(x0, y0+30, x0, y0-30, 4, 2, TFT_SKYBLUE);
      sprite.drawWedgeLine(x0+8, y0-15, x0, y0-30, 4, 2, TFT_SKYBLUE);
      sprite.drawWedgeLine(x0-8, y0-15, x0, y0-30, 4, 2, TFT_SKYBLUE);
    } else if (dir == "SingleDown") {
      sprite.drawWedgeLine(x0, y0-30, x0, y0+30, 4, 2, TFT_SKYBLUE);
      sprite.drawWedgeLine(x0-8, y0+15, x0, y0+30, 4, 2, TFT_SKYBLUE);
      sprite.drawWedgeLine(x0+8, y0+15, x0, y0+30, 4, 2, TFT_SKYBLUE);
    } else if (dir == "DoubleUp") {
      sprite.drawWedgeLine(x0-12,   y0+30, x0-12, y0-30, 4, 2, TFT_SKYBLUE);
      sprite.drawWedgeLine(x0+8-12, y0-15, x0-12, y0-30, 4, 2, TFT_SKYBLUE);
      sprite.drawWedgeLine(x0-8-12, y0-15, x0-12, y0-30, 4, 2, TFT_SKYBLUE);

      sprite.drawWedgeLine(x0+10,   y0+30, x0+12, y0-30, 4, 2, TFT_SKYBLUE);
      sprite.drawWedgeLine(x0+8+10, y0-15, x0+12, y0-30, 4, 2, TFT_SKYBLUE);
      sprite.drawWedgeLine(x0-8+10, y0-15, x0+12, y0-30, 4, 2, TFT_SKYBLUE);
    } else if (dir == "DoubleDown") {
      sprite.drawWedgeLine(x0-12,   y0-30, x0-12, y0+30, 4, 2, TFT_SKYBLUE);
      sprite.drawWedgeLine(x0-8-12, y0+15, x0-12, y0+30, 4, 2, TFT_SKYBLUE);
      sprite.drawWedgeLine(x0+8-12, y0+15, x0-12, y0+30, 4, 2, TFT_SKYBLUE);

      sprite.drawWedgeLine(x0+12,   y0-30, x0+12, y0+30, 4, 2, TFT_SKYBLUE);
      sprite.drawWedgeLine(x0-8+12, y0+15, x0+12, y0+30, 4, 2, TFT_SKYBLUE);
      sprite.drawWedgeLine(x0+8+12, y0+15, x0+12, y0+30, 4, 2, TFT_SKYBLUE);
    }
  }

  // Delta
  sprite.setTextDatum(BC_DATUM);
  sprite.loadFont(volkswagen_medium_36);
  sprite.setTextColor(okE ? TFT_SKYBLUE : TFT_DARKGREY, bg_color);
  sprite.drawString(dlt, x0, y0 - 50);

  // if (ok && ns.pumpBatValid) {
  //   sprite.drawString("P:" + String(ns.pumpBatPct) + "%", 6, 2);
  // }

  sprite.unloadFont();

  sprite.pushSprite(0, 0);
}




////

void drawClockScreen(TFT_eSprite& sprite) {
  sprite.fillSprite(TFT_BLACK);
  sprite.setTextDatum(MC_DATUM);
  sprite.setTextColor(TFT_GREEN, TFT_DARKGREEN);
  sprite.drawString("2222", 120, 100);
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

  // Максимально мелко и плотно
  sprite.setTextFont(1);
  sprite.setTextSize(1);
  sprite.setTextDatum(TL_DATUM);

  const int lineH = sprite.fontHeight(); // максимально плотный шаг
  const int x = 0;
  int y = 0;

  const int screenW = 320;
  const int screenH = 170;

  int linesLeft = screenH / lineH;       // сколько строк всего влезает

  // Печатаем самые новые сверху, но с переносом каждая запись может занять несколько строк
  for (int i = 0; i < (int)logs.count() && linesLeft > 0; i++) {
    LogEntry e = logs.getNewestFirst(i);

    uint16_t col = TFT_GREEN;
    if (e.level == LogLevel::Warn)  col = TFT_YELLOW;
    if (e.level == LogLevel::Error) col = TFT_RED;   // только ошибки красным

    sprite.setTextColor(col, bg);

    //char tag = (e.level == LogLevel::Info) ? 'I' : (e.level == LogLevel::Warn) ? 'W' : 'E';
    uint32_t sec = e.ms / 1000;

    //String msg = String(sec) + " " + String(tag) + " " + e.msg;
    String msg = String(sec) + " "  + e.msg;

    // рисуем с переносом
    int used = drawWrappedLine(sprite, msg, x, y, screenW, lineH, linesLeft);
    y += used * lineH;
    linesLeft -= used;
  }

  sprite.pushSprite(0, 0);
}



