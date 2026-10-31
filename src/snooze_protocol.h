#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>

// Формат UDP-протокола заимствован из M5_NightscoutMon, Martin Lukasek (mlukasek).
// Источник: https://github.com/mlukasek/M5_NightscoutMon/blob/master/M5_NightscoutMon.ino#L1663
// Время — Unix seconds (UTC); сохранена совместимость с исходным проектом.
namespace snooze_protocol {
constexpr uint16_t port = 50555;
constexpr uint32_t max_seconds = 86400;
constexpr size_t packet_capacity = 96;
constexpr char prefix[] = "M5_Nightscout SNOOZE: USR=";
constexpr char separator[] = ", SnoozeUntil=";
// Дополнительная команда T-Disp-NS; формат SNOOZE остаётся совместимым с M5.
constexpr char refresh_prefix[] = "M5_Nightscout REFRESH: USR=";

#if __cplusplus >= 201402L
#define SNOOZE_CONSTEXPR14 constexpr
#else
#define SNOOZE_CONSTEXPR14 inline
#endif

SNOOZE_CONSTEXPR14 uint16_t user_crc(const char* host) {
    uint16_t crc = 0;
    for (size_t i = 0; host[i]; ++i) {
        crc ^= static_cast<uint8_t>(host[i]);
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : crc >> 1;
    }
    return crc;
}

namespace detail {
SNOOZE_CONSTEXPR14 bool consume(const char* data, size_t size, size_t& pos, const char* expected) {
    for (size_t i = 0; expected[i]; ++i)
        if (pos >= size || data[pos++] != expected[i]) return false;
    return true;
}
SNOOZE_CONSTEXPR14 bool decimal(const char* data, size_t size, size_t& pos, uint32_t& value) {
    const size_t first = pos;
    value = 0;
    while (pos < size && data[pos] >= '0' && data[pos] <= '9') {
        const uint32_t digit = data[pos++] - '0';
        if (value > (UINT32_MAX - digit) / 10) return false;
        value = value * 10 + digit;
    }
    return pos != first;
}
}

SNOOZE_CONSTEXPR14 bool decode(const char* data, size_t size, uint16_t expected_crc, uint32_t& until) {
    if (!size || size >= packet_capacity) return false;
    size_t pos = 0;
    uint32_t crc = 0, deadline = 0;
    if (!detail::consume(data, size, pos, prefix)
        || !detail::decimal(data, size, pos, crc) || crc != expected_crc
        || !detail::consume(data, size, pos, separator)
        || !detail::decimal(data, size, pos, deadline)) return false;
    // Оригинал отправляет завершающий NUL; также принимается пакет без него.
    if (pos != size && !(pos + 1 == size && data[pos] == '\0')) return false;
    until = deadline;
    return true;
}

SNOOZE_CONSTEXPR14 bool seconds_left(uint32_t until, uint64_t epoch, uint32_t& seconds) {
    if (!until) { seconds = 0; return true; } // OFF не требует синхронизированных часов.
    if (!epoch) return false;
    if (until <= epoch) { seconds = 0; return true; }
    if (uint64_t(until) - epoch > max_seconds) return false;
    seconds = uint32_t(uint64_t(until) - epoch);
    return true;
}

inline size_t encode(char* data, size_t capacity, uint16_t crc, uint32_t until) {
    const int size = std::snprintf(data, capacity, "%s%u%s%lu", prefix, unsigned(crc),
        separator, static_cast<unsigned long>(until));
    return size > 0 && size_t(size) < capacity ? size_t(size) + 1 : 0;
}

SNOOZE_CONSTEXPR14 bool decode_refresh(const char* data, size_t size, uint16_t expected_crc) {
    if (!size || size >= packet_capacity) return false;
    size_t pos = 0;
    uint32_t crc = 0;
    if (!detail::consume(data, size, pos, refresh_prefix)
        || !detail::decimal(data, size, pos, crc) || crc != expected_crc) return false;
    return pos == size || (pos + 1 == size && data[pos] == '\0');
}

inline size_t encode_refresh(char* data, size_t capacity, uint16_t crc) {
    const int size = std::snprintf(data, capacity, "%s%u", refresh_prefix, unsigned(crc));
    return size > 0 && size_t(size) < capacity ? size_t(size) + 1 : 0;
}

#undef SNOOZE_CONSTEXPR14
} // namespace snooze_protocol
