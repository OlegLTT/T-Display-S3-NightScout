#pragma once

#include <cstdint>
#include "glucose_zone.h"

namespace ns_alarm {
constexpr unsigned option_count = 6;
constexpr uint16_t max_snooze_minutes = 1440;
constexpr int32_t max_age_seconds = 600; // Nightscout simplealarms: только данные моложе 10 минут.
enum class Kind : uint8_t { None, UrgentLow, Low, High, UrgentHigh };
constexpr uint8_t level(Kind kind) {
    return kind == Kind::UrgentLow || kind == Kind::UrgentHigh ? 2 : kind == Kind::None ? 0 : 1;
}
constexpr unsigned index(Kind kind) { return static_cast<unsigned>(kind) - 1; }

struct Preferences {
    uint32_t version = 1;
    bool loaded = false;
    bool overridden = false; // Сохранено для совместимости NVS; флаги сервера не управляют звуком.
    uint8_t enabledMask = 15;
    uint8_t counts[4] = {3, 4, 4, 4};
    uint16_t minutes[4][option_count] = {{15,30,45}, {15,30,45,60}, {30,60,90,120}, {30,60,90,120}};
    char origin[96] = {};

#if __cplusplus >= 201402L
    constexpr
#endif
    bool valid() const {
        if (version != 1 || enabledMask > 15 || origin[sizeof(origin)-1] != 0) return false;
        for (unsigned k=0; k<4; ++k) {
            if (!counts[k] || counts[k] > option_count) return false;
            for (unsigned i=0; i<counts[k]; ++i) {
                if (!minutes[k][i] || minutes[k][i] > max_snooze_minutes) return false;
                for (unsigned j=0; j<i; ++j) if (minutes[k][j] == minutes[k][i]) return false;
            }
        }
        return true;
    }
};

// Пороги в status.settings.thresholds нормализованы Nightscout в mg/dl независимо от units.
constexpr float mgdl_to_mmol(float value) { return value / 18.01559f; }
constexpr Kind classify(float glucose, int32_t age, const float* bounds) {
    // Звуковая тревога совпадает с красной зоной дисплея, включая крайние границы.
    return age < 0 || age >= max_age_seconds || glucose != glucose || glucose > 100
        || !glucose_zone_is_red(glucose_zone(glucose,bounds)) ? Kind::None
        : glucose <= bounds[0] ? Kind::UrgentLow : Kind::UrgentHigh;
}

struct Snapshot {
    Preferences preferences;
    float bounds[4] = {};
    bool connected = false;
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
bool server_settings_ready(const Snapshot& state, const char* origin) {
    if (!state.connected || !state.preferences.loaded || !state.preferences.valid()
        || !valid_glucose_bounds(state.bounds)) return false;
    for (unsigned i=0; i<sizeof(state.preferences.origin); ++i) {
        if (state.preferences.origin[i]!=origin[i]) return false;
        if (!origin[i]) return true;
    }
    return false;
}

constexpr uint64_t server_now(const Snapshot& state, uint32_t now) {
    return state.serverEpochMs ? state.serverEpochMs + uint32_t(now - state.clockAtMs) : 0;
}
} // namespace ns_alarm
