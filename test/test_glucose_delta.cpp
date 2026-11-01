#include "glucose_delta.h"
#include <limits>

using glucose_delta::Zone;
using glucose_delta::zone;
using glucose_delta::per_five_minutes;

static_assert(zone(0.0f) == Zone::Green && zone(-0.0f) == Zone::Green, "Zero delta is green");
static_assert(zone(0.2f) == Zone::Green && zone(-0.2f) == Zone::Green, "Small changes in either direction are green");
static_assert(zone(0.3f) == Zone::Yellow && zone(-0.3f) == Zone::Yellow, "Exact lower boundary is yellow in both directions");
static_assert(zone(0.5f) == Zone::Yellow && zone(-0.5f) == Zone::Yellow, "Moderate changes are yellow");
static_assert(zone(0.6f) == Zone::Red && zone(-0.6f) == Zone::Red, "Exact upper boundary is red in both directions");
static_assert(zone(0.7f) == Zone::Red && zone(-0.7f) == Zone::Red, "Large changes in either direction are red");
static_assert(zone(std::numeric_limits<float>::quiet_NaN()) == Zone::Invalid, "Missing delta has no trend color");
static_assert(zone(std::numeric_limits<float>::infinity()) == Zone::Invalid, "Infinite delta has no trend color");
static_assert(per_five_minutes(0.6f, 10.0f) == 0.3f, "Normalize a ten-minute gap once");
static_assert(per_five_minutes(-0.6f, 10.0f) == -0.3f, "Normalization preserves falling direction");
static_assert(per_five_minutes(0.2f, 1.0f) == 1.0f, "One-minute interval is scaled to five minutes");
static_assert(per_five_minutes(0.5f, 5.0f) == 0.5f, "Five-minute delta is unchanged");
static_assert(zone(per_five_minutes(0.0f, 0.0f)) == Zone::Invalid, "Zero interval is not a stable trend");
static_assert(zone(per_five_minutes(0.1f, -1.0f)) == Zone::Invalid, "Reversed timestamps are invalid");
static_assert(zone(per_five_minutes(0.1f, std::numeric_limits<float>::infinity())) == Zone::Invalid,
              "Unknown interval is invalid");
