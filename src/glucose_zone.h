// Проверка порогов и определение зоны глюкозы; крайние границы входят в красную зону.
#pragma once

#include <cmath>

enum class GlucoseZone { Invalid, LowRed, LowYellow, InRange, HighYellow, HighRed };

constexpr bool glucose_finite(float value) {
    return std::isfinite(value);
}

constexpr bool valid_glucose_bounds(const float* b) {
    return glucose_finite(b[0]) && glucose_finite(b[1])
        && glucose_finite(b[2]) && glucose_finite(b[3])
        && b[0] < b[1] && b[1] < b[2] && b[2] < b[3];
}

constexpr GlucoseZone glucose_zone(float value, const float* b) {
    return !glucose_finite(value) || value <= 0 || !valid_glucose_bounds(b)
        ? GlucoseZone::Invalid
        : value <= b[0] ? GlucoseZone::LowRed
        : value <= b[1] ? GlucoseZone::LowYellow
        : value < b[2] ? GlucoseZone::InRange
        : value < b[3] ? GlucoseZone::HighYellow
        : GlucoseZone::HighRed;
}

constexpr bool glucose_zone_is_red(GlucoseZone zone) {
    return zone == GlucoseZone::LowRed || zone == GlucoseZone::HighRed;
}
