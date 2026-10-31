#include "alarm_refresh.h"
#include "alarm_state.h"
#include "nightscout_alarm_state.h"
#include "nightscout_update_source.h"

constexpr bool alarm_edges(uint32_t start) {
    alarm_refresh::State state;
    state.observe(0, start);
    if (state.send_due(start)) return false;
    state.observe(1, start);
    for (unsigned i = 0; i < alarm_refresh::send_attempts; ++i) {
        const uint32_t now = start + i * alarm_refresh::retry_ms;
        if (!state.send_due(now)) return false;
        state.sent(now);
        // Multiple main-loop calls and repeated readings must not restart retries.
        state.observe(1, now + 1);
        if (state.send_due(now + alarm_refresh::retry_ms - 1)) return false;
    }
    if (state.send_due(start + 60000)) return false;
    state.observe(4, start + 60000); // High after low is a new cause.
    if (!state.send_due(start + 60000)) return false;
    state.observe(0, start + 60001); // Recovery cancels unsent retries.
    if (state.send_due(start + 61000)) return false;
    state.observe(8, start + 62000); // Missing data also broadcasts.
    return state.send_due(start + 62000);
}

constexpr bool received_burst(uint32_t start) {
    alarm_refresh::State state;
    if (!state.receive(start)) return false;
    if (state.receive(start + 250) || state.receive(start + 500)) return false;
    if (state.receive(start + alarm_refresh::coalesce_ms - 1)) return false;
    // Reception must never forward a broadcast, even after the cooldown expires.
    if (state.send_due(start + 60000)) return false;
    if (!state.receive(start + alarm_refresh::coalesce_ms)) return false;
    // Host changes use a fresh state: old causes and cooldowns must not leak.
    state = alarm_refresh::State{};
    return state.receive(start + alarm_refresh::coalesce_ms + 1);
}

static_assert(alarm_edges(0), "Broadcast only on alarm transitions, with three spaced attempts");
static_assert(alarm_edges(UINT32_MAX - 100), "Alarm retries survive millis wrap");
static_assert(received_burst(0), "Coalesce peer bursts without forwarding them");
static_assert(received_burst(UINT32_MAX - 100), "Receive cooldown survives millis wrap");

constexpr bool queued_sources() {
    nightscout_update::Source source;
    source.request(false);
    source.start(); // A local request is already running when LAN arrives.
    source.request(true);
    if (source.from_lan()) return false; // Do not relabel the in-flight result.
    source.request(false); // A scheduled poll cannot remove the pending LAN origin.
    source.start();
    if (!source.from_lan()) return false;
    for (unsigned retry = 0; retry < 3; ++retry) {
        source.retry(); // HTTP failure or Wi-Fi disconnect.
        source.request(false);
        source.start();
        if (!source.from_lan()) return false;
    }
    source.request(false);
    source.start(); // A later independent poll must no longer be suppressed.
    return !source.from_lan();
}

constexpr bool remote_alarm_no_echo(uint32_t start) {
    alarm_refresh::State receiver;
    nightscout_update::Source source;
    AlarmState alarm;
    if (!receiver.receive(start)) return false;
    // Receiving REFRESH only requests data; it cannot enable the alarm.
    if (alarm.should_sound(start)) return false;
    source.request(true);
    source.start();
    source.retry();
    source.start();

    const uint32_t response = start + 60000; // Well beyond the two-second coalescing window.
    constexpr float bounds[] = {3.0f, 4.0f, 10.0f, 14.0f};
    const auto kind = ns_alarm::classify(15.0f, 30, bounds);
    alarm.update(response, true, true, kind != ns_alarm::Kind::None);
    receiver.observe(uint8_t(kind), response, source.from_lan());
    if (!alarm.should_sound(response) || receiver.send_due(response)) return false;
    // The next main-loop tick and an independent poll of the same reading cannot echo it.
    receiver.observe(uint8_t(kind), response + 20);
    source.request(false);
    source.start();
    receiver.observe(uint8_t(kind), response + 60000, source.from_lan());
    if (receiver.send_due(response + 60000)) return false;

    // Recovery rearms the detector; a new local alarm broadcasts normally.
    receiver.observe(0, response + 60020);
    const uint32_t next = response + 120000;
    receiver.observe(uint8_t(ns_alarm::Kind::UrgentLow), next, source.from_lan());
    return receiver.send_due(next);
}

constexpr bool remote_changes_cause() {
    alarm_refresh::State state;
    state.observe(1, 1000);
    state.sent(1000);
    state.observe(4, 61000, true); // LAN response changes an already active alarm.
    return !state.send_due(61000) && !state.send_due(62000);
}

static_assert(queued_sources(), "Keep LAN origin across queued requests, failures and reconnects; reset on the next local poll");
static_assert(remote_alarm_no_echo(0), "Delayed LAN response sounds locally without a return broadcast; future local alarm still sends");
static_assert(remote_alarm_no_echo(UINT32_MAX - 100), "No-echo behavior also survives millis wrap");
static_assert(remote_changes_cause(), "Changing alarm type due to a LAN response cannot start new broadcast retries");
