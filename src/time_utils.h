#pragma once
#include <Arduino.h>
#include <stdint.h>

void applyTimeConfig(int16_t tzMinutes, bool dstEnable,
                     const char* ntp1="pool.ntp.org",
                     const char* ntp2="time.nist.gov",
                     const char* ntp3=nullptr);
