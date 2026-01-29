// ======================= nightscout_client.h =======================
#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

// GyverGSON
//#define GSON_NO_LIMITS
#define GSON_ENABLE_HASH
#include <GSON.h>
#include <StringUtils.h>   // su::SH()

#ifndef NS_LOGI
  #define NS_LOGI(...) do { Serial.printf(__VA_ARGS__); Serial.print("\n"); } while(0)
#endif
#ifndef NS_LOGW
  #define NS_LOGW(...) do { Serial.printf(__VA_ARGS__); Serial.print("\n"); } while(0)
#endif
#ifndef NS_LOGE
  #define NS_LOGE(...) do { Serial.printf(__VA_ARGS__); Serial.print("\n"); } while(0)
#endif

struct NSConfig {
  // Nightscout host/site (может быть без схемы или со схемой)
  // пример: "jxxxx.nightscout-jino.ru" или "https://jxxxx..."
  String  baseUrl;

  bool    preferHttpsIfNoScheme = true;
  uint32_t httpTimeoutMs = 8000;
  bool    tlsInsecure = true;

  // 4 границы (mmol/L) для подсветки SGV (как в AppSettings::sgvBounds)
  float sgvBounds[4] = {3.9f, 4.5f, 8.5f, 10.0f};
};

struct NightscoutData {
  // для логов/диагностики
  bool ok = false;
  int  http = 0;

  // данные (из /api/v2/properties)
  float    sgv_mmol = NAN;          // bgnow.sgvs[0].scaled
  float    delta_mmol = NAN;        // bgnow.delta.scaled
  uint64_t update_time_ms = 0;      // bgnow.mills
  char     trend[24] = "N/A";       // direction.value (например "FortyFiveDown")

  float    iob = NAN;               // iob.iob
  float    cob = NAN;               // cob.cob

  // батарея (лучший доступный процент/вольтаж)
  int      pumpBatPct = -1;         // %, -1 если нет
  float    pumpBatV   = 0.0f;       // pump.pump.battery.voltage (если приходит)

  // границы для раскраски (прокидываем на экраны)
  float    sgvBounds[4] = {3.9f, 4.5f, 8.5f, 10.0f};
};

class NightscoutClient {
public:
  void begin(const NSConfig& cfg);
  void requestUpdate();
  bool tick(NightscoutData& out);

private:
  NSConfig _cfg;

  // helpers
  String buildBase(String url) const;
  String makePropsPath(const String& base) const;
  void sanitizeJson(String& s) const;
  bool splitUrl(const String& fullUrl, bool& https, String& host, uint16_t& port, String& path) const;

  // parsing
  bool parseV2Properties(const String& json, NightscoutData& out) const;

  // FSM: один запрос
  enum class Phase : uint8_t { Idle, Connect, Send, ReadHeaders, ReadBody, Done, Error };

  struct Req {
    Phase   ph = Phase::Idle;

    bool https = false;
    String host;
    uint16_t port = 0;
    String path;

    int httpCode = 0;
    int contentLen = -1;
    bool chunked = false;

    String headerLine;
    String body;

    uint32_t tStart = 0;
    uint32_t tLastIO = 0;

    WiFiClientSecure cs;
    WiFiClient cp;
  };

  Req _r;
  bool _wantUpdate = false;

  // фатальная ошибка DNS/SSL/коннекта — не спамим сеть
  bool _hostError = false;

  void startReq(const String& base);
  bool stepReq(NightscoutData& out);
  void abortReq();

  static constexpr size_t READ_CHUNK = 512;
  static constexpr size_t BODY_MAX   = 32 * 1024; // /api/v2/properties бывает жирный
};
