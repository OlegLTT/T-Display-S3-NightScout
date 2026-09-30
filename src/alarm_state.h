// Состояние тревоги и общей отсрочки; расчёты времени устойчивы к переполнению millis().
#pragma once

#include <cstdint>

#if __cplusplus >= 201402L
#define ALARM_CONSTEXPR14 constexpr
#else
#define ALARM_CONSTEXPR14
#endif

// Без известной метки времени даётся 15 минут ожидания. Потеря часов не обнуляет возраст.
class MissingDataAlarm {
public:
    static constexpr int32_t stale_after_seconds = 15 * 60;
    constexpr MissingDataAlarm() = default;

    ALARM_CONSTEXPR14 bool update(uint32_t now, int32_t age_seconds) {
        if (!started_) {
            started_ = true;
            measurement_at_ = now;
        }
        if (age_seconds >= 0 && age_seconds != INT32_MAX) {
            expired_ = age_seconds >= stale_after_seconds;
            if (!expired_) measurement_at_ = now - uint32_t(age_seconds) * 1000;
        } else if (uint32_t(now - measurement_at_) >= uint32_t(stale_after_seconds) * 1000) {
            expired_ = true;
        }
        return expired_;
    }

private:
    uint32_t measurement_at_ = 0;
    bool started_ = false;
    bool expired_ = false;
};

class AlarmState {
public:
    constexpr AlarmState() = default;

    ALARM_CONSTEXPR14 void update(uint32_t now, bool red_enabled, bool valid, bool red,
                                  bool missing_enabled = false, bool missing = false) {
        enabled_ = red_enabled || missing_enabled;
        valid_ = valid;
        red_ = red_enabled && valid && red;
        missing_ = missing_enabled && missing;
        if (!enabled_ || (snooze_step_ && remaining_seconds(now) == 0)) {
            snooze_step_ = 0;
        }
    }

    // Через минуту нажатие восстанавливает выбранный срок; раньше — продолжает цикл 30/60/90/120/отмена.
    ALARM_CONSTEXPR14 void cycle_snooze(uint32_t now) {
        if (!enabled_) return;

        if (remaining_seconds(now) == 0) snooze_step_ = 0;
        else if (static_cast<uint32_t>(now - snooze_started_) >= 60000) {
            snooze_started_ = now;
            return;
        }
        snooze_step_ = static_cast<uint8_t>((snooze_step_ + 1) % 5);
        snooze_started_ = now;
    }

    // Остаток округляется вверх до секунды; разность без знака учитывает переполнение счётчика.
    constexpr uint32_t remaining_seconds(uint32_t now) const {
        return snooze_step_ && static_cast<uint32_t>(now - snooze_started_) < duration_ms()
            ? (duration_ms() - static_cast<uint32_t>(now - snooze_started_) + 999) / 1000
            : 0;
    }

    constexpr bool should_sound(uint32_t now) const {
        return enabled_ && (red_ || missing_) && remaining_seconds(now) == 0;
    }
    constexpr bool enabled() const { return enabled_; }
    constexpr bool data_valid() const { return valid_; }
    constexpr bool data_missing() const { return missing_; }
    constexpr uint8_t snooze_minutes() const { return snooze_step_ * 30; }

private:
    constexpr uint32_t duration_ms() const { return uint32_t(snooze_step_) * 30 * 60 * 1000; }
    bool enabled_ = false;
    bool valid_ = false;
    bool red_ = false;
    bool missing_ = false;
    uint8_t snooze_step_ = 0;
    uint32_t snooze_started_ = 0;
};

#undef ALARM_CONSTEXPR14
