#pragma once

#include <cstdint>

namespace alarm_refresh {
constexpr uint32_t retry_ms = 250;
constexpr uint32_t coalesce_ms = 2000;
constexpr uint8_t send_attempts = 3;

#if __cplusplus >= 201402L
#define REFRESH_CONSTEXPR14 constexpr
#else
#define REFRESH_CONSTEXPR14
#endif

// Отдельное состояние от звука и Snooze: тревожные данные важны и при громкости 0.
class State {
public:
    REFRESH_CONSTEXPR14 void observe(uint8_t cause, uint32_t now, bool from_lan = false) {
        if (cause == cause_) return;
        cause_ = cause;
        // Тревогу по полученным данным включаем локально, без ответного REFRESH.
        retries_ = cause && !from_lan ? send_attempts : 0;
        last_send_ = now - retry_ms;
    }

    constexpr bool send_due(uint32_t now) const {
        return retries_ && uint32_t(now - last_send_) >= retry_ms;
    }

    REFRESH_CONSTEXPR14 void sent(uint32_t now) {
        if (retries_) --retries_;
        last_send_ = now;
    }

    // Объединяем повторы и одновременные сигналы нескольких дисплеев в один запрос.
    // Полученный пакет сам по себе никогда не запускает исходящую рассылку.
    REFRESH_CONSTEXPR14 bool receive(uint32_t now) {
        if (received_ && uint32_t(now - received_at_) < coalesce_ms) return false;
        received_ = true;
        received_at_ = now;
        return true;
    }

private:
    uint8_t cause_ = 0, retries_ = 0;
    uint32_t last_send_ = 0, received_at_ = 0;
    bool received_ = false;
};

#undef REFRESH_CONSTEXPR14
} // namespace alarm_refresh
