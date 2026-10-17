// GPIO13: LEDC существует только на время писка, в паузах — обычный выход.
#pragma once

#include "app_constants.h"
#include <cstdint>

namespace buzzer {

namespace detail {
constexpr uint8_t pwmResolutionBits = 10; // ledcWriteTone: 10 бит в Arduino 2.x/3.x.
constexpr uint32_t pwmPeriod = 1u << pwmResolutionBits;
constexpr uint32_t pwmHalfDuty = pwmPeriod / 2;
// Линейная шкала 0..50% PWM без дополнительного ослабления малых уровней.
// Тишину в паузах обеспечивает отключение LEDC, а не уменьшение duty сигнала.
constexpr uint32_t passiveDuty(uint8_t percent) {
    return percent == 0 ? 0 : percent >= 100 ? pwmHalfDuty
        : (pwmHalfDuty * percent + 50u) / 100u;
}
// LEDC задаёт время HIGH; для входа active LOW активным является время LOW.
constexpr uint32_t passiveOutputDuty(uint8_t percent, bool inactiveHigh) {
    return inactiveHigh ? pwmPeriod - passiveDuty(percent) : passiveDuty(percent);
}
} // namespace detail

// Тип, полярность и фиксированная частота 2000 Гц задаются в app_constants.h.
// Интервалы: 1..INT32_MAX мс; durationMs меньше обоих интервалов между писками.
struct Config {
    uint32_t durationMs = app_constants::alarm_beep_ms;
    uint32_t secondBeepMs = app_constants::alarm_second_beep_ms;
    uint32_t repeatMs = app_constants::alarm_repeat_ms;
};

bool begin(const Config& config = Config{});
bool available();

// Пассивный buzzer: линейная шкала PWM, 0 — полное отключение, 100 — duty 50%.
// Активный buzzer поддерживает только выключение/включение.
void setVolume(uint8_t percent);
// Обычная тревога: два писка; критическая: три коротких с более частым повтором.
void setAlarm(bool active, bool critical = false);
// Одиночный сигнал на фиксированной частоте 2000 Гц.
bool play(uint32_t durationMs = app_constants::alarm_beep_ms);
// Немедленно отсоединяет LEDC и восстанавливает уровень покоя.
void stop();

} // namespace buzzer
