// Состояние тревоги и общей отсрочки; расчёты времени устойчивы к переполнению millis().
#pragma once

#include "app_constants.h"
#include <cstdint>

#if __cplusplus >= 201402L
#define ALARM_CONSTEXPR14 constexpr
#else
#define ALARM_CONSTEXPR14
#endif

// Без известной метки времени даётся 15 минут ожидания. Потеря часов не обнуляет возраст.
class MissingDataAlarm {
public:
    static constexpr int32_t stale_after_seconds = app_constants::data_expiry_seconds;
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
        if (!enabled_) clear_snooze();
        else if (snooze_duration_ms_ && remaining_seconds(now) == 0) {
            snooze_step_ = 0;
            snooze_duration_ms_ = 0;
        }
    }

    // Цикл Snooze заимствован из M5_NightscoutMon, Martin Lukasek (mlukasek).
    // Источник: https://github.com/mlukasek/M5_NightscoutMon#buttons
    // Пауза применяется сразу, включая упреждающее нажатие до тревоги.
    ALARM_CONSTEXPR14 void cycle_snooze(uint32_t now) {
        if (!enabled_) return;

        if (!snooze_clicked_ || uint32_t(now - snooze_clicked_at_) >= app_constants::snooze_restart_ms)
            snooze_step_ = 0;
        snooze_step_ = static_cast<uint8_t>((snooze_step_ + 1) % (app_constants::snooze_steps + 1));
        snooze_duration_ms_ = uint32_t(snooze_step_) * app_constants::snooze_step_minutes * 60000;
        snooze_started_ = now;
        snooze_clicked_at_ = now;
        snooze_clicked_ = true;
    }

    ALARM_CONSTEXPR14 void clear_snooze() {
        snooze_step_ = 0;
        snooze_duration_ms_ = 0;
        snooze_clicked_ = false;
    }

    // LAN передаёт общий срок для всех типов тревог; следующий локальный цикл начинается с 1x.
    ALARM_CONSTEXPR14 void receive_snooze(uint32_t now, uint32_t seconds) {
        if (seconds > 86400) return;
        clear_snooze();
        if (!enabled_) return;
        snooze_duration_ms_ = seconds * 1000;
        snooze_started_ = now;
    }

    // Остаток округляется вверх до секунды; разность без знака учитывает переполнение счётчика.
    constexpr uint32_t remaining_seconds(uint32_t now) const {
        return snooze_duration_ms_ && static_cast<uint32_t>(now - snooze_started_) < snooze_duration_ms_
            ? (snooze_duration_ms_ - static_cast<uint32_t>(now - snooze_started_) + 999) / 1000
            : 0;
    }

    constexpr bool should_sound(uint32_t now) const {
        return enabled_ && (red_ || missing_) && remaining_seconds(now) == 0;
    }
    constexpr bool enabled() const { return enabled_; }
    constexpr bool data_valid() const { return valid_; }
    constexpr bool data_missing() const { return missing_; }
    constexpr uint16_t snooze_minutes() const { return (snooze_duration_ms_ + 59999) / 60000; }

private:
    bool enabled_ = false;
    bool valid_ = false;
    bool red_ = false;
    bool missing_ = false;
    uint8_t snooze_step_ = 0;
    uint32_t snooze_started_ = 0;
    uint32_t snooze_duration_ms_ = 0;
    uint32_t snooze_clicked_at_ = 0;
    bool snooze_clicked_ = false;
};

#undef ALARM_CONSTEXPR14
