#include "nightscout_client.h"

void NightscoutClient::begin(const NSConfig& cfg) {
  _cfg = cfg;
}

String NightscoutClient::buildBase(String url) const {
  url.trim();
  if (url.isEmpty()) return url;

  if (!url.startsWith("http://") && !url.startsWith("https://")) {
    url = String(_cfg.preferHttpsIfNoScheme ? "https://" : "http://") + url;
  }
  if (url.endsWith("/")) url.remove(url.length() - 1);
  return url;
}

String NightscoutClient::makeEntriesUrl(const String& base) const {
  String u = base;
  if (_cfg.sgvOnly) {
    u += "/api/v1/entries.json?find[type][$eq]=sgv&count=10";
  } else {
    u += "/api/v1/entries.json?count=10";
  }

  if (!_cfg.token.isEmpty()) {
    // entries всегда уже с "?"
    u += "&token=";
    u += _cfg.token;
  }
  return u;
}

String NightscoutClient::makePropsUrl(const String& base) const {
  String u = base;

  if (_cfg.infoLine == 2) {
    u += "/api/v2/properties/iob,cob,delta,loop,basal";
  } else if (_cfg.infoLine == 3) {
    u += "/api/v2/properties/iob,cob,delta,openaps,basal";
  } else {
    u += "/api/v2/properties/iob,cob,delta,basal";
  }

  if (!_cfg.token.isEmpty()) {
    // props без query до токена
    u += "?token=";
    u += _cfg.token;
  }
  return u;
}

void NightscoutClient::sanitizeJson(String& s) const {
  // как у вас: контрольные символы -> пробел
  for (int i = 0; i < (int)s.length(); i++) {
    if ((uint8_t)s[i] < 32) s.setCharAt(i, ' ');
  }
  s.replace("\\u0000", " ");
  s.replace("\\u000b", " ");
  s.replace("\\u0032", " ");

  // ваша “защита date: ... .123”
  int ndx = 0;
  int sr = s.indexOf("\"date\":", ndx);
  while (sr != -1) {
    ndx = sr + 1;
    if (sr + 20 < (int)s.length()) {
      if (s.charAt(sr + 20) == '.') {
        s.remove(sr + 20, 1);
        while (sr + 20 < (int)s.length()) {
          char c = s.charAt(sr + 20);
          if (c >= '0' && c <= '9') s.remove(sr + 20, 1);
          else break;
        }
      }
    }
    sr = s.indexOf("\"date\":", ndx);
  }
}

bool NightscoutClient::httpGet(const String& url, String& body, int& httpCode) const {
  HTTPClient http;
  http.setTimeout(_cfg.httpTimeoutMs);

  Serial.printf("[NS] GET %s\n", url.c_str());

  // Важно: клиент должен жить ДО КОНЦА функции, а не внутри if-блока
  WiFiClientSecure clientSecure;
  WiFiClient       clientPlain;

  bool begun = false;
  if (url.startsWith("https://")) {
    clientSecure.setInsecure();                 // если нужен реальный TLS — поставьте CA вместо setInsecure()
    begun = http.begin(clientSecure, url);
  } else {
    begun = http.begin(clientPlain, url);
  }

  if (!begun) {
    Serial.println("[NS] http.begin FAILED");
    httpCode = -1;
    body = "";
    return false;
  }

  uint32_t t0 = millis();
  httpCode = http.GET();
  uint32_t dt = millis() - t0;

  // Печатаем код один раз, единообразно
  Serial.printf("[NS] code=%d, %lu ms\n", httpCode, (unsigned long)dt);

  // Читаем body всегда (даже при 400/401/500) — это сильно помогает диагностике
  body = http.getString();
  Serial.printf("[NS] bodyLen=%u\n", (unsigned)body.length());
  if (body.length() && body.length() < 512) {   // чтобы не заспамить Serial
    Serial.printf("[NS] body: %s\n", body.c_str());
  } else if (body.length() >= 512) {
    Serial.println("[NS] body: <too long, skipped>");
  }

  http.end();

  // Успех — только 200 OK (при желании можно разрешить 204 и т.п.)
  if (httpCode == HTTP_CODE_OK) return true;

  // Пояснение ошибки (для отрицательных кодов errorToString часто пустой)
  if (httpCode < 0) {
    Serial.printf("[NS] transport error: %s\n", http.errorToString(httpCode).c_str());
  } else {
    Serial.println("[NS] HTTP error");
  }
  return false;
}

bool NightscoutClient::parseEntries(const String& json, NightscoutData& out, ArduinoJson::DynamicJsonDocument& doc) const {
  DeserializationError e = deserializeJson(doc, json);
  if (e) {
    NS_LOGE("[NS] entries JSON parse failed: %s", e.c_str());
    return false;
  }

  JsonArray arr = doc.as<JsonArray>();
  if (arr.isNull() || arr.size() == 0) {
    NS_LOGE("[NS] entries empty");
    return false;
  }

  // найти первый объект с "sgv" (как у вас)
  int sgvindex = 0;
  JsonObject obj;
  do {
    obj = doc[sgvindex].as<JsonObject>();
    sgvindex++;
  } while (!obj.containsKey("sgv") && (sgvindex < (int)arr.size() - 1));
  sgvindex--;

  if (sgvindex < 0 || sgvindex >= (int)arr.size()) sgvindex = 0;

  strlcpy(out.dev, doc[sgvindex]["device"] | "N/A", sizeof(out.dev));
  strlcpy(out.dir, doc[sgvindex]["direction"] | "N/A", sizeof(out.dir));
  if (strcmp(out.dir, "N/A") == 0) {
    strlcpy(out.dir, doc[sgvindex]["trend"] | "N/A", sizeof(out.dir));
  }

  out.rawtime_ms = doc[sgvindex]["date"].as<long long>();
  out.sensTime   = (time_t)(out.rawtime_ms / 1000LL);

  out.sgv_mgdl = doc[sgvindex]["sgv"] | 0.0f;
  out.sgv_mmol = mgdlToMmol(out.sgv_mgdl);

  for (int i = 0; i < 10; i++) {
    float v = doc[i]["sgv"] | 0.0f;
    out.last10mmol[i] = mgdlToMmol(v);
  }

  NS_LOGI("[NS] entries OK dev='%s' dir='%s' sgv=%.1f mmol (%.0f mg/dL)",
          out.dev, out.dir, out.sgv_mmol, out.sgv_mgdl);

  return true;
}

bool NightscoutClient::parseProps(const String& json, NightscoutData& out, ArduinoJson::DynamicJsonDocument& doc) const {
  DeserializationError e = deserializeJson(doc, json);
  if (e) {
    NS_LOGE("[NS] props JSON parse failed: %s", e.c_str());
    return false;
  }

  JsonObject iob = doc["iob"];
  out.iob = iob["iob"] | 0.0f;

  JsonObject cob = doc["cob"];
  out.cob = cob["cob"] | 0.0f;

  JsonObject delta = doc["delta"];
  out.delta_mgdl = delta["mgdl"] | 0;
  out.delta_mmol = mgdlToMmol((float)out.delta_mgdl);

  if (_cfg.showMgdl) {
    snprintf(out.delta_display, sizeof(out.delta_display), "%+d", out.delta_mgdl);
  } else {
    snprintf(out.delta_display, sizeof(out.delta_display), "%+.1f", out.delta_mmol);
  }

  JsonObject loop_obj;
  JsonObject loop_display;
  if (_cfg.infoLine == 3) {
    loop_obj = doc["openaps"];
    loop_display = loop_obj["status"];
  } else {
    loop_obj = doc["loop"];
    loop_display = loop_obj["display"];
  }

  strlcpy(out.loop_code,  loop_display["code"]  | "N/A", sizeof(out.loop_code));
  strlcpy(out.loop_label, loop_display["label"] | "N/A", sizeof(out.loop_label));

  JsonObject basal = doc["basal"];
  strlcpy(out.basal_display, basal["display"] | "N/A", sizeof(out.basal_display));

  NS_LOGI("[NS] props OK iob=%.2f cob=%.0f delta=%s loop=%s basal=%s",
          out.iob, out.cob, out.delta_display, out.loop_label, out.basal_display);

  return true;
}

bool NightscoutClient::update(NightscoutData& out) {
  out = NightscoutData{}; // как “обнуление” в вашем readNightscout()

  if (WiFi.status() != WL_CONNECTED) {
    NS_LOGE("[NS] WiFi not connected");
    return false;
  }

  String base = buildBase(_cfg.baseUrl);
  if (base.isEmpty()) {
    NS_LOGE("[NS] baseUrl empty");
    return false;
  }

  // общий doc как в проекте (16384 у вас часто хватает)
  ArduinoJson::DynamicJsonDocument doc(16384);

  // ----- entries -----
  {
    const String url = makeEntriesUrl(base);
    String json;
    int httpCode = 0;

    if (httpGet(url, json, httpCode)) {
      out.httpEntries = httpCode;
      sanitizeJson(json);
      out.okEntries = parseEntries(json, out, doc);
    } else {
      out.httpEntries = httpCode;
      out.okEntries = false;
    }
  }

  // ----- props -----
  {
    const String url = makePropsUrl(base);
    String json;
    int httpCode = 0;

    if (httpGet(url, json, httpCode)) {
      out.httpProps = httpCode;
      sanitizeJson(json);
      out.okProps = parseProps(json, out, doc);
    } else {
      out.httpProps = httpCode;
      out.okProps = false;
    }
  }

  return out.okEntries || out.okProps;
}
