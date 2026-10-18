#include "nightscout_alarm_state.h"
#include "nightscout_http.h"
#include <limits>

constexpr float nsBounds[] = {3.0f,4.0f,10.0f,14.0f};
static_assert(ns_alarm::classify(2.9f,30,nsBounds)==ns_alarm::Kind::UrgentLow,"critical low");
static_assert(ns_alarm::classify(3.5f,30,nsBounds)==ns_alarm::Kind::None,"low yellow zone must stay silent");
static_assert(ns_alarm::classify(11.f,30,nsBounds)==ns_alarm::Kind::None,"high yellow zone must stay silent");
static_assert(ns_alarm::classify(3.f,30,nsBounds)==ns_alarm::Kind::UrgentLow,"exact low red boundary must sound");
static_assert(ns_alarm::classify(14.f,30,nsBounds)==ns_alarm::Kind::UrgentHigh,"exact high red boundary must sound");
static_assert(ns_alarm::classify(3.001f,30,nsBounds)==ns_alarm::Kind::None,"just above low red boundary is silent");
static_assert(ns_alarm::classify(13.999f,30,nsBounds)==ns_alarm::Kind::None,"just below high red boundary is silent");
static_assert(ns_alarm::classify(4.f,30,nsBounds)==ns_alarm::Kind::None,"inner low boundary is silent");
static_assert(ns_alarm::classify(10.f,30,nsBounds)==ns_alarm::Kind::None,"inner high boundary is silent");
static_assert(ns_alarm::classify(15.f,30,nsBounds)==ns_alarm::Kind::UrgentHigh,"critical high");
static_assert(ns_alarm::classify(6.f,30,nsBounds)==ns_alarm::Kind::None,"normal range");
static_assert(ns_alarm::classify(2.9f,600,nsBounds)==ns_alarm::Kind::None,"stale measurements cannot sound");
static_assert(ns_alarm::classify(15.f,INT32_MAX,nsBounds)==ns_alarm::Kind::None,"unknown timestamp cannot sound");
static_assert(ns_alarm::classify(15.f,-1,nsBounds)==ns_alarm::Kind::None,"future timestamp cannot sound");
static_assert(ns_alarm::classify(std::numeric_limits<float>::quiet_NaN(),0,nsBounds)==ns_alarm::Kind::None,"NaN cannot sound");
static_assert(ns_alarm::mgdl_to_mmol(180.1559f)>9.999f && ns_alarm::mgdl_to_mmol(180.1559f)<10.001f,"status thresholds use Nightscout MMOL_TO_MGDL regardless of display units");

// Nightscout rounds 3.5 / 4 / 10 / 12 mmol/L to 63 / 72 / 180 / 216 mg/dL.
constexpr float importedBounds[] = {ns_alarm::mgdl_to_mmol(63),ns_alarm::mgdl_to_mmol(72),
    ns_alarm::mgdl_to_mmol(180),ns_alarm::mgdl_to_mmol(216)};
static_assert(ns_alarm::classify(3.4f,30,importedBounds)==ns_alarm::Kind::UrgentLow,
    "server low threshold must be applied");
static_assert(ns_alarm::classify(9.5f,30,importedBounds)==ns_alarm::Kind::None,
    "previous local high threshold of 9 must not trigger an alarm");
static_assert(ns_alarm::classify(12.1f,30,importedBounds)==ns_alarm::Kind::UrgentHigh,
    "server high threshold must be applied");
static_assert(ns_alarm::cached_bounds_ready(importedBounds,"server.test","server.test",true),
    "a validated server cache is usable offline after restart");
static_assert(!ns_alarm::cached_bounds_ready(importedBounds,"server.test","server.test",false),
    "old manual and factory bounds without the server-only cache marker must be discarded");
static_assert(!ns_alarm::cached_bounds_ready(importedBounds,"server.test","other.test"),
    "switching server must invalidate the previous cache");
static_assert(!ns_alarm::cached_bounds_ready(importedBounds,"","server.test"),
    "bounds without a server origin must not be used");
constexpr float missingBounds[] = {std::numeric_limits<float>::quiet_NaN(),
    std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::quiet_NaN(),
    std::numeric_limits<float>::quiet_NaN()};
static_assert(!ns_alarm::cached_bounds_ready(missingBounds,"server.test","server.test"),
    "invalid cache must wait for Nightscout");
static_assert(ns_alarm::classify(2.0f,30,missingBounds)==ns_alarm::Kind::None
    && ns_alarm::classify(20.0f,30,missingBounds)==ns_alarm::Kind::None,
    "glucose alarms must not use factory or manual fallback thresholds before the first import");

constexpr bool server_clock_wrap() {
    ns_alarm::Snapshot state;
    state.serverEpochMs=1700000000000ULL;
    state.clockAtMs=UINT32_MAX-1000;
    return ns_alarm::server_now(state,state.clockAtMs+30000)==state.serverEpochMs+30000;
}
static_assert(server_clock_wrap(), "Server time remains available for LAN without NTP, including millis wrap");

constexpr bool settings_require_live_server() {
    ns_alarm::Snapshot state;
    for (unsigned i=0;i<4;++i) state.bounds[i]=nsBounds[i];
    state.loaded=true;
    state.origin[0]='a';
    if (ns_alarm::server_bounds_ready(state,"a")) return false;
    state.connected=true;
    if (!ns_alarm::server_bounds_ready(state,"a") || ns_alarm::server_bounds_ready(state,"b")) return false;
    state.loaded=false;
    if (ns_alarm::server_bounds_ready(state,"a")) return false;
    state.loaded=true;
    state.bounds[0]=state.bounds[1];
    return !ns_alarm::server_bounds_ready(state,"a");
}
static_assert(settings_require_live_server(),"only a valid threshold snapshot from the selected live server can be imported");

static_assert(ns_http::timeout_ms(0)==15000 && ns_http::timeout_ms(15000)==15000,
    "alarm HTTP inherits the configured glucose client timeout");
static_assert(ns_http::timeout_ms(1)==1000 && ns_http::timeout_ms(UINT32_MAX)==60000,
    "timeouts fit HTTPClient uint16_t without overflow");
static_assert(ns_http::handshake_seconds(15001)==16,
    "TLS handshake timeout rounds up to seconds");
static_assert(ns_http::startup_status_due(false,false,0,0),
    "thresholds must be requested on startup, including at millis zero");
static_assert(!ns_http::startup_status_due(false,true,29999,0)
    && ns_http::startup_status_due(false,true,30000,0),
    "a failed startup request retries after 30 seconds without a tight loop at millis zero");
static_assert(!ns_http::startup_status_due(true,true,300000,0)
    && !ns_http::startup_status_due(true,true,UINT32_MAX,0),
    "loaded thresholds must never be polled again until a new startup/session");
static_assert(!ns_http::startup_status_due(false,true,28998,UINT32_MAX-1000)
    && ns_http::startup_status_due(false,true,28999,UINT32_MAX-1000),
    "startup retries preserve their interval across millis wrap");
static_assert(ns_http::response_code(502,-11,false)==502,
    "response body read errors must not mask a server error as a transport failure");
static_assert(ns_http::response_code(200,-11,false)==-11,
    "preserve the actual HTTPClient body read error");
static_assert(ns_http::response_code(200,2,false)==200,
    "successful bounded response keeps HTTP status");
static_assert(ns_http::response_code(200,-10,true)==ns_http::body_too_large,
    "body overflow has a distinct local code");
