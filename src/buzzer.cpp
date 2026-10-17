#include "buzzer.h"
#include "board_config.h"

#if defined(BOARD_TDISPLAY_S3)
#include <Arduino.h>
#include <esp_arduino_version.h>
#include <esp_system.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

namespace buzzer {
namespace {

constexpr uint8_t channel = app_constants::buzzer_channel;
constexpr uint8_t resolution = detail::pwmResolutionBits;
constexpr uint32_t frequency = app_constants::buzzer_frequency_hz;
constexpr gpio_num_t pin = static_cast<gpio_num_t>(PIN_BUZZER);
static_assert(PIN_BUZZER != PIN_BACKLIGHT, "Buzzer and backlight pins must differ");
static_assert(channel < 8 && app_constants::backlight_channel < 8
    && channel / 2 != app_constants::backlight_channel / 2,
    "ESP32-S3 buzzer and backlight must use separate LEDC timers");
static_assert(frequency > 0 && frequency != uint32_t(app_constants::backlight_pwm_hz),
    "Buzzer frequency must be valid and differ from the backlight frequency");

Config cfg;
enum class Mode : uint8_t { Off, Single, Alarm };
Mode mode = Mode::Off;
bool ready = false;
bool attached = false;
bool sounding = false;
uint8_t beepIndex = 0;
bool criticalAlarm = false;
uint8_t volumePercent = 100;
uint32_t startedAt = 0;
uint32_t duration = 0;
uint32_t lastBeep = 0;
StaticSemaphore_t mutexStorage;
SemaphoreHandle_t mutex = nullptr;
TaskHandle_t worker = nullptr;

constexpr bool inactiveHigh() { return app_constants::buzzer_inactive_high; }

bool writeDuty(uint32_t duty) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    return ledcWrite(PIN_BUZZER, duty);
#else
    ledcWrite(channel, duty);
    return true;
#endif
}

void driveInactive() {
    pinMode(PIN_BUZZER, OUTPUT);
    gpio_set_level(pin, inactiveHigh());
}

void silence() {
    // Пока матрица ещё подключена к LEDC, готовим уровень обычного GPIO.
    gpio_set_level(pin, inactiveHigh());
    if (attached) {
        // Нулевое время активного уровня: для active LOW это постоянный HIGH.
        writeDuty(detail::passiveOutputDuty(0, inactiveHigh()));
#if ESP_ARDUINO_VERSION_MAJOR >= 3
        ledcDetach(PIN_BUZZER);
#else
        ledcDetachPin(PIN_BUZZER);
        ledcWrite(channel, 0); // Канал уже отсоединён от входа active LOW.
#endif
        attached = false;
    }
    driveInactive();
    sounding = false;
}

bool startSound(uint32_t durationMs) {
    if (app_constants::buzzer_active) {
        // Активный модуль сам генерирует тон; PWM ему не нужен.
        gpio_set_level(pin, !inactiveHigh());
    } else {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
        // Явный канал и отличающаяся частота отделяют PWM от подсветки.
        attached = ledcAttachChannel(PIN_BUZZER, frequency, resolution, channel);
#else
        if (ledcSetup(channel, frequency, resolution) > 0) {
            ledcAttachPin(PIN_BUZZER, channel);
            attached = true;
        }
#endif
        bool ok = attached;
        if (ok) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
            ok = ledcWriteTone(PIN_BUZZER, frequency) > 0;
#else
            ok = ledcWriteTone(channel, frequency) > 0;
#endif
            if (ok) ok = writeDuty(detail::passiveOutputDuty(volumePercent, inactiveHigh()));
        }
        if (!ok) silence();
        if (!ok) return false;
    }
    duration = durationMs;
    startedAt = millis();
    sounding = true;
    return true;
}

void service(uint32_t now) {
    if (sounding && uint32_t(now - startedAt) >= duration) {
        silence(); // В том числе в короткой паузе между двумя писками.
        if (mode == Mode::Single) mode = Mode::Off;
    }
    if (mode != Mode::Alarm || sounding) return;
    const uint32_t interval = beepIndex ? (criticalAlarm ? 200 : cfg.secondBeepMs)
        : (criticalAlarm ? 1500 : cfg.repeatMs);
    if (uint32_t(now - lastBeep) < interval) return;
    if (!startSound(criticalAlarm ? 100 : cfg.durationMs)) {
        silence();
        mode = Mode::Off;
        ready = false;
        return;
    }
    lastBeep = millis();
    beepIndex = (beepIndex + 1) % (criticalAlarm ? 3 : 2);
}

void run(void*) {
    for (;;) {
        xSemaphoreTake(mutex, portMAX_DELAY);
        service(millis());
        const bool idle = mode == Mode::Off;
        xSemaphoreGive(mutex);
        // Это ожидание планировщика, длительность писков определяет millis().
        // В тишине задача спит до команды; HTTP/Wi-Fi её не задерживают.
        const TickType_t pollTicks = pdMS_TO_TICKS(2) ? pdMS_TO_TICKS(2) : 1;
        ulTaskNotifyTake(pdTRUE, idle ? portMAX_DELAY : pollTicks);
    }
}

void shutdown() { stop(); }

} // namespace

bool begin(const Config& config) {
    if (mutex) return available(); // Однократная инициализация, без звука при повторе.
    driveInactive();
    if (config.durationMs == 0
        || config.secondBeepMs > INT32_MAX || config.repeatMs > INT32_MAX
        || config.durationMs >= config.secondBeepMs
        || config.durationMs >= config.repeatMs) return false;
    cfg = config;
    driveInactive();
    mutex = xSemaphoreCreateMutexStatic(&mutexStorage);
    if (!mutex) return false;
    // Останавливает звук и при ESP.restart() из обработчиков портала.
    if (esp_register_shutdown_handler(shutdown) != ESP_OK) return false;
    ready = true;
    if (xTaskCreate(run, "buzzer", 3072, nullptr, 2, &worker) != pdPASS) {
        ready = false;
        return false;
    }
    return true;
}

bool available() {
    if (!mutex) return false;
    xSemaphoreTake(mutex, portMAX_DELAY);
    const bool result = ready;
    xSemaphoreGive(mutex);
    return result;
}

void setVolume(uint8_t percent) {
    if (!mutex) return;
    const uint8_t volume = percent > 100 ? 100 : percent;
    xSemaphoreTake(mutex, portMAX_DELAY);
    if (volumePercent == volume) {
        xSemaphoreGive(mutex);
        return;
    }
    volumePercent = volume;
    if (volume == 0) {
        mode = Mode::Off;
        silence();
    } else if (attached && !writeDuty(detail::passiveOutputDuty(volumePercent, inactiveHigh()))) {
        mode = Mode::Off;
        silence();
        ready = false;
    }
    xSemaphoreGive(mutex);
    if (worker) xTaskNotifyGive(worker);
}

void setAlarm(bool active, bool critical) {
    if (!mutex) return;
    xSemaphoreTake(mutex, portMAX_DELAY);
    if (!active || !ready || volumePercent == 0) {
        if (mode != Mode::Off || sounding || attached) silence();
        mode = Mode::Off;
    } else if (mode != Mode::Alarm || criticalAlarm != critical) {
        silence();
        mode = Mode::Alarm;
        beepIndex = 0;
        criticalAlarm = critical;
        lastBeep = millis() - (criticalAlarm ? 1500 : cfg.repeatMs);
        service(millis());
    }
    xSemaphoreGive(mutex);
    if (worker) xTaskNotifyGive(worker);
}

bool play(uint32_t durationMs) {
    if (!mutex) return false;
    xSemaphoreTake(mutex, portMAX_DELAY);
    silence();
    mode = Mode::Off;
    const bool result = ready && volumePercent > 0 && durationMs > 0 && durationMs <= INT32_MAX
        && startSound(durationMs);
    if (result) mode = Mode::Single;
    else silence();
    xSemaphoreGive(mutex);
    if (worker) xTaskNotifyGive(worker);
    return result;
}

void stop() {
    if (!mutex) {
        driveInactive();
        return;
    }
    xSemaphoreTake(mutex, portMAX_DELAY);
    mode = Mode::Off;
    silence();
    xSemaphoreGive(mutex);
    if (worker) xTaskNotifyGive(worker);
}

} // namespace buzzer

#endif
