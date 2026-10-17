// Проверки электрических ограничений шкалы: тишина при 0 и максимум 50% PWM.
#include "buzzer.h"

constexpr bool valid_volume_range() {
    uint32_t previous = 0;
    for (unsigned percent = 0; percent <= 255; ++percent) {
        const uint32_t duty = buzzer::detail::passiveDuty(static_cast<uint8_t>(percent));
        if (duty < previous || duty > 512) return false;
        if (percent == 0 && duty != 0) return false;
        if (percent > 0 && duty == 0) return false;
        if (percent < 100 && duty == 512) return false;
        if (percent >= 100 && duty != 512) return false;
        previous = duty;
    }
    return true;
}

static_assert(valid_volume_range(), "PWM volume must be monotonic, mute at 0, and bounded at 50%");

// Без инверсии GPIO-матрицы время LOW нужно считать из полного периода LEDC.
constexpr bool valid_output_polarity() {
    uint32_t previousHigh = 0;
    uint32_t previousLow = buzzer::detail::pwmPeriod;
    for (unsigned percent = 0; percent <= 255; ++percent) {
        const uint8_t volume = static_cast<uint8_t>(percent);
        const uint32_t high = buzzer::detail::passiveOutputDuty(volume, false);
        const uint32_t low = buzzer::detail::passiveOutputDuty(volume, true);
        if (high < previousHigh || high > buzzer::detail::pwmHalfDuty) return false;
        if (low > previousLow || low < buzzer::detail::pwmHalfDuty
            || low > buzzer::detail::pwmPeriod) return false;
        if (high + low != buzzer::detail::pwmPeriod) return false;
        previousHigh = high;
        previousLow = low;
    }
    return buzzer::detail::passiveOutputDuty(0, false) == 0
        && buzzer::detail::passiveOutputDuty(0, true) == buzzer::detail::pwmPeriod
        && buzzer::detail::passiveOutputDuty(100, false) == buzzer::detail::pwmHalfDuty
        && buzzer::detail::passiveOutputDuty(100, true) == buzzer::detail::pwmHalfDuty;
}

static_assert(valid_output_polarity(), "PWM must mute and increase active time correctly for both polarities");
static_assert(buzzer::detail::passiveDuty(30) < buzzer::detail::passiveDuty(50)
    && buzzer::detail::passiveDuty(50) < buzzer::detail::passiveDuty(75)
    && buzzer::detail::passiveDuty(75) < buzzer::detail::passiveDuty(100),
    "Volume must continue to increase above 30%");
