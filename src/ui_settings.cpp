// Экран подключения к веб-настройкам: реквизиты Wi-Fi и QR-коды сети и адреса портала.
#include "ui_settings.h"
#include "ui_layout.h"
#include "fonts.h"

#include <cstring>
#include <cstdio>

#include "captive_portal.h"
#include <IPAddress.h>

#include <lvgl.h>
  #include "src/libs/qrcode/lv_qrcode.h"

static constexpr int margin = 6;
static constexpr int gap    = 6;

static constexpr int qrBig   = 104;
static constexpr int qrSmall = 64;

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

static char lastWifiPayload[320] = {0};
static char lastUrlPayload[32]   = {0};

// Экранируем специальные символы SSID и пароля для формата Wi-Fi QR.
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

// Пересоздаём содержимое QR только при изменении строки данных.
static void update_qr_if_changed(lv_obj_t* qr, const char* payload,
                                char* lastBuf, size_t lastCap) {
  if (!qr || !payload || !lastBuf || lastCap == 0) return;

  if (strncmp(payload, lastBuf, lastCap) == 0) return;

  strlcpy(lastBuf, payload, lastCap);

  lv_qrcode_set_data(qr, payload);
}

lv_obj_t* ui_settings_create() {
  const int W = ui_layout::content_width();
  const int H = ui_layout::content_height(false);

  settings_screen = lv_obj_create(nullptr);
  ui_layout::prepare_screen(settings_screen, false);

lv_obj_set_style_bg_color(settings_screen, lv_color_black(), LV_PART_MAIN);
lv_obj_set_style_bg_opa(settings_screen, LV_OPA_COVER, LV_PART_MAIN);
lv_obj_clear_flag(settings_screen, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_set_size(settings_screen, W, lv_display_get_vertical_resolution(nullptr));
  lv_obj_set_style_bg_color(settings_screen, lv_color_black(), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(settings_screen, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_clear_flag(settings_screen, LV_OBJ_FLAG_SCROLLABLE);

  // Большой QR подключает телефон к Wi-Fi устройства.
  const int qr1X = margin;
  const int qr1Y = margin + 18;

  qr_wifi = lv_qrcode_create(settings_screen);
lv_qrcode_set_size(qr_wifi, qrBig);
lv_qrcode_set_dark_color(qr_wifi, lv_color_black());
lv_qrcode_set_light_color(qr_wifi, lv_color_white());

lv_qrcode_set_quiet_zone(qr_wifi, true);
  lv_obj_set_pos(qr_wifi, qr1X, qr1Y);
  lv_obj_set_style_border_width(qr_wifi, 2, LV_PART_MAIN);
  lv_obj_set_style_border_color(qr_wifi, lv_palette_darken(LV_PALETTE_GREY, 3), LV_PART_MAIN);

  const int t1X = qr1X + qrBig + gap;
  int y = 4;

  label_title = lv_label_create(settings_screen);
  lv_obj_set_pos(label_title, 1, y);
  lv_obj_set_width(label_title, W - 1 - margin);
  lv_label_set_long_mode(label_title, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_font(label_title, &comfortaa_bold_18, LV_PART_MAIN);
  lv_obj_set_style_text_color(label_title, lv_color_hex(0xFF0000), LV_PART_MAIN);
#if defined(BOARD_TDISPLAY_S3)
  lv_obj_set_pos(label_title, margin, y);
  lv_obj_set_width(label_title, W - 2 * margin);
  lv_obj_set_style_text_align(label_title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_label_set_text(label_title, "НАСТРОЙКИ");
#else
  lv_label_set_text(label_title, "ИНСТРУКЦИЯ ПО НАСТРОЙКЕ");
#endif
  y += 22;

  label_l1 = lv_label_create(settings_screen);

  lv_obj_set_pos(label_l1, t1X, y);
  lv_obj_set_width(label_l1, W - t1X - margin);
  lv_obj_set_style_text_font(label_l1, &corporate_s_w10_16, LV_PART_MAIN);
  lv_obj_set_style_text_color(label_l1, lv_color_hex(0xCCFF33), LV_PART_MAIN);

lv_obj_set_style_text_opa(label_l1, LV_OPA_COVER, LV_PART_MAIN);
lv_obj_set_style_blend_mode(label_title, LV_BLEND_MODE_NORMAL, LV_PART_MAIN);

  lv_label_set_text(label_l1, "1) Подключитесь к Wi-Fi:");
  y += 18;

  {
    lv_obj_t* lblSsidKey = lv_label_create(settings_screen);
    lv_obj_set_pos(lblSsidKey, t1X, y);
    lv_obj_set_style_text_font(lblSsidKey, &corporate_s_w10_16, LV_PART_MAIN);
    lv_obj_set_style_text_color(lblSsidKey, lv_color_hex(0xFF9900), LV_PART_MAIN);
    lv_label_set_text(lblSsidKey, "SSID:");

    label_ssid = lv_label_create(settings_screen);
    lv_obj_set_pos(label_ssid, t1X + 50, y);
    lv_obj_set_width(label_ssid, W - (t1X + 50) - margin);
    lv_obj_set_style_text_font(label_ssid, &corporate_s_w10_16, LV_PART_MAIN);
    lv_obj_set_style_text_color(label_ssid, lv_color_hex(0x66CCFF), LV_PART_MAIN);
    lv_label_set_long_mode(label_ssid, LV_LABEL_LONG_DOT);
    lv_label_set_text(label_ssid, "");
    y += 18;
  }

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

  // Малый QR открывает HTTP-адрес портала; подписи располагаются слева от него.
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

  const int bx = qr2X - 10;
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
  lv_obj_align(label_url, LV_ALIGN_BOTTOM_RIGHT, -(qrSmall + 10) + 0, 0);

#if defined(BOARD_TDISPLAY_S3)

  constexpr int lineStep = 18;
  const int textTop = H - margin - 2 * lineStep;
  lv_label_set_text(label_l2a, "2) Откройте в браузере:");
  lv_obj_add_flag(label_l2b, LV_OBJ_FLAG_HIDDEN);
  lv_obj_t* lines[] = {label_l2a, label_url};
  for (int i = 0; i < 2; ++i) {
    lv_obj_set_align(lines[i], LV_ALIGN_TOP_LEFT);
    lv_obj_set_pos(lines[i], margin, textTop + i * lineStep);
    lv_obj_set_width(lines[i], qr2X - gap - margin);
    lv_obj_set_style_text_align(lines[i], LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    lv_label_set_long_mode(lines[i], LV_LABEL_LONG_CLIP);
  }
#elif defined(BOARD_M5CORE_S3)

  constexpr int lineStep = 18;
  const int textTop = H - margin - 4 * lineStep;
  lv_obj_t* lines[] = {label_l2a, label_l2b, label_url};
  for (int i = 0; i < 3; ++i) {
    lv_obj_set_align(lines[i], LV_ALIGN_TOP_LEFT);
    lv_obj_set_pos(lines[i], margin, textTop + i * lineStep);
    lv_obj_set_width(lines[i], qr2X - gap - margin);
    lv_label_set_long_mode(lines[i], LV_LABEL_LONG_CLIP);
  }
#else

  lv_obj_set_x(label_url, lv_obj_get_x(label_l2b) + 110);
  lv_obj_set_y(label_url, lv_obj_get_y(label_l2b));
#endif

  lv_qrcode_update(qr_wifi, "", 0);
  lv_qrcode_update(qr_url,  "", 0);

  return settings_screen;
}

void ui_settings_update_from_portal(const CaptivePortal& portal) {
  if (!settings_screen) return;

  const String& ssidS = portal.apSsid();
  const String& passS = portal.apPass();
  const IPAddress ip  = portal.apIP();

  const char* ssid = ssidS.c_str();
  const char* pass = passS.c_str();

  set_label_text_safe(label_ssid, ssid);
  set_label_text_safe(label_pass, pass);

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

  update_qr_if_changed(qr_wifi, wifiPayload, lastWifiPayload, sizeof(lastWifiPayload));
  update_qr_if_changed(qr_url,  urlPayload,  lastUrlPayload,  sizeof(lastUrlPayload));
}
