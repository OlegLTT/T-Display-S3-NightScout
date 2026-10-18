#pragma once
#include "nightscout_client.h"
#include "nightscout_alarm_state.h"
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

// Все DNS/TCP/TLS/HTTP выполняются одной задачей, интерфейс получает только копии результатов.
class NightscoutSession {
public:
    void begin(const NSConfig& cfg);
    void requestUpdate();
    bool tick(NightscoutData& out);
    bool isBusy() const;
    bool isUpdateWanted() const;
    uint32_t lastRequestAtMs() const;
    ns_alarm::Snapshot thresholds() const;

private:
    mutable SemaphoreHandle_t _mutex = nullptr;
    TaskHandle_t _task = nullptr;
    QueueHandle_t _results = nullptr;
    NightscoutClient _client;
    NSConfig _nextConfig;
    NSConfig _config;
    ns_alarm::Snapshot _state;
    bool _configure = false, _update = false, _busy = false, _wanted = false;
    uint32_t _lastRequest = 0;
    uint32_t _lastStatus = 0;
    bool _statusAttempted = false;
    static void run(void* self);
    void work();
    int http(const String& path, String& response);
    bool status();
    void publish(const ns_alarm::Snapshot& state);
    void message(const char* text, bool error = false);
    void httpError(const char* action, int code);
};
