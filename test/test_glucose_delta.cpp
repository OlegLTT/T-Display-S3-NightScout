#include "glucose_delta.h"
#include <limits>

using glucose_delta::Zone;
using glucose_delta::zone;

static_assert(zone(0.0f) == Zone::Green && zone(-0.0f) == Zone::Green, "Zero delta is green");
static_assert(zone(0.2f) == Zone::Green && zone(-0.2f) == Zone::Green, "Small changes in either direction are green");
static_assert(zone(0.3f) == Zone::Yellow && zone(-0.3f) == Zone::Yellow, "Exact lower boundary is yellow in both directions");
static_assert(zone(0.5f) == Zone::Yellow && zone(-0.5f) == Zone::Yellow, "Moderate changes are yellow");
static_assert(zone(0.6f) == Zone::Red && zone(-0.6f) == Zone::Red, "Exact upper boundary is red in both directions");
static_assert(zone(0.7f) == Zone::Red && zone(-0.7f) == Zone::Red, "Large changes in either direction are red");
static_assert(zone(std::numeric_limits<float>::quiet_NaN()) == Zone::Invalid, "Missing delta has no trend color");
static_assert(zone(std::numeric_limits<float>::infinity()) == Zone::Invalid, "Infinite delta has no trend color");
