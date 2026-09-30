#include "alarm_state.h"
#include "glucose_zone.h"
#include <limits>

// Compile with C++14 and -fsyntax-only: these tests execute in the compiler,
// using the same state machine as the firmware, without an attached device.
constexpr uint32_t minute = 60000;

constexpr bool snooze_cycle() {
    AlarmState alarm;
    alarm.update(0, true, true, true);
    if (!alarm.should_sound(0)) return false;
    for (uint8_t step = 1; step <= 4; ++step) {
        const uint32_t now = step * 1000;
        alarm.cycle_snooze(now);
        if (alarm.snooze_minutes() != step * 30
            || alarm.remaining_seconds(now) != step * 30 * 60
            || alarm.should_sound(now)) return false;
    }
    alarm.cycle_snooze(5 * 1000);
    if (alarm.remaining_seconds(5 * 1000) || !alarm.should_sound(5 * 1000)) return false;
    alarm.cycle_snooze(6 * 1000);
    return alarm.snooze_minutes() == 30 && alarm.remaining_seconds(6 * 1000) == 1800;
}

constexpr bool expiry_and_reentry() {
    AlarmState alarm;
    alarm.update(0, true, true, false);
    if (alarm.should_sound(0)) return false;
    alarm.update(0, true, true, true);
    alarm.cycle_snooze(0);
    alarm.update(minute, true, true, false);
    alarm.update(2 * minute, true, true, true);
    if (alarm.should_sound(2 * minute) || alarm.remaining_seconds(2 * minute) != 28 * 60) return false;
    if (alarm.remaining_seconds(30 * minute - 1) != 1) return false;
    alarm.update(30 * minute, true, true, true);
    if (!alarm.should_sound(30 * minute) || alarm.snooze_minutes()) return false;
    alarm.cycle_snooze(30 * minute);
    alarm.update(60 * minute, true, true, false);
    if (alarm.should_sound(60 * minute) || alarm.snooze_minutes()) return false;
    alarm.update(61 * minute, true, true, true);
    return alarm.should_sound(61 * minute);
}

constexpr bool restart_selection_after_minute(uint32_t start) {
    for (uint8_t step = 1; step <= 4; ++step) {
        AlarmState alarm;
        alarm.update(start, true, true, true);
        for (uint8_t i = 0; i < step; ++i) alarm.cycle_snooze(start);
        alarm.update(start + minute, true, true, true);
        alarm.cycle_snooze(start + minute);
        if (alarm.snooze_minutes() != step * 30
            || alarm.remaining_seconds(start + minute) != step * 1800) return false;
        alarm.cycle_snooze(start + minute + 1000);
        if (alarm.snooze_minutes() != ((step + 1) % 5) * 30) return false;
    }
    AlarmState alarm;
    alarm.update(start, true, true, true);
    alarm.cycle_snooze(start);
    alarm.cycle_snooze(start + minute - 1);
    return alarm.snooze_minutes() == 60;
}

static_assert(restart_selection_after_minute(0), "Restore each selected duration after 60s; advance before 60s");
static_assert(restart_selection_after_minute(UINT32_MAX - 30000), "Selection reset survives millis wrap");

constexpr bool disabled_and_invalid() {
    AlarmState alarm;
    alarm.update(0, false, true, true);
    alarm.cycle_snooze(0);
    if (alarm.should_sound(0) || alarm.snooze_minutes()) return false;
    alarm.update(0, true, false, true);
    if (alarm.should_sound(0)) return false;
    alarm.cycle_snooze(0);
    alarm.update(minute, true, false, true);
    if (alarm.remaining_seconds(minute) != 29 * 60) return false;
    alarm.update(30 * minute, true, false, true);
    if (alarm.should_sound(30 * minute)) return false;
    alarm.update(31 * minute, true, true, true);
    if (!alarm.should_sound(31 * minute)) return false;
    alarm.cycle_snooze(31 * minute);
    alarm.update(32 * minute, false, true, true);
    return !alarm.should_sound(32 * minute) && !alarm.snooze_minutes();
}

constexpr bool timer_wraparound() {
    AlarmState alarm;
    const uint32_t start = UINT32_MAX - minute;
    alarm.update(start, true, true, true);
    for (int i = 0; i < 4; ++i) alarm.cycle_snooze(start);
    if (alarm.remaining_seconds(start + 2 * minute) != 118 * 60
        || alarm.should_sound(start + 2 * minute)) return false;
    alarm.update(start + 120 * minute, true, true, true);
    return alarm.should_sound(start + 120 * minute) && !alarm.snooze_minutes();
}

static_assert(snooze_cycle(), "30 -> 60 -> 90 -> 120 -> off; restart timer at each click");
static_assert(expiry_and_reentry(), "Keep snooze on reentry; resume on expiry only if still red");
static_assert(disabled_and_invalid(), "No alarm with disabled option or invalid/stale data");
static_assert(timer_wraparound(), "Snooze must survive millis() wraparound");

constexpr bool missing_data_age() {
    MissingDataAlarm missing;
    if (missing.update(0, INT32_MAX)) return false;
    if (missing.update(15 * minute - 1, INT32_MAX)) return false;
    if (!missing.update(15 * minute, INT32_MAX)) return false;
    // A fresh measurement stops the alarm. Fetching old data does not.
    if (missing.update(16 * minute, 30)) return false;
    if (missing.update(17 * minute, 90)) return false;
    if (missing.update(16 * minute + 869999, 899)) return false;
    if (!missing.update(16 * minute + 870000, 900)) return false;
    if (!missing.update(32 * minute, 990)) return false;
    if (!missing.update(33 * minute, INT32_MAX)) return false;
    return !missing.update(34 * minute, 0);
}

constexpr bool missing_data_clock_loss() {
    MissingDataAlarm missing;
    const uint32_t start = UINT32_MAX - minute;
    if (missing.update(start, 14 * 60)) return false;
    if (missing.update(start + minute - 1, INT32_MAX)) return false;
    if (!missing.update(start + minute, INT32_MAX)) return false;
    // An expired alarm stays active even across a long uptime/wrap.
    if (!missing.update(start, INT32_MAX)) return false;
    return !missing.update(start + 1, 5);
}

constexpr bool missing_data_options_and_snooze() {
    AlarmState alarm;
    alarm.update(0, false, false, false, false, true);
    if (alarm.should_sound(0) || alarm.enabled()) return false;
    alarm.update(0, false, false, false, true, true);
    if (!alarm.should_sound(0) || !alarm.data_missing()) return false;
    for (uint8_t step = 1; step <= 4; ++step) {
        alarm.cycle_snooze(step * 1000);
        if (alarm.snooze_minutes() != step * 30
            || alarm.should_sound(step * 1000)) return false;
    }
    alarm.cycle_snooze(5 * 1000);
    if (!alarm.should_sound(5 * 1000)) return false;
    alarm.cycle_snooze(6 * 1000);
    // Fresh red data during the shared snooze must also remain silent.
    alarm.update(7 * minute, true, true, true, true, false);
    if (alarm.should_sound(7 * minute) || alarm.data_missing()) return false;
    alarm.update(36 * minute, true, false, true, true, true);
    if (!alarm.should_sound(36 * minute)) return false;
    alarm.update(37 * minute, true, true, false, true, false);
    if (alarm.should_sound(37 * minute)) return false;
    // Only the missing-data option is enabled: fresh red data stays silent.
    alarm.update(38 * minute, false, true, true, true, false);
    if (alarm.should_sound(38 * minute)) return false;
    // Only the red option is enabled: stale red data stays silent.
    alarm.update(39 * minute, true, false, true, false, true);
    if (alarm.should_sound(39 * minute)) return false;
    alarm.cycle_snooze(39 * minute);
    alarm.update(40 * minute, false, false, false, false, true);
    return !alarm.should_sound(40 * minute) && !alarm.snooze_minutes();
}

static_assert(missing_data_age(), "Missing data: startup grace, 15-minute boundary, stale replies, recovery");
static_assert(missing_data_clock_loss(), "Preserve measurement age without wall clock and across millis wrap");
static_assert(missing_data_options_and_snooze(), "Independent alarm options share the complete snooze cycle");

constexpr float bounds[] = {3.5f, 4.5f, 10.0f, 12.0f};
constexpr float bad_bounds[] = {3.5f, 4.5f, 4.5f, 12.0f};
constexpr float invalid_nan = std::numeric_limits<float>::quiet_NaN();
constexpr float inf = std::numeric_limits<float>::infinity();
constexpr float invalid_bounds[] = {3.5f, 4.5f, 10.0f, inf};
static_assert(glucose_zone(3.4f, bounds) == GlucoseZone::LowRed, "Low red boundary");
static_assert(glucose_zone(3.5f, bounds) == GlucoseZone::LowRed, "Exact lower red bound is red");
static_assert(glucose_zone(3.6f, bounds) == GlucoseZone::LowYellow, "Above lower red bound is yellow");
static_assert(glucose_zone(4.5f, bounds) == GlucoseZone::LowYellow, "Exact lower yellow bound is yellow");
static_assert(glucose_zone(4.6f, bounds) == GlucoseZone::InRange, "Above lower yellow bound is green");
static_assert(glucose_zone(9.9f, bounds) == GlucoseZone::InRange, "Below upper yellow bound is green");
static_assert(glucose_zone(10.0f, bounds) == GlucoseZone::HighYellow, "Exact upper yellow bound is yellow");
static_assert(glucose_zone(11.9f, bounds) == GlucoseZone::HighYellow, "Below upper red bound is yellow");
static_assert(glucose_zone(12.0f, bounds) == GlucoseZone::HighRed, "Exact upper red bound is red");
static_assert(glucose_zone(12.1f, bounds) == GlucoseZone::HighRed, "High red boundary");
static_assert(glucose_zone_is_red(glucose_zone(bounds[0], bounds)), "Alarm includes lower red boundary");
static_assert(glucose_zone_is_red(glucose_zone(bounds[3], bounds)), "Alarm includes upper red boundary");
static_assert(!glucose_zone_is_red(glucose_zone(bounds[1], bounds)), "Lower yellow boundary must not alarm");
static_assert(!glucose_zone_is_red(glucose_zone(bounds[2], bounds)), "Upper yellow boundary must not alarm");
static_assert(glucose_zone(invalid_nan, bounds) == GlucoseZone::Invalid, "NaN must not alarm");
static_assert(glucose_zone(inf, bounds) == GlucoseZone::Invalid, "Infinity must not alarm");
static_assert(glucose_zone(0, bounds) == GlucoseZone::Invalid, "Missing glucose must not alarm");
static_assert(glucose_zone(-1, bounds) == GlucoseZone::Invalid, "Negative glucose must not alarm");
static_assert(!valid_glucose_bounds(bad_bounds), "Thresholds must be ordered");
static_assert(!valid_glucose_bounds(invalid_bounds), "Thresholds must be finite");
static_assert(!glucose_zone_is_red(glucose_zone(20, bad_bounds)), "Invalid thresholds must not alarm");
