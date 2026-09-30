// Получение данных Nightscout по HTTP/HTTPS с повторными попытками и разбором JSON.
#include "nightscout_client.h"
#include "logs.h"
#include <math.h>
#include <time.h>
#include <stdlib.h>
#include <ctype.h>

using su::SH;

void NightscoutClient::begin(const NSConfig& cfg) {
  _cfg = cfg;
  _hostBackoffMs = 0;
  _retryCount = 0;
  _retryAtMs = 0;
  _wantUpdate = false;
  _lastNoWifiLogMs = 0;
  _lastWeakLogMs = 0;
  _r = Req{};
  _r.ph = Phase::Idle;
}

void NightscoutClient::requestUpdate() {
  _wantUpdate = true;
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

static String urlEncode(const String& s) {
  static const char hex[] = "0123456789ABCDEF";
  String out;
  out.reserve(s.length() * 3);
  for (size_t i = 0; i < s.length(); i++) {
    const uint8_t c = (uint8_t)s[i];
    const bool unreserved =
      (c >= 'a' && c <= 'z') ||
      (c >= 'A' && c <= 'Z') ||
      (c >= '0' && c <= '9') ||
      c == '-' || c == '_' || c == '.' || c == '~';
    if (unreserved) out += (char)c;
    else {
      out += '%';
      out += hex[(c >> 4) & 0xF];
      out += hex[c & 0xF];
    }
  }
  return out;
}

String NightscoutClient::makePropsPath(const String& base) const {
  String url = base + "/api/v2/properties/bgnow,direction,delta,iob,cob,pump";
  if (!_cfg.token.isEmpty()) {
    url += "?token=" + urlEncode(_cfg.token);
  }
  return url;
}

void NightscoutClient::sanitizeJson(String& s) const {
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

  if (u.startsWith("https://")) { https = true; u.remove(0, 8); }
  else if (u.startsWith("http://")) { https = false; u.remove(0, 7); }
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
    port = https ? 443 : 80;
  }
  host.trim();
  return !host.isEmpty();
}

void NightscoutClient::applyBounds(NightscoutData& out) const {
  memcpy(out.sgvBounds, _cfg.sgvBounds, sizeof(out.sgvBounds));
}

void NightscoutClient::warnIfWeakRssi() {
  if (_cfg.warnRssiDbm == 0) return;
  int rssi = WiFi.RSSI();
  if (rssi == 0) return;

  if (rssi < _cfg.warnRssiDbm) {
    uint32_t now = millis();
    if (now - _lastWeakLogMs >= _cfg.weakRssiLogPeriodMs) {
      _lastWeakLogMs = now;
      logs::logWf("[WIFI] weak signal RSSI=%d dBm -> NS request anyway", rssi);
    }
  }
}

// Повторная попытка планируется по таймеру и не использует delay.
void NightscoutClient::scheduleRetry() {

  if (_cfg.retryMax != 0 && _retryCount >= _cfg.retryMax) {
    logs::logEf("[NS] request failed -> retries exhausted (%u)", (unsigned)_cfg.retryMax);
    _wantUpdate = false;
    _r.ph = Phase::Idle;
    return;
  }

  _retryCount++;
  _retryAtMs = millis() + _cfg.retryDelayMs;

  _wantUpdate = true;

  logs::logWf("[NS] request failed -> retry #%u in %ums",
              (unsigned)_retryCount,
              (unsigned)_cfg.retryDelayMs);

  _r.ph = Phase::RetryWait;
}

void NightscoutClient::startReq(const String& base) {
  _r = Req{};
  _r.ph = Phase::Connect;
  _r.tStart  = millis();
  _r.tLastIO = _r.tStart;

  String full = makePropsPath(base);
  if (!splitUrl(full, _r.https, _r.host, _r.port, _r.path)) {
    _r.ph = Phase::Error;
    return;
  }

  if (_r.https && _cfg.tlsInsecure) _r.cs.setInsecure();

  logs::logIf("[NS] startReq GET %s", _r.path.c_str());
}

void NightscoutClient::abortReq() {
  if (_r.https) _r.cs.stop();
  else _r.cp.stop();
  _r.ph = Phase::Idle;
}

// Запрос обрабатывается по этапам; вызов connect внутри этапа подключения остаётся синхронным.
bool NightscoutClient::stepReq(NightscoutData& out) {
  auto& r = _r;
  const uint32_t now = millis();

  if ((r.ph == Phase::Connect || r.ph == Phase::Send || r.ph == Phase::ReadHeaders || r.ph == Phase::ReadBody) &&
      (now - r.tLastIO) > _cfg.httpTimeoutMs) {
    logs::logEf("[NS] timeout phase=%d", (int)r.ph);
    r.ph = Phase::Error;
  }

  WiFiClient* c = r.https ? (WiFiClient*)&r.cs : (WiFiClient*)&r.cp;

  switch (r.ph) {
    case Phase::Connect: {
      warnIfWeakRssi();
      logs::logIf("[NS] Connect %s:%u https=%d", r.host.c_str(), (unsigned)r.port, (int)r.https);

      bool ok = c->connect(r.host.c_str(), r.port);
      r.tLastIO = now;

      if (!ok) {
        logs::logE("[NS] connect failed");
        r.httpCode = -1;

        const uint32_t backoff = (_cfg.hostErrorBackoffMs > 0) ? _cfg.hostErrorBackoffMs : 60000;
        _hostErrorAtMs = millis();
        _hostBackoffMs = backoff;

        r.ph = Phase::Error;
        return false;
      }

      _hostBackoffMs = 0;
      r.ph = Phase::Send;
      return false;
    }

    case Phase::Send: {
      logs::logI("[NS] Send request (HTTP/1.0 no-chunked)");

      String req =
        String("GET ") + r.path + " HTTP/1.0\r\n" +
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
            logs::logIf("[NS] Headers done (http=%d len=%d)", r.httpCode, r.contentLen);
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
          }
        }

        if (r.headerLine.length() > 1024) { r.ph = Phase::Error; break; }
      }
      return false;
    }

    case Phase::ReadBody: {
      size_t readNow = 0;
      char buf[READ_CHUNK];

      while (c->available() && readNow < READ_CHUNK) {
        size_t can = min((size_t)c->available(), (size_t)(READ_CHUNK - readNow));
        size_t n = c->readBytes((uint8_t*)buf, can);
        if (!n) break;

        r.tLastIO = now;
        r.body.concat(buf, (unsigned int)n);
        readNow += n;

        if (r.body.length() > BODY_MAX) { r.ph = Phase::Error; break; }
      }

      // Не заменяем ошибку превышения размера тела успешным завершением при закрытии соединения.
      if (r.ph == Phase::Error) return false;
      if (!c->connected() && !c->available()) {
        logs::logIf("[NS] Body done (len=%u)", (unsigned)r.body.length());
        r.ph = Phase::Done;
      }
      return false;
    }

    case Phase::Done: {
      applyBounds(out);
      out.http = r.httpCode;
      out.ok   = false;

      if (r.httpCode != 200) {
        logs::logEf("[NS] HTTP %d (len=%u)", r.httpCode, (unsigned)r.body.length());
        abortReq();
        return true;
      }

      sanitizeJson(r.body);
      logs::logI("[NS] Parse v2/properties via GSON...");

      if (!parseV2Properties(r.body, out)) {
        out.ok = false;
      } else {
        out.ok = true;
        _retryCount = 0;
      }

      abortReq();
      return true;
    }

    case Phase::Error: {
      applyBounds(out);
      out.http = (r.httpCode ? r.httpCode : -1);
      out.ok   = false;
      abortReq();
      return true;
    }

    default:
      return false;
  }
}

bool NightscoutClient::tick(NightscoutData& out) {
  const uint32_t now = millis();

  if (WiFi.status() != WL_CONNECTED) {

    if (_r.ph != Phase::WaitWiFi && _r.ph != Phase::RetryWait) {
      if (isBusy()) {
        abortReq();
        _wantUpdate = true;
      }
      _r.ph = Phase::WaitWiFi;
    }
    if (_wantUpdate) {
      if (now - _lastNoWifiLogMs >= _cfg.noWifiLogPeriodMs) {
        _lastNoWifiLogMs = now;
        logs::logW("[NS] no WiFi -> wait");
      }
    }
    return false;
  }
  if (_r.ph == Phase::WaitWiFi) _r.ph = Phase::Idle;

  // Пауза после ошибки подключения измеряется разностью millis, включая переполнение.
  if (_hostBackoffMs != 0) {
    if (uint32_t(now - _hostErrorAtMs) < _hostBackoffMs) return false;
    _hostBackoffMs = 0;
  }

  if (_r.ph == Phase::RetryWait) {
    if ((int32_t)(now - _retryAtMs) < 0) return false;
    _r.ph = Phase::Idle;
  }

  if (_r.ph == Phase::Idle) {
    if (_wantUpdate) {
      _wantUpdate = false;
      String base = buildBase(_cfg.baseUrl);

      if (base.isEmpty()) {
        logs::logE("[NS] baseUrl empty");
        applyBounds(out);
        out.ok = false;
        out.http = 0;
        scheduleRetry();
        return true;
      }

      warnIfWeakRssi();
      startReq(base);

      if (_r.ph == Phase::Error) {
        applyBounds(out);
        out.ok = false;
        out.http = -1;
        abortReq();
        scheduleRetry();
        return true;
      }

      return false;
    }
    return false;
  }

  bool changed = stepReq(out);
  if (!changed) return false;

  if (!out.ok) {
    scheduleRetry();
  } else {
    _r.ph = Phase::Idle;
  }
  return true;
}

static bool entryToCstr(gson::Entry e, char* out, size_t outSz) {
  if (!out || outSz == 0) return false;
  out[0] = 0;
  if (!e) return false;
  e.toStr(out, outSz);
  return out[0] != 0;
}

static float entryToFloatAny(gson::Entry e) {
  if (!e) return NAN;
  char buf[24];
  if (!entryToCstr(e, buf, sizeof(buf))) return NAN;
  return (float)atof(buf);
}

static int entryToBatteryPercent(gson::Entry e) {

  if (!e || (!e.is(gson::Type::Int) && !e.is(gson::Type::Float)
             && !e.is(gson::Type::String))) return -1;

  char buf[24];
  if (e.value().length() >= sizeof(buf) || !entryToCstr(e, buf, sizeof(buf))) return -1;
  char* end = nullptr;
  const float percent = strtof(buf, &end);
  if (end == buf || !isfinite(percent) || percent < 0.0f || percent > 100.0f) return -1;
  while (*end && isspace(static_cast<unsigned char>(*end))) ++end;
  if (*end) return -1;
  return static_cast<int>(percent);
}

// Разбор свойств Nightscout; заряд загрузчика и напряжение помпы — разные показатели.
bool NightscoutClient::parseV2Properties(const String& json, NightscoutData& out) const {
  gson::Parser p;
  p.reserve(250);

  if (!p.parse(json) || p.hasError()) {
    logs::logEf("[NS] GSON parse error: %s at %d", p.readError(), p.errorIndex());
    return false;
  }

  p.hashKeys();

  gson::Entry bgnow = p.get(SH("bgnow"));
  if (!bgnow) {
    logs::logE("[NS] no 'bgnow'");
    return false;
  }

  gson::Entry sgvs = bgnow.get(SH("sgvs"));
  if (!sgvs || sgvs.length() <= 0) {
    logs::logE("[NS] no 'bgnow.sgvs'");
    return false;
  }

  gson::Entry sg0 = sgvs.get((int)0);
  out.sgv_mmol = entryToFloatAny(sg0.get(SH("scaled")));
  out.update_time_ms = (uint64_t)bgnow.get(SH("mills")).toInt64();

  strlcpy(out.trend, "N/A", sizeof(out.trend));
  gson::Entry direction = p.get(SH("direction"));
  if (direction) {
    char buf[24];
    if (entryToCstr(direction.get(SH("value")), buf, sizeof(buf))) {
      strlcpy(out.trend, buf, sizeof(out.trend));
    }
  }

  gson::Entry delta = p.get(SH("delta"));
  out.delta_mmol = delta ? entryToFloatAny(delta.get(SH("scaled"))) : NAN;

  gson::Entry iob = p.get(SH("iob"));
  out.iob = iob ? entryToFloatAny(iob.get(SH("iob"))) : NAN;

  gson::Entry cob = p.get(SH("cob"));
  out.cob = cob ? entryToFloatAny(cob.get(SH("cob"))) : NAN;

  out.uploaderBatPct = -1;
  out.pumpBatV   = 0.0f;
  gson::Entry batteryPercent;
  char uploaderName[32] = "";

  gson::Entry pump = p.get(SH("pump"));
  if (pump) {

    gson::Entry uploader = pump.get(SH("uploader"));
    batteryPercent = uploader.get(SH("battery"));
    out.uploaderBatPct = entryToBatteryPercent(batteryPercent);
    entryToCstr(uploader.get(SH("name")), uploaderName, sizeof(uploaderName));

    gson::Entry pump_inner = pump.get(SH("pump"));
    if (pump_inner) {
      gson::Entry bat = pump_inner.get(SH("battery"));
      if (bat) {
        float v = entryToFloatAny(bat.get(SH("voltage")));
        if (!isnan(v) && v > 0.0f) out.pumpBatV = v;
      }
    }
  }

  char rawPercent[24] = "<missing>";
  if (batteryPercent && !entryToCstr(batteryPercent, rawPercent, sizeof(rawPercent))) {
    strlcpy(rawPercent, "<empty/long>", sizeof(rawPercent));
  }
  logs::logIf("[NS] battery: source=pump.uploader.battery name=%s raw='%s' pct=%d%%",
              uploaderName[0] ? uploaderName : "<missing>", rawPercent, out.uploaderBatPct);

  char tbuf[32] = {0};
  bool haveTime = false;

  if (out.update_time_ms > 0) {
    time_t ts = (time_t)(out.update_time_ms / 1000ULL);
    if (time(nullptr) > 100000 && ts > 0) {
      struct tm tmv;
      localtime_r(&ts, &tmv);
      if (strftime(tbuf, sizeof(tbuf), "%H:%M:%S", &tmv) > 0) haveTime = true;
    }
  }
  if (!haveTime) {
    snprintf(tbuf, sizeof(tbuf), "%llu", (unsigned long long)out.update_time_ms);
  }

  logs::logIf("[NS] parsed: sgv=%.1f mmol trend=%s delta=%.2f iob=%.2f cob=%.0f upbat=%d%% pumpV=%.2f t=%s elems=%u bytes=%u",
              out.sgv_mmol, out.trend, out.delta_mmol, out.iob, out.cob,
              out.uploaderBatPct, out.pumpBatV,
              tbuf,
              (unsigned)p.length(), (unsigned)p.size());

  if (isnan(out.sgv_mmol) || out.update_time_ms == 0) {
    logs::logW("[NS] parsed but missing sgv/time");
    return false;
  }

  return true;
}
