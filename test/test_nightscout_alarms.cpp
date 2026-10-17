#include "nightscout_alarm_state.h"
#include "nightscout_http.h"
#include <limits>

constexpr float nsBounds[] = {3.0f,4.0f,10.0f,14.0f};
static_assert(ns_alarm::classify(2.9f,30,nsBounds,15)==ns_alarm::Kind::UrgentLow,"critical low");
static_assert(ns_alarm::classify(3.5f,30,nsBounds,15)==ns_alarm::Kind::None,"low yellow zone must stay silent");
static_assert(ns_alarm::classify(11.f,30,nsBounds,15)==ns_alarm::Kind::None,"high yellow zone must stay silent");
static_assert(ns_alarm::classify(3.f,30,nsBounds,15)==ns_alarm::Kind::UrgentLow,"exact low red boundary must sound");
static_assert(ns_alarm::classify(14.f,30,nsBounds,15)==ns_alarm::Kind::UrgentHigh,"exact high red boundary must sound");
static_assert(ns_alarm::classify(3.001f,30,nsBounds,15)==ns_alarm::Kind::None,"just above low red boundary is silent");
static_assert(ns_alarm::classify(13.999f,30,nsBounds,15)==ns_alarm::Kind::None,"just below high red boundary is silent");
static_assert(ns_alarm::classify(4.f,30,nsBounds,15)==ns_alarm::Kind::None,"inner low boundary is silent");
static_assert(ns_alarm::classify(10.f,30,nsBounds,15)==ns_alarm::Kind::None,"inner high boundary is silent");
static_assert(ns_alarm::classify(15.f,30,nsBounds,15)==ns_alarm::Kind::UrgentHigh,"critical high");
static_assert(ns_alarm::classify(6.f,30,nsBounds,15)==ns_alarm::Kind::None,"normal range");
static_assert(ns_alarm::classify(2.9f,600,nsBounds,15)==ns_alarm::Kind::None,"stale measurements cannot sound");
static_assert(ns_alarm::classify(15.f,INT32_MAX,nsBounds,15)==ns_alarm::Kind::None,"unknown timestamp cannot sound");
static_assert(ns_alarm::classify(15.f,-1,nsBounds,15)==ns_alarm::Kind::None,"future timestamp cannot sound");
static_assert(ns_alarm::classify(std::numeric_limits<float>::quiet_NaN(),0,nsBounds,15)==ns_alarm::Kind::None,"NaN cannot sound");
static_assert(ns_alarm::classify(2.9f,0,nsBounds,0)==ns_alarm::Kind::None,"all alarms disabled");
static_assert(ns_alarm::classify(2.9f,0,nsBounds,2)==ns_alarm::Kind::Low,"disabled critical falls back to enabled ordinary alarm");
static_assert(ns_alarm::classify(15.f,0,nsBounds,4)==ns_alarm::Kind::High,"high red zone permits enabled ordinary fallback");
static_assert(ns_alarm::classify(3.5f,0,nsBounds,2)==ns_alarm::Kind::None,"ordinary low cannot sound in yellow");
static_assert(ns_alarm::classify(11.f,0,nsBounds,4)==ns_alarm::Kind::None,"ordinary high cannot sound in yellow");
static_assert(ns_alarm::mgdl_to_mmol(180.1559f)>9.999f && ns_alarm::mgdl_to_mmol(180.1559f)<10.001f,"status thresholds use Nightscout MMOL_TO_MGDL regardless of display units");

constexpr bool server_clock_wrap() {
    ns_alarm::Snapshot state;
    state.serverEpochMs=1700000000000ULL;
    state.clockAtMs=UINT32_MAX-1000;
    return ns_alarm::server_now(state,state.clockAtMs+30000)==state.serverEpochMs+30000;
}
static_assert(server_clock_wrap(), "Server time remains available for LAN without NTP, including millis wrap");

constexpr bool rejects_invalid_options() {
    ns_alarm::Preferences settings;
    if (!settings.valid()) return false;
    settings.version=2; if (settings.valid()) return false;
    settings=ns_alarm::Preferences{}; settings.counts[0]=7; if (settings.valid()) return false;
    settings=ns_alarm::Preferences{}; settings.minutes[1][1]=settings.minutes[1][0]; if (settings.valid()) return false;
    settings=ns_alarm::Preferences{}; settings.minutes[1][1]=0; if (settings.valid()) return false;
    settings=ns_alarm::Preferences{}; settings.minutes[1][1]=1441; return !settings.valid();
}
static_assert(rejects_invalid_options(),"invalid/corrupt NVS snooze settings must be rejected");

constexpr bool settings_require_live_server() {
    ns_alarm::Snapshot state;
    for (unsigned i=0;i<4;++i) state.bounds[i]=nsBounds[i];
    state.preferences.loaded=true;
    state.preferences.origin[0]='a';
    if (ns_alarm::server_settings_ready(state,"a")) return false;
    state.connected=true;
    if (!ns_alarm::server_settings_ready(state,"a") || ns_alarm::server_settings_ready(state,"b")) return false;
    state.preferences.loaded=false;
    if (ns_alarm::server_settings_ready(state,"a")) return false;
    state.preferences.loaded=true;
    state.bounds[0]=state.bounds[1];
    return !ns_alarm::server_settings_ready(state,"a");
}
static_assert(settings_require_live_server(),"cached NVS settings, a different host or invalid data cannot reveal server bounds");

constexpr bool snooze_options_always_follow_server() {
    ns_alarm::Preferences saved, server;
    saved.origin[0]=server.origin[0]='a';
    saved.overridden=true; saved.enabledMask=2;
    saved.minutes[0][0]=99;
    server.loaded=true; server.counts[0]=2;
    server.minutes[0][0]=7; server.minutes[0][1]=19;
    const auto next=ns_alarm::import_preferences(server,saved);
    if (!next.loaded || !next.overridden || next.enabledMask!=2
        || next.counts[0]!=2 || next.minutes[0][0]!=7 || next.minutes[0][1]!=19) return false;
    saved.origin[0]='b';
    const auto other=ns_alarm::import_preferences(server,saved);
    return !other.overridden && other.enabledMask==server.enabledMask;
}
static_assert(snooze_options_always_follow_server(),"local alarm flags must not freeze server snooze intervals; another host clears overrides");

static_assert(ns_http::timeout_ms(0)==15000 && ns_http::timeout_ms(15000)==15000,
    "alarm HTTP inherits the configured glucose client timeout");
static_assert(ns_http::timeout_ms(1)==1000 && ns_http::timeout_ms(UINT32_MAX)==60000,
    "timeouts fit HTTPClient uint16_t without overflow");
static_assert(ns_http::handshake_seconds(15001)==16,
    "TLS handshake timeout rounds up to seconds");
static_assert(ns_http::response_code(502,-11,false)==502,
    "response body read errors must not mask a server error as a transport failure");
static_assert(ns_http::response_code(200,-11,false)==-11,
    "preserve the actual HTTPClient body read error");
static_assert(ns_http::response_code(200,2,false)==200,
    "successful bounded response keeps HTTP status");
static_assert(ns_http::response_code(200,-10,true)==ns_http::body_too_large,
    "body overflow has a distinct local code");
