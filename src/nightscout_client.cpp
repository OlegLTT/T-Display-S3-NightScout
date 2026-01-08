#include "nightscout_client.h"
#include "sgv_zone.h"
// -------------------- Public API --------------------

void NightscoutClient::begin(const NSConfig& cfg) {
  _cfg = cfg;
}

void NightscoutClient::requestUpdate() {
  _wantUpdate = true;
}


static inline void updateZoneCache(NightscoutData& ns) {
  if (ns.okEntries && !isnan(ns.sgv_mmol)) {
    ns.zoneColor = colorBySgvBounds(ns.sgv_mmol);
    ns.zoneValid = true;
  } else {
    ns.zoneColor = TFT_DARKGREY;
    ns.zoneValid = false;
  }
}


// -------------------- Helpers --------------------

String NightscoutClient::buildBase(String url) const {
  url.trim();
  if (url.isEmpty()) return url;

  if (!url.startsWith("http://") && !url.startsWith("https://")) {
    url = String(_cfg.preferHttpsIfNoScheme ? "https://" : "http://") + url;
  }
  if (url.endsWith("/")) url.remove(url.length() - 1);
  return url;
}

String NightscoutClient::makeEntriesPath(const String& base) const {
  return base + "/api/v1/entries.json?count=10";
}

String NightscoutClient::makePropsPath(const String& base) const {
  return base + "/api/v2/properties/iob,cob,delta,openaps,basal";
}

String NightscoutClient::makeDevStatusPath(const String& base) const {
  String u = base +
    "/api/v1/devicestatus.json?"
    "find%5Buploader.battery%5D%5B%24exists%5D=true"
    "&count=1";
  return u;
}

void NightscoutClient::sanitizeJson(String& s) const {
  for (int i = 0; i < (int)s.length(); i++) {
    if ((uint8_t)s[i] < 32) s.setCharAt(i, ' ');
  }
  s.replace("\\u0000", " ");
  s.replace("\\u000b", " ");
  s.replace("\\u0032", " ");

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

bool NightscoutClient::splitUrl(const String& fullUrl, bool& https, String& host,
                                uint16_t& port, String& path) const {
  https = false; host = ""; port = 0; path = "/";
  String u = fullUrl;

  if (u.startsWith("https://")) { https = true;  u.remove(0, 8); port = 443; }
  else if (u.startsWith("http://")) { https = false; u.remove(0, 7); port = 80; }
  else return false;

  int slash = u.indexOf('/');
  String hostport = (slash >= 0) ? u.substring(0, slash) : u;
  path = (slash >= 0) ? u.substring(slash) : "/";

  int colon = hostport.indexOf(':');
  if (colon >= 0) {
    host = hostport.substring(0, colon);
    port = (uint16_t)hostport.substring(colon + 1).toInt();
  } else {
    host = hostport;
  }

  host.trim();
  return !host.isEmpty();
}

// -------------------- FSM core --------------------

void NightscoutClient::startReq(ReqKind k, const String& base) {
  _r = Req{};
  _r.kind = k;
  _r.ph = Phase::Connect;
  _r.tStart = millis();
  _r.tLastIO = _r.tStart;

  String full;
  if (k == ReqKind::Entries)       full = makeEntriesPath(base);
  else if (k == ReqKind::Props)   full = makePropsPath(base);
  else if (k == ReqKind::DevStatus) full = makeDevStatusPath(base);
  else full = "";

  if (!splitUrl(full, _r.https, _r.host, _r.port, _r.path)) {
    _r.ph = Phase::Error;
    return;
  }

  if (_r.https && _cfg.tlsInsecure) _r.cs.setInsecure();

  NS_LOGI("[NS] startReq kind=%d path='%s'", (int)k, _r.path.c_str());
}

void NightscoutClient::abortReq() {
  if (_r.https) _r.cs.stop();
  else _r.cp.stop();

  _r.ph = Phase::Idle;
  _r.kind = ReqKind::None;
}

bool NightscoutClient::stepReq(NightscoutData& out) {
  auto& r = _r;
  const uint32_t now = millis();

  if (r.ph != Phase::Idle && (now - r.tLastIO) > _cfg.httpTimeoutMs) {
    NS_LOGE("[NS] timeout in phase=%d kind=%d", (int)r.ph, (int)r.kind);
    r.ph = Phase::Error;
  }

  WiFiClient* c = r.https ? (WiFiClient*)&r.cs : (WiFiClient*)&r.cp;

  switch (r.ph) {
    case Phase::Connect: {
      bool ok = c->connect(r.host.c_str(), r.port);
      r.tLastIO = now;
      if (!ok) { r.ph = Phase::Error; break; }
      r.ph = Phase::Send;
      return false;
    }

    case Phase::Send: {
      String req =
        String("GET ") + r.path + " HTTP/1.1\r\n" +
        "Host: " + r.host + "\r\n" +
        "Connection: close\r\n" +
        "User-Agent: t-display-s3\r\n" +
        "Accept: application/json\r\n\r\n";

      c->print(req);
      r.tLastIO = now;
      r.ph = Phase::ReadHeaders;
      return false;
    }

    case Phase::ReadHeaders: {
      while (c->available()) {
        char ch = (char)c->read();
        r.tLastIO = now;
        r.headerLine += ch;

        if (r.headerLine.endsWith("\r\n")) {
          String line = r.headerLine;
          r.headerLine = "";

          line.trim();
          if (line.isEmpty()) {
            r.ph = Phase::ReadBody;
            break;
          }

          if (line.startsWith("HTTP/")) {
            int sp = line.indexOf(' ');
            if (sp > 0) r.httpCode = line.substring(sp + 1).toInt();
          }

          String lower = line;
          lower.toLowerCase();

          if (lower.startsWith("content-length:")) {
            r.contentLen = lower.substring(String("content-length:").length()).toInt();
          } else if (lower.startsWith("transfer-encoding:") && lower.indexOf("chunked") >= 0) {
            r.chunked = true;
          }
        }

        if (r.headerLine.length() > 1024) { r.ph = Phase::Error; break; }
      }
      return false;
    }

    case Phase::ReadBody: {
      size_t readNow = 0;

      while (c->available() && readNow < READ_CHUNK) {
        char buf[READ_CHUNK];
        size_t can = min((size_t)c->available(), (size_t)(READ_CHUNK - readNow));
        size_t n = c->readBytes((uint8_t*)buf, can);
        if (!n) break;

        r.tLastIO = now;
        r.body.concat(buf, (unsigned int)n);

        readNow += n;
        if (r.body.length() > BODY_MAX) { r.ph = Phase::Error; break; }
      }

      if (!c->connected() && !c->available()) {
        r.ph = Phase::Done;
      }
      return false;
    }

    case Phase::Done: {
      const int code = r.httpCode;
      const bool ok = (code == 200);

      ArduinoJson::DynamicJsonDocument doc(16384);

      if (ok) {
        sanitizeJson(r.body);
        DeserializationError e = deserializeJson(doc, r.body);
        if (e) {
          NS_LOGE("[NS] JSON parse failed kind=%d: %s", (int)r.kind, e.c_str());
          doc.clear();
        }
      }

      if (r.kind == ReqKind::Entries) {
        _stage.httpEntries = code;
        _stage.okEntries = (ok && !doc.isNull() && doc.size() > 0) ? parseEntries(r.body, _stage, doc) : false;
        
          updateZoneCache(_stage);

        _stageHasEntries = _stage.okEntries;
        _entriesDone = true;
      }
      else if (r.kind == ReqKind::Props) {
        _stage.httpProps = code;
        _stage.okProps = (ok && !doc.isNull() && doc.size() > 0) ? parseProps(r.body, _stage, doc) : false;
        _stageHasProps = _stage.okProps;
        _propsDone = true;
      }
      else if (r.kind == ReqKind::DevStatus) {
        _stage.httpDevStatus = code;
        _stage.okDevStatus = ok;
        if (ok && !doc.isNull() && doc.size() > 0) {
          parseDevStatus(r.body, _stage, doc);
        } else {
          _stage.pumpBatValid = false;
        }
        _devDone = true;

        NS_LOGI("[NS] devstatus done http=%d len=%u valid=%d pct=%d",
                code, (unsigned)r.body.length(), _stage.pumpBatValid, _stage.pumpBatPct);
      }

      abortReq();
      return true;
    }

    case Phase::Error: {
      const int code = (r.httpCode ? r.httpCode : -1);

      if (r.kind == ReqKind::Entries) {
        _stage.httpEntries = code;
        _stage.okEntries = false;
        _stageHasEntries = false;
        _entriesDone = true;
      } else if (r.kind == ReqKind::Props) {
        _stage.httpProps = code;
        _stage.okProps = false;
        _stageHasProps = false;
        _propsDone = true;
      } else if (r.kind == ReqKind::DevStatus) {
        _stage.httpDevStatus = code;
        _stage.okDevStatus = false;
        _stage.pumpBatValid = false;
        _devDone = true;

        NS_LOGE("[NS] devstatus ERROR http=%d bodyLen=%u",
                code, (unsigned)r.body.length());
      }

      abortReq();
      return true;
    }

    case Phase::Idle:
    default:
      return false;
  }
}

bool NightscoutClient::tick(NightscoutData& out) {
  if (WiFi.status() != WL_CONNECTED) return false;

  // Старт цикла
  if (_r.ph == Phase::Idle) {
    if (_wantUpdate) {
      _wantUpdate = false;

      _entriesDone = false;
      _propsDone   = false;
      _devDone     = false;

      resetStageFrom(out);

      _stage.httpEntries = 0; _stage.okEntries = false;
      _stage.httpProps   = 0; _stage.okProps   = false;
      _stage.httpDevStatus = 0; _stage.okDevStatus = false;

      String base = buildBase(_cfg.baseUrl);
      if (base.isEmpty()) {
        NS_LOGE("[NS] baseUrl empty");
        return true;
      }

      startReq(ReqKind::Entries, base);
      return false;
    }
    return false;
  }

  // шаг FSM
  bool finished = stepReq(out);

  // цепочка запросов: entries -> props -> devstatus
  if (finished && _r.ph == Phase::Idle) {
    String base = buildBase(_cfg.baseUrl);

    if (_entriesDone && !_propsDone) {
      if (!base.isEmpty()) startReq(ReqKind::Props, base);
      return false;
    }
    if (_propsDone && !_devDone) {
      if (!base.isEmpty()) startReq(ReqKind::DevStatus, base);
      return false;
    }
  }

  // 1) синхронный коммит entries+props
  if (_propsDone) {
    if (commitIfReady(out)) {
      return true;
    }
  }

  // 2) devstatus коммитим НЕЗАВИСИМО (важно!)
  if (_devDone) {
    bool changed = false;

    if (out.okDevStatus != _stage.okDevStatus) changed = true;
    if (out.httpDevStatus != _stage.httpDevStatus) changed = true;
    if (out.pumpBatValid != _stage.pumpBatValid) changed = true;
    if (out.pumpBatPct != _stage.pumpBatPct) changed = true;
    if (fabs(out.pumpBatV - _stage.pumpBatV) > 0.001f) changed = true;
    if (out.pumpTime != _stage.pumpTime) changed = true;

    out.okDevStatus   = _stage.okDevStatus;
    out.httpDevStatus = _stage.httpDevStatus;
    out.pumpBatValid  = _stage.pumpBatValid;
    out.pumpBatPct    = _stage.pumpBatPct;
    out.pumpBatV      = _stage.pumpBatV;
    out.pumpTime      = _stage.pumpTime;

    if (changed) return true;
  }

  return false;
}

// -------------------- JSON parse --------------------

bool NightscoutClient::parseEntries(const String& json, NightscoutData& out,
                                    ArduinoJson::DynamicJsonDocument& doc) const {
  JsonArray arr = doc.as<JsonArray>();
  if (arr.isNull() || arr.size() == 0) {
    NS_LOGE("[NS] entries empty");
    return false;
  }

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

  int n = min(10, (int)arr.size());
  for (int i = 0; i < n; i++) {
    float v = doc[i]["sgv"] | 0.0f;
    out.last10mmol[i] = mgdlToMmol(v);
  }
  for (int i = n; i < 10; i++) out.last10mmol[i] = 0;

  NS_LOGI("[NS] entries OK dev='%s' dir='%s' sgv=%.1f mmol (%.0f mg/dL)",
          out.dev, out.dir, out.sgv_mmol, out.sgv_mgdl);

  return true;
}

bool NightscoutClient::parseProps(const String& json, NightscoutData& out,
                                  ArduinoJson::DynamicJsonDocument& doc) const {
  JsonObject iob = doc["iob"];
  out.iob = iob["iob"] | 0.0f;

  JsonObject cob = doc["cob"];
  out.cob = cob["cob"] | 0.0f;

  JsonObject delta = doc["delta"];
  out.delta_mgdl = delta["mgdl"] | 0;
  out.delta_mmol = mgdlToMmol((float)out.delta_mgdl);

  snprintf(out.delta_display, sizeof(out.delta_display), "%+.1f", out.delta_mmol);

  JsonObject loop_obj;
  JsonObject loop_display;
  loop_obj = doc["openaps"];
  loop_display = loop_obj["status"];

  strlcpy(out.loop_code,  loop_display["code"]  | "N/A", sizeof(out.loop_code));
  strlcpy(out.loop_label, loop_display["label"] | "N/A", sizeof(out.loop_label));

  JsonObject basal = doc["basal"];
  strlcpy(out.basal_display, basal["display"] | "N/A", sizeof(out.basal_display));

  NS_LOGI("[NS] props OK iob=%.2f cob=%.0f delta=%s loop=%s basal=%s",
          out.iob, out.cob, out.delta_display, out.loop_label, out.basal_display);

  return true;
}

// --- devicestatus parse helpers ---
static bool readPumpBattery(JsonObject o, int& pct, float& volt) {
  pct = -1;
  volt = 0.0f;

  // 1) основной кейс: uploader.battery = %
  if (o.containsKey("uploader")) {
    JsonObject up = o["uploader"].as<JsonObject>();
    if (!up.isNull() && up.containsKey("battery")) {
      int b = up["battery"] | -1;
      if (b >= 0 && b <= 100) {
        pct = b;
        return true;
      }
    }
  }

  // 2) fallback: pump.battery.percent / voltage
  if (o.containsKey("pump")) {
    JsonObject pump = o["pump"].as<JsonObject>();
    if (!pump.isNull() && pump.containsKey("battery")) {
      JsonVariant bat = pump["battery"];

      if (bat.is<JsonObject>()) {
        JsonObject b = bat.as<JsonObject>();
        if (b.containsKey("percent")) pct = b["percent"] | -1;
        if (b.containsKey("voltage")) volt = b["voltage"] | 0.0f;
      } else if (bat.is<int>()) {
        pct = bat.as<int>();
      } else if (bat.is<float>()) {
        float v = bat.as<float>();
        if (v <= 10.0f) volt = v;
        else pct = (int)v;
      }

      if ((pct >= 0 && pct <= 100) || (volt > 0.0f)) return true;
    }
  }

  return false;
}

bool NightscoutClient::parseDevStatus(const String& json, NightscoutData& out,
                                      ArduinoJson::DynamicJsonDocument& doc) const {
  JsonArray arr = doc.as<JsonArray>();
  if (arr.isNull() || arr.size() == 0) {
    NS_LOGW("[NS] devicestatus empty");
    out.pumpBatValid = false;
    return true;
  }

  // Вариант A: count=1, поэтому берём первый объект
  JsonObject o = arr[0].as<JsonObject>();
  int pct; float volt;
  bool ok = readPumpBattery(o, pct, volt);

  if (ok) {
    out.pumpBatValid = true;
    out.pumpBatPct = pct;
    out.pumpBatV = volt;

    long long mills = o["mills"] | 0LL;
    out.pumpTime = (mills > 0) ? (time_t)(mills / 1000LL) : 0;

    if (pct >= 0) NS_LOGI("[NS] pumpBat=%d%% (V=%.2f)", pct, volt);
    else          NS_LOGI("[NS] pumpBat V=%.2f", volt);
  } else {
    out.pumpBatValid = false;
    out.pumpBatPct = -1;
    out.pumpBatV = 0.0f;
    out.pumpTime = 0;
    NS_LOGW("[NS] devicestatus: no uploader.battery/pump.battery in first record");
  }

  return true;
}
