#pragma once
#include <cstdint>

namespace ns_http {
// Не пересекаются с отрицательными кодами Arduino HTTPClient (-1 ... -11).
constexpr int no_wifi = -100;
constexpr int invalid_url = -101;
constexpr int body_too_large = -102;

constexpr uint16_t timeout_ms(uint32_t configured) {
    return configured == 0 ? 15000 : configured < 1000 ? 1000
        : configured > 60000 ? 60000 : uint16_t(configured);
}
constexpr uint32_t handshake_seconds(uint16_t timeout) {
    return (uint32_t(timeout)+999)/1000;
}
constexpr bool transient_error(int code) {
    // connect, send headers/payload, disconnected, connection lost, read timeout.
    return code == -1 || code == -2 || code == -3 || code == -4 || code == -5 || code == -11;
}
// Ошибка чтения тела не должна скрывать уже полученную ошибку HTTP сервера.
constexpr int response_code(int status, int readResult, bool overflow) {
    return status < 200 || status >= 300 ? status
        : overflow ? body_too_large : readResult < 0 ? readResult : status;
}
} // namespace ns_http
