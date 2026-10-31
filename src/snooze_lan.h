#pragma once

#include "alarm_state.h"
#include "alarm_refresh.h"
#include <WiFiUdp.h>

// Используется только из основного цикла; HTTP и TLS здесь не выполняются.
class SnoozeLan {
public:
    // true: сосед запросил обновление Nightscout. cause=0 означает отсутствие тревоги.
    bool tick(AlarmState& alarm, const char* host, uint64_t epoch, uint32_t now, uint8_t cause,
              bool from_lan = false);
    void announce(const AlarmState& alarm, uint64_t epoch, uint32_t now);

private:
    WiFiUDP udp_;
    IPAddress address_;
    char host_[96] = {};
    bool bound_ = false, pending_ = false, deadline_ready_ = false;
    bool received_ = false;
    uint8_t retries_ = 0;
    uint32_t until_ = 0, last_send_ = 0, last_poll_ = 0, retry_bind_at_ = 0;
    uint32_t received_until_ = 0, received_at_ = 0;
    IPAddress received_from_;
    alarm_refresh::State refresh_;
    bool broadcast(const char* data, size_t size);
};
