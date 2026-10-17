// Навигация и индикация тревоги: сенсорная кнопка CoreS3 и компактный индикатор T-Display.
#pragma once

#include <cstdint>

namespace ui_navigation {
enum class Action { SwitchLeft, SwitchRight, ScrollUp, ScrollDown, CycleAlarmSnooze };
using Callback = void (*)(Action);

void init(Callback callback);
void update(bool logs_active, bool portal_active);
void set_alarm_status(bool enabled, bool valid, bool sounding, bool audio_available,
                      uint32_t remaining_seconds, uint16_t snooze_minutes, bool missing = false, bool critical = false);

void feed_touch(bool pressed, int16_t x, int16_t y);
void poll();
}
