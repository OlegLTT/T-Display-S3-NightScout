#pragma once
#include <ctime>
#include <cstdint>
bool getLocalTime(tm* info, uint32_t timeout_ms = 5000);
