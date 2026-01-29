// ======================= nightscout_client.cpp =======================
#include "nightscout_client.h"

using su::SH;

// -------------------- Public API --------------------

void NightscoutClient::begin(const NSConfig& cfg) {
  _cfg = cfg;
  _hostError = false;
}

void NightscoutClient::requestUpdate() {
  _wantUpdate = true;
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

String NightscoutClient::makePropsPath(const String& base) const {
  return base + "/api/v2/properties/bgnow,direction,delta,iob,cob,pump";
}

void NightscoutClient::sanitizeJson(String& s) const {
  // чистим управляющие символы
  for (int i = 0; i < (int)s.length(); i++) {
    if ((uint8_t)s[i] < 32) s.setCharAt(i, ' ');
  }
  s.replace("\\u0000", " ");
  s.replace("\\u000b", " ");
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

void NightscoutClient::startReq(const String& base) {
  _r = Req{};
  _r.ph = Phase::Connect;
  _r.tStart = millis();
  _r.tLastIO = _r.tStart;

  String full = makePropsPath(base);
  if (!splitUrl(full, _r.https, _r.host, _r.port, _r.path)) {
    _r.ph = Phase::Error;
    return;
  }

  if (_r.https && _cfg.tlsInsecure) _r.cs.setInsecure();

  NS_LOGI("[NS] startReq GET %s", _r.path.c_str());
}

void NightscoutClient::abortReq() {
  if (_r.https) _r.cs.stop();
  else _r.cp.stop();

  _r.ph = Phase::Idle;
}

bool NightscoutClient::stepReq(NightscoutData& out) {
  auto& r = _r;
  const uint32_t now = millis();

  if (r.ph != Phase::Idle && (now - r.tLastIO) > _cfg.httpTimeoutMs) {
    NS_LOGE("[NS] timeout phase=%d", (int)r.ph);
    r.ph = Phase::Error;
  }

  WiFiClient* c = r.https ? (WiFiClient*)&r.cs : (WiFiClient*)&r.cp;

  switch (r.ph) {
    case Phase::Connect: {
      NS_LOGI("[NS] Connect %s:%u https=%d", r.host.c_str(), (unsigned)r.port, (int)r.https);
      bool ok = c->connect(r.host.c_str(), r.port);
      r.tLastIO = now;
      if (!ok) {
        NS_LOGE("[NS] connect failed");
        r.httpCode = -1;
        _hostError = true;
        r.ph = Phase::Error;
        return false;
      }
      r.ph = Phase::Send;
      return false;
    }

    case Phase::Send: {
      NS_LOGI("[NS] Send request");
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
            NS_LOGI("[NS] Headers done (http=%d len=%d chunked=%d)",
                    r.httpCode, r.contentLen, (int)r.chunked);
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
        NS_LOGI("[NS] Body done (len=%u)", (unsigned)r.body.length());
        r.ph = Phase::Done;
      }
      return false;
    }

    case Phase::Done: {
      // всегда прокидываем bounds на экраны
      memcpy(out.sgvBounds, _cfg.sgvBounds, sizeof(out.sgvBounds));

      out.http = r.httpCode;
      out.ok = false;

      if (r.httpCode != 200) {
        NS_LOGE("[NS] HTTP %d (len=%u)", r.httpCode, (unsigned)r.body.length());
        abortReq();
        return true;
      }

      sanitizeJson(r.body);

      NS_LOGI("[NS] Parse v2/properties via GSON...");
      if (!parseV2Properties(r.body, out)) {
        out.ok = false;
      } else {
        out.ok = true;
      }

      abortReq();
      return true;
    }

    case Phase::Error: {
      memcpy(out.sgvBounds, _cfg.sgvBounds, sizeof(out.sgvBounds));
      out.http = (r.httpCode ? r.httpCode : -1);
      out.ok = false;
      abortReq();
      return true;
    }

    case Phase::Idle:
    default:
      return false;
  }
   return false;
}

bool NightscoutClient::tick(NightscoutData& out) {
  if (WiFi.status() != WL_CONNECTED) return false;

  // если хост уже “фатально” не коннектится — не спамим сеть
  if (_hostError && _r.ph == Phase::Idle) {
    _wantUpdate = false;
    return false;
  }

  // старт запроса
  if (_r.ph == Phase::Idle) {
    if (_wantUpdate && !_hostError) {
      _wantUpdate = false;

      String base = buildBase(_cfg.baseUrl);
      if (base.isEmpty()) {
        NS_LOGE("[NS] baseUrl empty");
        out.ok = false;
        out.http = 0;
        memcpy(out.sgvBounds, _cfg.sgvBounds, sizeof(out.sgvBounds));
        return true;
      }

      startReq(base);
      return false;
    }
    return false;
  }

  // шаг FSM
  return stepReq(out);
}

// -------------------- Parse (GyverGSON + hashKeys + su::SH) --------------------

static bool entryToCstr(gson::Entry e, char* out, size_t outSz) {
  if (!out || outSz == 0) return false;
  out[0] = 0;
  if (!e) return false;
  e.toStr(out, outSz);
  return out[0] != 0;
}

static float entryToFloatAny(gson::Entry e) {
  if (!e) return NAN;

  // если это число — можно напрямую
  // (у Entry есть type(), но даже без него можно безопасно: number тоже toStr отдаст)
  char buf[24];
  if (!entryToCstr(e, buf, sizeof(buf))) return NAN;
  return (float)atof(buf);
}

static int entryToIntAny(gson::Entry e, int def = 0) {
  if (!e) return def;
  return e.toInt32();
}

bool NightscoutClient::parseV2Properties(const String& json, NightscoutData& out) const {
  gson::Parser p;

  // теперь пакет маленький, но reserve всё равно ускоряет
  p.reserve(250);

  if (!p.parse(json) || p.hasError()) {
    NS_LOGE("[NS] GSON parse error: %s at %d", p.readError(), p.errorIndex());
    return false;
  }

  // хешируем ключи для ускорения доступа
  p.hashKeys();

  // ---- bgnow ----
  gson::Entry bgnow = p.get(SH("bgnow"));
  if (!bgnow) {
    NS_LOGE("[NS] no 'bgnow'");
    return false;
  }

  gson::Entry sgvs = bgnow.get(SH("sgvs"));
  if (!sgvs || sgvs.length() <= 0) {
    NS_LOGE("[NS] no 'bgnow.sgvs'");
    return false;
  }

  gson::Entry sg0 = sgvs.get((int)0);
  out.sgv_mmol = entryToFloatAny(sg0.get(SH("scaled")));
  out.update_time_ms = (uint64_t)bgnow.get(SH("mills")).toInt64();

  // ---- direction.value ----
  strlcpy(out.trend, "N/A", sizeof(out.trend));
  gson::Entry direction = p.get(SH("direction"));
  if (direction) {
    char buf[24];
    if (entryToCstr(direction.get(SH("value")), buf, sizeof(buf))) {
      strlcpy(out.trend, buf, sizeof(out.trend));
    }
  }

  // ---- delta.scaled ----
  gson::Entry delta = p.get(SH("delta"));
  out.delta_mmol = delta ? entryToFloatAny(delta.get(SH("scaled"))) : NAN;

  // ---- iob.iob / cob.cob ----
  gson::Entry iob = p.get(SH("iob"));
  out.iob = iob ? entryToFloatAny(iob.get(SH("iob"))) : NAN;

  gson::Entry cob = p.get(SH("cob"));
  out.cob = cob ? entryToFloatAny(cob.get(SH("cob"))) : NAN;

  // ---- pump battery ----
  out.pumpBatPct = -1;
  out.pumpBatV   = 0.0f;

  gson::Entry pump = p.get(SH("pump"));
  if (pump) {
    // voltage
    gson::Entry pump_inner = pump.get(SH("pump"));
    if (pump_inner) {
      gson::Entry bat = pump_inner.get(SH("battery"));
      if (bat) {
        float v = entryToFloatAny(bat.get(SH("voltage")));
        if (!isnan(v) && v > 0.0f) out.pumpBatV = v;

        int pct = entryToIntAny(bat.get(SH("percent")), -1);
        if (pct > 0 && pct <= 100) out.pumpBatPct = pct;  // у тебя часто 0
      }
    }

    // uploader battery (%)
    if (out.pumpBatPct < 0) {
      gson::Entry uploader = pump.get(SH("uploader"));
      if (uploader) {
        int up = entryToIntAny(uploader.get(SH("battery")), -1);
        if (up >= 0 && up <= 100) out.pumpBatPct = up;
      }
    }
  }

  NS_LOGI("[NS] parsed: sgv=%.1f mmol trend=%s delta=%.2f iob=%.2f cob=%.0f bat=%d%% V=%.2f t=%llu elems=%u bytes=%u",
          out.sgv_mmol, out.trend, out.delta_mmol, out.iob, out.cob,
          out.pumpBatPct, out.pumpBatV,
          (unsigned long long)out.update_time_ms,
          (unsigned)p.length(), (unsigned)p.size());

  // минимальная валидность
  if (isnan(out.sgv_mmol) || out.update_time_ms == 0) {
    NS_LOGW("[NS] parsed but missing sgv/time");
    return false;
  }
  return true;
}
