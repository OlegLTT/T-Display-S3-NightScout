// Условия планового опроса Nightscout по зоне глюкозы и возрасту измерения.
#pragma once

#include "app_constants.h"
#include <cstdint>
#include "glucose_zone.h"

namespace nightscout_poll {
constexpr uint32_t interval_ms = app_constants::poll_interval_ms;
constexpr int32_t stale_seconds = app_constants::poll_stale_seconds;

constexpr bool warning_zone(GlucoseZone zone) {
    return zone == GlucoseZone::LowRed || zone == GlucoseZone::LowYellow
        || zone == GlucoseZone::HighYellow || zone == GlucoseZone::HighRed;
}

// После получения первого измерения опрашиваем не чаще раза в минуту при предупреждении или старых данных.
constexpr bool due(uint32_t now, uint32_t last_request, bool have_reading,
                   GlucoseZone zone, int32_t age_seconds) {
    return have_reading && static_cast<uint32_t>(now - last_request) >= interval_ms
        && (warning_zone(zone) || age_seconds > stale_seconds);
}
}
