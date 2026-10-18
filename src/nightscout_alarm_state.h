#pragma once

#include <cstdint>
#include "glucose_zone.h"

namespace ns_alarm {
constexpr int32_t max_age_seconds = 600; // Nightscout simplealarms: только данные моложе 10 минут.
enum class Kind : uint8_t { None, UrgentLow, Low, High, UrgentHigh };
constexpr uint8_t level(Kind kind) {
    return kind == Kind::UrgentLow || kind == Kind::UrgentHigh ? 2 : kind == Kind::None ? 0 : 1;
}

// Пороги в status.settings.thresholds нормализованы Nightscout в mg/dl независимо от units.
constexpr float mgdl_to_mmol(float value) { return value / 18.01559f; }
constexpr Kind classify(float glucose, int32_t age, const float* bounds) {
    // Звуковая тревога совпадает с красной зоной дисплея, включая крайние границы.
    return age < 0 || age >= max_age_seconds || glucose != glucose || glucose > 100
        || !glucose_zone_is_red(glucose_zone(glucose,bounds)) ? Kind::None
        : glucose <= bounds[0] ? Kind::UrgentLow : Kind::UrgentHigh;
}

struct Snapshot {
    float bounds[4] = {};
    char origin[96] = {};
    bool loaded = false;
    bool connected = false; // Последний status.json успешно проверен; от текущего Wi-Fi не зависит.
    uint32_t revision = 0;
    uint64_t serverEpochMs = 0;
    uint32_t clockAtMs = 0;
    char message[128] = {};
};

#if __cplusplus >= 201402L
constexpr
#else
inline
#endif
bool cached_bounds_ready(const float* bounds, const char* cachedOrigin, const char* origin,
                         bool serverCache = true) {
    if (!serverCache || !cachedOrigin[0] || !origin[0] || !valid_glucose_bounds(bounds)) return false;
    for (unsigned i=0; i<sizeof(Snapshot::origin); ++i) {
        if (cachedOrigin[i]!=origin[i]) return false;
        if (!origin[i]) return true;
    }
    return false;
}

#if __cplusplus >= 201402L
constexpr
#else
inline
#endif
bool server_bounds_ready(const Snapshot& state, const char* origin) {
    return state.connected && state.loaded && cached_bounds_ready(state.bounds,state.origin,origin);
}

constexpr uint64_t server_now(const Snapshot& state, uint32_t now) {
    return state.serverEpochMs ? state.serverEpochMs + uint32_t(now - state.clockAtMs) : 0;
}
} // namespace ns_alarm
