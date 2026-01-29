#include "ui_settings.h"
#include "fonts.h"

#include <cstring>
#include <cstdio>

#include "captive_portal.h"   // чтобы видеть CaptivePortal::apSsid/apPass/apIP
#include <IPAddress.h>

// #if __has_include("lv_qrcode.h")
//   #include "lv_qrcode.h"
// #elif __has_include("src/libs/qrcode/lv_qrcode.h")
#include <lvgl.h>
  #include "src/libs/qrcode/lv_qrcode.h"
// #elif __has_include("lvgl/src/libs/qrcode/lv_qrcode.h")
//   #include "lvgl/src/libs/qrcode/lv_qrcode.h"
// #else
//   #error "LVGL QRCode header not found. Check LVGL library/port."
// #endif

// ================= Layout =================
static constexpr int W = 320;
static constexpr int H = 170;

static constexpr int margin = 6;
static constexpr int gap    = 6;

static constexpr int qrBig   = 104;
static constexpr int qrSmall = 64;

// ================= LVGL objects =================
static lv_obj_t* settings_screen = nullptr;

static lv_obj_t* qr_wifi = nullptr;
static lv_obj_t* qr_url  = nullptr;

static lv_obj_t* label_title = nullptr;
static lv_obj_t* label_l1    = nullptr;
static lv_obj_t* label_ssid  = nullptr;
static lv_obj_t* label_pass  = nullptr;

static lv_obj_t* label_l2a   = nullptr;
static lv_obj_t* label_l2b   = nullptr;
static lv_obj_t* label_url   = nullptr;

// ================= Caches =================
static char lastWifiPayload[320] = {0};
static char lastUrlPayload[32]   = {0};

// ================= Helpers =================
static void wifiEscape(const char* in, char* out, size_t outLen) {
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
}

static void buildUrl(const IPAddress& ip, char* out, size_t outLen) {
  snprintf(out, outLen, "http://%u.%u.%u.%u/",
           (unsigned)ip[0], (unsigned)ip[1], (unsigned)ip[2], (unsigned)ip[3]);
}

static void set_label_text_safe(lv_obj_t* lbl, const char* txt) {
  if (!lbl) return;
  lv_label_set_text(lbl, txt ? txt : "");
}

static void update_qr_if_changed(lv_obj_t* qr, const char* payload,
                                char* lastBuf, size_t lastCap) {
  if (!qr || !payload || !lastBuf || lastCap == 0) return;

  if (strncmp(payload, lastBuf, lastCap) == 0) return; // не менялось

  // обновим кеш
  strlcpy(lastBuf, payload, lastCap);

  // обновим QR
  lv_qrcode_set_data(qr, payload);
}

// ================= Public API =================
lv_obj_t* ui_settings_create() {
  // Screen
  settings_screen = lv_obj_create(nullptr);

// lv_obj_remove_style_all(settings_screen);
lv_obj_set_style_bg_color(settings_screen, lv_color_black(), LV_PART_MAIN);
lv_obj_set_style_bg_opa(settings_screen, LV_OPA_COVER, LV_PART_MAIN);
lv_obj_clear_flag(settings_screen, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_set_size(settings_screen, W, H);
  lv_obj_set_style_bg_color(settings_screen, lv_color_black(), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(settings_screen, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_clear_flag(settings_screen, LV_OBJ_FLAG_SCROLLABLE);

// lv_obj_t* border = lv_obj_create(settings_screen);
// lv_obj_remove_style_all(border);
// lv_obj_set_style_border_width(border, 1, 0);
// lv_obj_set_style_border_color(border, lv_color_white(), 0);
// lv_obj_set_style_bg_opa(border, LV_OPA_TRANSP, 0);
// lv_obj_set_size(border, 320, 170);
// lv_obj_set_pos(border, 0, 0);


  // ----- Big WiFi QR (top-left) -----
  const int qr1X = margin;
  const int qr1Y = margin + 18;

  qr_wifi = lv_qrcode_create(settings_screen);
lv_qrcode_set_size(qr_wifi, qrBig);
lv_qrcode_set_dark_color(qr_wifi, lv_color_black());
lv_qrcode_set_light_color(qr_wifi, lv_color_white());
// опционально: quiet zone
lv_qrcode_set_quiet_zone(qr_wifi, true);
  lv_obj_set_pos(qr_wifi, qr1X, qr1Y);
  lv_obj_set_style_border_width(qr_wifi, 2, LV_PART_MAIN);
  lv_obj_set_style_border_color(qr_wifi, lv_palette_darken(LV_PALETTE_GREY, 3), LV_PART_MAIN);
  //lv_obj_add_flag(qr_url,  LV_OBJ_FLAG_HIDDEN);

  // ----- Text block (right of big QR) -----
  const int t1X = qr1X + qrBig + gap;
  int y = 4;

  label_title = lv_label_create(settings_screen);
  lv_obj_set_pos(label_title, 1, y);
  lv_obj_set_width(label_title, W - 1 - margin);
  lv_label_set_long_mode(label_title, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_font(label_title, &comfortaa_bold_18, LV_PART_MAIN);
  lv_obj_set_style_text_color(label_title, lv_color_hex(0xFF0000), LV_PART_MAIN);
  lv_label_set_text(label_title, "ИНСТРУКЦИЯ ПО НАСТРОЙКЕ");
  y += 22;

  label_l1 = lv_label_create(settings_screen);

  

  lv_obj_set_pos(label_l1, t1X, y);
  lv_obj_set_width(label_l1, W - t1X - margin);
  lv_obj_set_style_text_font(label_l1, &corporate_s_w10_16, LV_PART_MAIN);
  lv_obj_set_style_text_color(label_l1, lv_color_hex(0xCCFF33), LV_PART_MAIN); // greenyellow-ish

// lv_obj_remove_style_all(label_l1);
lv_obj_set_style_text_opa(label_l1, LV_OPA_COVER, LV_PART_MAIN);
lv_obj_set_style_blend_mode(label_title, LV_BLEND_MODE_NORMAL, LV_PART_MAIN);

  lv_label_set_text(label_l1, "1) Подключитесь к Wi-Fi:");
  y += 18;

  // SSID line
  {
    lv_obj_t* lblSsidKey = lv_label_create(settings_screen);
    lv_obj_set_pos(lblSsidKey, t1X, y);
    lv_obj_set_style_text_font(lblSsidKey, &corporate_s_w10_16, LV_PART_MAIN);
    lv_obj_set_style_text_color(lblSsidKey, lv_color_hex(0xFF9900), LV_PART_MAIN); // orange
    lv_label_set_text(lblSsidKey, "SSID:");

    label_ssid = lv_label_create(settings_screen);
    lv_obj_set_pos(label_ssid, t1X + 50, y);
    lv_obj_set_width(label_ssid, W - (t1X + 50) - margin);
    lv_obj_set_style_text_font(label_ssid, &corporate_s_w10_16, LV_PART_MAIN);
    lv_obj_set_style_text_color(label_ssid, lv_color_hex(0x66CCFF), LV_PART_MAIN); // skyblue-ish
    lv_label_set_long_mode(label_ssid, LV_LABEL_LONG_DOT);
    lv_label_set_text(label_ssid, "");
    y += 18;
  }

  // PASS line
  {
    lv_obj_t* lblPassKey = lv_label_create(settings_screen);
    lv_obj_set_pos(lblPassKey, t1X, y);
    lv_obj_set_style_text_font(lblPassKey, &corporate_s_w10_16, LV_PART_MAIN);
    lv_obj_set_style_text_color(lblPassKey, lv_color_hex(0xFF9900), LV_PART_MAIN);
    lv_label_set_text(lblPassKey, "PASS:");

    label_pass = lv_label_create(settings_screen);
    lv_obj_set_pos(label_pass, t1X + 50, y);
    lv_obj_set_width(label_pass, W - (t1X + 50) - margin);
    lv_obj_set_style_text_font(label_pass, &corporate_s_w10_16, LV_PART_MAIN);
    lv_obj_set_style_text_color(label_pass, lv_color_hex(0x66CCFF), LV_PART_MAIN);
    lv_label_set_long_mode(label_pass, LV_LABEL_LONG_DOT);
    lv_label_set_text(label_pass, "");
  }

  // ----- Small URL QR (bottom-right) -----
  const int qr2X = W - margin - qrSmall;
  const int qr2Y = H - margin - qrSmall;

  qr_url = lv_qrcode_create(settings_screen);
lv_qrcode_set_size(qr_url, qrSmall);
lv_qrcode_set_dark_color(qr_url, lv_color_black());
lv_qrcode_set_light_color(qr_url, lv_color_white());
lv_qrcode_set_quiet_zone(qr_url, true);
  lv_obj_set_pos(qr_url, qr2X, qr2Y);
  lv_obj_set_style_border_width(qr_url, 2, LV_PART_MAIN);
  lv_obj_set_style_border_color(qr_url, lv_palette_darken(LV_PALETTE_GREY, 3), LV_PART_MAIN);
//lv_obj_add_flag(qr_wifi, LV_OBJ_FLAG_HIDDEN);

  // ----- Bottom text (left of small QR) -----
  const int bx = qr2X - 10;       // right edge for text
  const int by2 = H - 2 - 18;
  const int by3 = H - 2;

  label_l2a = lv_label_create(settings_screen);
  lv_obj_set_style_text_font(label_l2a, &corporate_s_w10_16, LV_PART_MAIN);
  lv_obj_set_style_text_color(label_l2a, lv_color_hex(0xCCFF33), LV_PART_MAIN);
  lv_label_set_text(label_l2a, "2) Если портал не открылся,");
  lv_obj_align(label_l2a, LV_ALIGN_BOTTOM_RIGHT, -(qrSmall + 10), -18);

  label_l2b = lv_label_create(settings_screen);
  lv_obj_set_style_text_font(label_l2b, &corporate_s_w10_16, LV_PART_MAIN);
  lv_obj_set_style_text_color(label_l2b, lv_color_hex(0xCCFF33), LV_PART_MAIN);
  lv_label_set_text(label_l2b, "перейдите на");
  lv_obj_align(label_l2b, LV_ALIGN_BOTTOM_RIGHT, -(qrSmall + 10), 0);

  label_url = lv_label_create(settings_screen);
  lv_obj_set_style_text_font(label_url, &corporate_s_w10_16, LV_PART_MAIN);
  lv_obj_set_style_text_color(label_url, lv_color_hex(0x66CCFF), LV_PART_MAIN);
  lv_label_set_text(label_url, "");
  lv_obj_align(label_url, LV_ALIGN_BOTTOM_RIGHT, -(qrSmall + 10) + 0, 0); // будет “на той же линии”, но сдвинем чуть позже

  // чуть подправим позицию URL, чтобы он был правее "перейдите на"
  // самый простой способ — сделать его “плавающим” рядом:
  lv_obj_set_x(label_url, lv_obj_get_x(label_l2b) + 110);
  lv_obj_set_y(label_url, lv_obj_get_y(label_l2b));

  // стартовые QR (пустые)
  lv_qrcode_update(qr_wifi, "", 0);
  lv_qrcode_update(qr_url,  "", 0);

  return settings_screen;
}

void ui_settings_update_from_portal(const CaptivePortal& portal) {
  if (!settings_screen) return;

  // Достаём данные (тут String внутри портала — ок, мы их не конкатеним)
  const String& ssidS = portal.apSsid();
  const String& passS = portal.apPass();
  const IPAddress ip  = portal.apIP();

  const char* ssid = ssidS.c_str();
  const char* pass = passS.c_str();

  // Обновим видимые строки
  set_label_text_safe(label_ssid, ssid);
  set_label_text_safe(label_pass, pass);

  // Соберём payload (без String concat)
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

  set_label_text_safe(label_url, urlPayload);

  // QR обновляем только если изменилось
  update_qr_if_changed(qr_wifi, wifiPayload, lastWifiPayload, sizeof(lastWifiPayload));
  update_qr_if_changed(qr_url,  urlPayload,  lastUrlPayload,  sizeof(lastUrlPayload));
}
