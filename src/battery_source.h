#pragma once
#include <cstdint>

// Номера источников сохраняются в NVS; не менять их при расширении списка.
enum class BatterySource : uint8_t { Blueberry = 0, Pump = 1 };

constexpr bool valid_battery_source(uint8_t value) {
    return value <= static_cast<uint8_t>(BatterySource::Pump);
}

// Ноль — корректный заряд. При отсутствии процентов другой источник не подставляется.
constexpr int battery_percent_for_source(BatterySource source, int blueberry, int pump) {
    return source == BatterySource::Blueberry
        ? (blueberry >= 0 && blueberry <= 100 ? blueberry : -1)
        : source == BatterySource::Pump ? (pump >= 0 && pump <= 100 ? pump : -1) : -1;
}
