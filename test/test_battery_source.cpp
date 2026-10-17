#include "battery_source.h"

// Разряженная помпа не должна показывать заряд Blueberry.
static_assert(battery_percent_for_source(BatterySource::Blueberry, 80, 0) == 80, "Blueberry source");
static_assert(battery_percent_for_source(BatterySource::Pump, 80, 0) == 0, "Empty pump battery");
static_assert(battery_percent_for_source(BatterySource::Pump, 80, -1) == -1, "No fallback to Blueberry");
static_assert(battery_percent_for_source(BatterySource::Blueberry, -1, 70) == -1, "No fallback to pump");
static_assert(battery_percent_for_source(BatterySource::Pump, 20, 101) == -1, "Invalid percentage");
static_assert(battery_percent_for_source(BatterySource::Pump, 20, 100) == 100, "Full battery");
static_assert(!valid_battery_source(2) && !valid_battery_source(255), "Invalid persisted source");
static_assert(battery_percent_for_source(static_cast<BatterySource>(2), 80, 70) == -1, "Invalid source");
