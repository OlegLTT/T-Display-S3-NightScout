#include "nightscout_session.h"
#include "nightscout_http.h"
#include <HTTPClient.h>
#include <cmath>
#include <ctime>
#include <cstring>

using su::SH;
namespace {
String encode(const String& input) {
    const char hex[] = "0123456789ABCDEF";
    String out;
    for (unsigned i=0; i<input.length(); ++i) {
        const uint8_t c = input[i];
        if (isalnum(c) || c=='-' || c=='_' || c=='.' || c=='~') out += char(c);
        else { out += '%'; out += hex[c>>4]; out += hex[c&15]; }
    }
    return out;
}
class LimitedBody : public Stream {
public:
    String value;
    bool overflow = false;
    size_t write(uint8_t c) override { return write(&c,1); }
    size_t write(const uint8_t* bytes, size_t size) override {
        if (value.length()+size > app_constants::http_body_max_bytes) { overflow=true; return 0; }
        return value.concat(reinterpret_cast<const char*>(bytes), size) ? size : 0;
    }
    int available() override { return 0; }
    int read() override { return -1; }
    int peek() override { return -1; }
    void flush() override {}
};
bool number(gson::Entry entry, double& result) {
    if (!entry || (!entry.is(gson::Type::Int) && !entry.is(gson::Type::Float))) return false;
    char text[32]; entry.toStr(text,sizeof(text)); char* end = nullptr;
    result = strtod(text,&end);
    return end != text && !*end && std::isfinite(result);
}
}

void NightscoutSession::begin(const NSConfig& cfg) {
    if (!_mutex) _mutex = xSemaphoreCreateMutex();
    if (!_results) _results = xQueueCreate(1,sizeof(NightscoutData));
    if (!_mutex || !_results) { logs::logE("[NS] Session allocation failed"); return; }
    xSemaphoreTake(_mutex,portMAX_DELAY);
    _nextConfig = cfg; _configure = true; _update = true;
    _updateFromLan = false;
    _state=ns_alarm::Snapshot{};
    strlcpy(_state.message,"Connecting to Nightscout...",sizeof(_state.message));
    xSemaphoreGive(_mutex);
    if (!_task && xTaskCreate(run,"nightscout",12288,this,1,&_task) != pdPASS)
        logs::logE("[NS] Failed to start network task");
}
void NightscoutSession::requestUpdate(bool from_lan) {
    if (!_mutex) return;
    xSemaphoreTake(_mutex,portMAX_DELAY);
    _update=true; _updateFromLan=_updateFromLan||from_lan;
    xSemaphoreGive(_mutex);
}
bool NightscoutSession::tick(NightscoutData& out) {
    if (!_mutex || !_results) return false;
    xSemaphoreTake(_mutex,portMAX_DELAY);
    // Запоздалый ответ прежнего сервера не должен стать данными новой конфигурации.
    const bool received=!_configure && xQueueReceive(_results,&out,0)==pdTRUE;
    xSemaphoreGive(_mutex);
    return received;
}
bool NightscoutSession::isBusy() const {
    if (!_mutex) return false;
    xSemaphoreTake(_mutex,portMAX_DELAY); bool value=_busy; xSemaphoreGive(_mutex); return value;
}
bool NightscoutSession::isUpdateWanted() const {
    if (!_mutex) return false;
    xSemaphoreTake(_mutex,portMAX_DELAY); bool value=_wanted||_update; xSemaphoreGive(_mutex); return value;
}
uint32_t NightscoutSession::lastRequestAtMs() const {
    if (!_mutex) return 0;
    xSemaphoreTake(_mutex,portMAX_DELAY); uint32_t value=_lastRequest; xSemaphoreGive(_mutex); return value;
}
ns_alarm::Snapshot NightscoutSession::thresholds() const {
    if (!_mutex) return {};
    xSemaphoreTake(_mutex,portMAX_DELAY); auto value=_state; xSemaphoreGive(_mutex); return value;
}
void NightscoutSession::publish(const ns_alarm::Snapshot& state) {
    xSemaphoreTake(_mutex,portMAX_DELAY);
    if (!_configure) _state=state;
    xSemaphoreGive(_mutex);
}
void NightscoutSession::message(const char* text, bool error) {
    auto state=thresholds(); strlcpy(state.message,text,sizeof(state.message));
    logs::trimUtf8Tail(state.message); publish(state);
    if (error) logs::logEf("[NS ALARM] %s",text); else logs::logIf("[NS ALARM] %s",text);
}
void NightscoutSession::httpError(const char* action, int code) {
    char text[128];
    const String transportReason = code<0 ? HTTPClient::errorToString(code) : String();
    const char* reason = code==ns_http::no_wifi ? ": Wi-Fi disconnected"
        : code==ns_http::invalid_url ? ": invalid server URL"
        : code==ns_http::body_too_large ? ": response body exceeds limit"
        : code==-1 ? ": connection failed (DNS/TCP/TLS); see NS NET log"
        : code==401 || code==403 ? ": check token permissions"
        : code==404 ? ": API unavailable"
        : code==502 ? ": bad gateway; check Nightscout hosting logs"
        : code==503 ? ": service unavailable" : code==504 ? ": gateway timeout"
        : code<0 ? ": transport error" : "";
    if (code<0 && code!=-1 && transportReason.length())
        snprintf(text,sizeof(text),"%s: transport %d: %s",action,code,transportReason.c_str());
    else snprintf(text,sizeof(text),"%s: %s %d%s",action,code<0?"transport":"HTTP",code,reason);
    logs::trimUtf8Tail(text);
    message(text,true);
}
int NightscoutSession::http(const String& path, String& response) {
    response="";
    if (WiFi.status()!=WL_CONNECTED) return ns_http::no_wifi;
    String base=_config.baseUrl; base.trim();
    if (!base.startsWith("http://") && !base.startsWith("https://")) base=String(_config.preferHttpsIfNoScheme?"https://":"http://")+base;
    while (base.endsWith("/")) base.remove(base.length()-1);
    String url=base+path;
    if (!_config.token.isEmpty()) url += String(path.indexOf('?')<0?'?':'&')+"token="+encode(_config.token);
    WiFiClient plain; WiFiClientSecure secure;
    if (_config.tlsInsecure) secure.setInsecure();
    const uint16_t timeout=ns_http::timeout_ms(_config.httpTimeoutMs);
    secure.setHandshakeTimeout(ns_http::handshake_seconds(timeout));
    HTTPClient request; request.setConnectTimeout(timeout); request.setTimeout(timeout);
    // Такой же транспорт, как у работающего клиента измерений; отдельное соединение.
    request.useHTTP10(true); request.setReuse(false); request.setUserAgent("t-display-s3");
    const bool https=url.startsWith("https://");
    if (!request.begin(https?static_cast<WiFiClient&>(secure):plain,url)) return ns_http::invalid_url;
    request.addHeader("Accept","application/json");
    request.addHeader("Cache-Control","no-cache");
    // В журнал не попадают URL с токеном или тело ответа.
    const int query=path.indexOf('?');
    const String route=query<0?path:path.substring(0,query);
    const uint32_t startedAt=millis(), heapBefore=ESP.getFreeHeap();
    const uint32_t largestBefore=ESP.getMaxAllocHeap();
    int code = request.GET();
    LimitedBody output;
    if (code>=200 && code<300) {
        if (request.getSize()>int(app_constants::http_body_max_bytes)) code=ns_http::body_too_large;
        else {
            const int readResult=request.writeToStream(&output);
            code=ns_http::response_code(code,readResult,output.overflow);
        }
    }
    if (code<0) {
        logs::logEf("[NS NET] GET %s: transport=%d, elapsed=%lu ms, RSSI=%d",
            route.c_str(),code,(unsigned long)(millis()-startedAt),WiFi.RSSI());
        logs::logEf("[NS NET] Wi-Fi=%d, TLS=%d, heap=%lu, largest=%lu, timeout=%u ms",
            int(WiFi.status()),int(https),(unsigned long)heapBefore,(unsigned long)largestBefore,unsigned(timeout));
        if (https) {
            char detail[80]={}; const int tls=secure.lastError(detail,sizeof(detail));
            if (tls<0) logs::logEf("[NS TLS] code=%d: %s",tls,detail);
        }
    }
    response=output.value; request.end();
    return code;
}
bool NightscoutSession::status() {
    String body; int code=http("/api/v1/status.json",body);
    auto state=thresholds();
    if (code!=200) { state.connected=false; publish(state); httpError("Nightscout status",code); return false; }
    const auto invalid = [this,&state](const char* reason) {
        state.connected=false; publish(state); message(reason,true); return false;
    };
    gson::Parser parser; parser.reserve(600);
    if (!parser.parse(body)) return invalid("Invalid status.json");
    auto settings=parser.get("settings"), thresholds=settings.get("thresholds");
    const char* names[]={"bgLow","bgTargetBottom","bgTargetTop","bgHigh"};
    for (unsigned i=0; i<4; ++i) {
        double value;
        if (!number(thresholds.get(names[i]),value)) return invalid("Status missing four glucose thresholds");
        state.bounds[i]=ns_alarm::mgdl_to_mmol(value);
    }
    if (!valid_glucose_bounds(state.bounds)) return invalid("Invalid Nightscout thresholds");
    double epoch;
    // Время сервера — необязательный резерв для часов; импорт порогов от него не зависит.
    if (number(parser.get("serverTimeEpoch"),epoch) && epoch>=1000000000000.0 && epoch<=4102444800000.0) {
        state.serverEpochMs=uint64_t(epoch); state.clockAtMs=millis();
    }
    strlcpy(state.origin,_config.baseUrl.c_str(),sizeof(state.origin));
    state.loaded=true; state.connected=true;
    ++state.revision; publish(state);
    message("Nightscout glucose thresholds loaded");
    return true;
}
void NightscoutSession::run(void* self) { static_cast<NightscoutSession*>(self)->work(); }
void NightscoutSession::work() {
    NightscoutData data;
    for (;;) {
        xSemaphoreTake(_mutex,portMAX_DELAY);
        const bool configure=_configure, update=_update;
        const bool updateFromLan=_updateFromLan;
        if (configure) {
            _config=_nextConfig; _configure=false; _state=ns_alarm::Snapshot{}; xQueueReset(_results);
            strlcpy(_state.message,"Connecting to Nightscout...",sizeof(_state.message));
        }
        _update=false;
        _updateFromLan=false;
        xSemaphoreGive(_mutex);
        if (configure) { _client.begin(_config); _lastStatus=0; _statusAttempted=false; }
        if (update) _client.requestUpdate(updateFromLan);
        if (!_config.baseUrl.isEmpty() && WiFi.status()==WL_CONNECTED) {
            // При запуске сначала запрашиваем серверные пороги, затем измерения.
            // После ошибки повторяем через 30 секунд только до первой успешной загрузки.
            if (!_client.isBusy()) {
                const uint32_t now=millis();
                if (ns_http::startup_status_due(thresholds().loaded,_statusAttempted,now,_lastStatus)) {
                    status(); _lastStatus=millis(); _statusAttempted=true;
                }
            }
            if (_client.tick(data)) xQueueOverwrite(_results,&data);
        } else if (WiFi.status()!=WL_CONNECTED) {
            _client.tick(data); // Дать существующему клиенту отменить незавершённый запрос.
            // Успешно полученный при запуске снимок сохраняется и при потере Wi-Fi.
        }
        xSemaphoreTake(_mutex,portMAX_DELAY);
        _busy=_client.isBusy(); _wanted=_client.isUpdateWanted(); _lastRequest=_client.lastRequestAtMs();
        xSemaphoreGive(_mutex);
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
