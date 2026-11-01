#pragma once

#include "app_constants.h"
#include <cmath>

namespace glucose_delta {
enum class Zone { Invalid, Green, Yellow, Red };

constexpr Zone zone(float delta_mmol) {
    return !std::isfinite(delta_mmol) ? Zone::Invalid
        : std::fabs(delta_mmol) >= app_constants::delta_red_mmol ? Zone::Red
        : std::fabs(delta_mmol) >= app_constants::delta_yellow_mmol ? Zone::Yellow
        : Zone::Green;
}

// Цвет соответствует числу на экране: +0.28 отображается как +0.3 и уже жёлтый.
inline float display_value(float delta_mmol) {
    const float rounded = std::round(delta_mmol * 10.0f) / 10.0f;
    return rounded == 0.0f ? 0.0f : rounded; // Не показывать -0.0.
}
} // namespace glucose_delta
