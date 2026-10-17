// Логика LAN-синхронизации Snooze заимствована из M5_NightscoutMon,
// Martin Lukasek (mlukasek), и адаптирована для T-Display S3 и CoreS3.
// Источник: https://github.com/mlukasek/M5_NightscoutMon/blob/master/M5_NightscoutMon.ino#L1663
#include "snooze_lan.h"
#include "snooze_protocol.h"
#include "logs.h"
#include <WiFi.h>
#include <cstring>

namespace {
constexpr uint32_t retry_ms = 250;
constexpr uint32_t poll_ms = 20;
constexpr uint8_t send_attempts = 3;
}

void SnoozeLan::announce(const AlarmState& alarm, uint64_t epoch, uint32_t now) {
    const uint32_t seconds = alarm.remaining_seconds(now);
    deadline_ready_ = !seconds || (epoch && epoch + seconds <= UINT32_MAX);
    until_ = seconds && deadline_ready_ ? uint32_t(epoch + seconds) : 0;
    pending_ = true;
    retries_ = send_attempts;
    last_send_ = now - retry_ms;
    last_poll_ = now - poll_ms;
}

void SnoozeLan::tick(AlarmState& alarm, const char* host, uint64_t epoch, uint32_t now) {
    if (!alarm.enabled()) pending_ = false;
    if (std::strcmp(host_, host) != 0) {
        // Пауза прежнего пользователя не переносится на другой Nightscout.
        if (host_[0]) { alarm.clear_snooze(); pending_ = false; }
        strlcpy(host_, host, sizeof(host_));
        received_ = false;
    }
    const IPAddress local = WiFi.localIP();
    if (WiFi.status() != WL_CONNECTED || !host_[0] || !uint32_t(local)) {
        if (bound_) udp_.stop();
        bound_ = false;
        retry_bind_at_ = now;
        return;
    }
    if (bound_ && address_ != local) { udp_.stop(); bound_ = false; }
    if (!bound_) {
        if (uint32_t(now - retry_bind_at_) < 1000) return;
        retry_bind_at_ = now;
        // Слушаем STA, чтобы AP портала не участвовал в обмене Snooze.
        bound_ = udp_.begin(local, snooze_protocol::port) != 0;
        if (!bound_) { logs::logW("[SNOOZE LAN] Cannot bind UDP port"); return; }
        address_ = local;
    }
    // WiFiUDP выделяет буфер при каждом parsePacket(), даже без входящего пакета.
    if (uint32_t(now - last_poll_) < poll_ms) return;
    last_poll_ = now;

    const uint16_t crc = snooze_protocol::user_crc(host_);
    // Ограниченный пакетный цикл не задерживает LVGL и обработку кнопок.
    for (unsigned count = 0; count < 4; ++count) {
        const int size = udp_.parsePacket();
        if (size <= 0) break;
        const IPAddress sender = udp_.remoteIP();
        if (sender == local || size >= int(snooze_protocol::packet_capacity)) {
            udp_.flush(); continue;
        }
        char data[snooze_protocol::packet_capacity];
        const int read = udp_.read(reinterpret_cast<uint8_t*>(data), sizeof(data));
        uint32_t until = 0, seconds = 0;
        if (read != size || !snooze_protocol::decode(data, size_t(read), crc, until)
            || !snooze_protocol::seconds_left(until, epoch, seconds)) continue;
        if (received_ && sender == received_from_ && until == received_until_
            && uint32_t(now - received_at_) < 2000) continue;
        received_ = true; received_from_ = sender; received_until_ = until; received_at_ = now;
        pending_ = false; // Полученную паузу не рассылаем обратно.
        alarm.receive_snooze(now, seconds);
        logs::logIf("[SNOOZE LAN] Received: %lu s", static_cast<unsigned long>(seconds));
    }

    if (!pending_ || uint32_t(now - last_send_) < retry_ms) return;
    if (deadline_ready_ && until_ && epoch >= until_) { pending_ = false; return; }
    if (!deadline_ready_) {
        const uint32_t seconds = alarm.remaining_seconds(now);
        if (!seconds) { pending_ = false; return; }
        if (!epoch || epoch + seconds > UINT32_MAX) return;
        until_ = uint32_t(epoch + seconds);
        deadline_ready_ = true;
    }
    char data[snooze_protocol::packet_capacity];
    const size_t size = snooze_protocol::encode(data, sizeof(data), crc, until_);
    const IPAddress broadcast(uint32_t(local) | ~uint32_t(WiFi.subnetMask()));
    bool sent = size && udp_.beginPacket(broadcast, snooze_protocol::port);
    if (sent) { udp_.write(reinterpret_cast<const uint8_t*>(data), size); sent = udp_.endPacket() != 0; }
    last_send_ = now;
    if (!--retries_) {
        pending_ = false;
        if (!sent) logs::logW("[SNOOZE LAN] Broadcast failed; local snooze remains active");
    }
}
