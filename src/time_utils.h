#pragma once
#include <TimeLib.h>

bool isDST(time_t timestamp);
void adjustTimezone(time_t& timestamp);
