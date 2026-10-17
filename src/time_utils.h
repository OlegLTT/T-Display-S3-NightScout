// Настройка часового пояса, летнего времени и серверов синхронизации часов.
#pragma once
#include "app_constants.h"
#include <Arduino.h>
#include <stdint.h>

void applyTimeConfig(int16_t tzMinutes, bool dstEnable,
                     const char* ntp1=app_constants::ntp_primary,
                     const char* ntp2=app_constants::ntp_secondary,
                     const char* ntp3=nullptr);
