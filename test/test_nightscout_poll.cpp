#include "nightscout_poll.h"
#include <climits>

using nightscout_poll::due;

constexpr bool warning_intervals() {
    const GlucoseZone zones[] = {GlucoseZone::LowRed, GlucoseZone::LowYellow,
                                GlucoseZone::HighYellow, GlucoseZone::HighRed};
    for (auto zone : zones) {
        if (due(60999, 1000, true, zone, 30)) return false;
        if (!due(61000, 1000, true, zone, 30)) return false;
        // Same SGV timestamp must not prevent the following refresh.
        if (due(120999, 61000, true, zone, 90)) return false;
        if (!due(121000, 61000, true, zone, 90)) return false;
    }
    return true;
}

static_assert(warning_intervals(), "All yellow/red zones refresh at 60 seconds, never earlier");
static_assert(!due(60000, 0, true, GlucoseZone::InRange, 305), "Fresh green reading waits");
static_assert(due(60000, 0, true, GlucoseZone::InRange, 306), "Stale green reading refreshes");
static_assert(!due(59999, 0, true, GlucoseZone::InRange, 900), "Stale data still respects 60 seconds");
static_assert(!due(60000, 0, false, GlucoseZone::LowRed, INT32_MAX), "Initial request belongs to startup/retry logic");
static_assert(!due(60000, 0, true, GlucoseZone::Invalid, 30), "Invalid value is not a yellow/red zone");
static_assert(due(60000, 0, true, GlucoseZone::Invalid, INT32_MAX), "Unknown data age triggers refresh");
static_assert(!due(119999, 60000, true, GlucoseZone::LowRed, 900), "A retry start resets the planned interval");
static_assert(due(120000, 60000, true, GlucoseZone::LowRed, 900), "Next planned refresh after retry is 60 seconds later");

constexpr uint32_t before_wrap = UINT32_MAX - 10000;
static_assert(!due(before_wrap + 59999u, before_wrap, true, GlucoseZone::HighRed, 30), "No early request across millis wrap");
static_assert(due(before_wrap + 60000u, before_wrap, true, GlucoseZone::HighRed, 30), "Refresh survives millis wrap");

constexpr float bounds[] = {3.9f, 4.5f, 8.5f, 10.0f};
static_assert(due(60000, 0, true, glucose_zone(bounds[0], bounds), 30), "Lower red boundary uses minute polling");
static_assert(due(60000, 0, true, glucose_zone(bounds[1], bounds), 30), "Lower yellow boundary uses minute polling");
static_assert(due(60000, 0, true, glucose_zone(bounds[2], bounds), 30), "Upper yellow boundary uses minute polling");
static_assert(due(60000, 0, true, glucose_zone(bounds[3], bounds), 30), "Upper red boundary uses minute polling");
static_assert(!due(60000, 0, true, glucose_zone(6.0f, bounds), 30), "Returning to green stops minute polling");
