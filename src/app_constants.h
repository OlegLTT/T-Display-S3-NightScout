#pragma once

#include <cstddef>
#include <cstdint>
#include "battery_source.h"

// Общие параметры поведения прошивки. После изменения нужна сборка и прошивка.
// Значения сохранены при переносе: этот файл сам по себе не меняет поведение.
// default_* — заводские значения. Сохранённые в NVS настройки имеют приоритет;
// новые значения применяются при отсутствии ключа или после заводского сброса.
// Распиновка находится в include/board_config.h, конфигурация LVGL — в
// include/lv_conf.h, размеры и размещение элементов — в файлах интерфейса.
// Суффиксы: _ms — миллисекунды, _seconds — секунды, _minutes — минуты,
// _hz — герцы, _percent — проценты, _bytes — байты, _px — пиксели.
namespace app_constants {

// Данные и опрос Nightscout.
// Минимальный интервал плановых запросов после первого измерения. В зелёной
// зоне запрос нужен лишь при возрасте > poll_stale_seconds; в жёлтой/красной
// зоне выполняется каждый интервал. Повторы после ошибки имеют свой таймер.
constexpr uint32_t poll_interval_ms = 60000;
constexpr int32_t poll_stale_seconds = 305; // 5 минут + запас на обновление сервера.
// С этого возраста числа скрываются и разрешается тревога потери данных.
// При неизвестном времени это также срок ожидания после старта отслеживания.
constexpr int32_t data_expiry_seconds = 15 * 60;
// Порог stale5 по секундам; приглушение часов использует полные минуты:
// при ageMin <= fresh_age_minutes обычный цвет, затем приглушённый.
constexpr int32_t fresh_age_minutes = 5;
constexpr uint32_t ui_refresh_ms = 15000; // Перерисовка возраста без новых данных.
constexpr uint32_t http_timeout_ms = 15000; // Тайм-аут HTTP клиента.
constexpr uint32_t host_error_backoff_ms = 60000; // Пауза после ошибки соединения с хостом.
constexpr uint32_t ns_retry_delay_ms = 1500; // Пауза повторной попытки Nightscout.
constexpr uint8_t ns_retry_max = 0; // 0 — без ограничения числа повторов.
constexpr bool prefer_https = true; // HTTPS, если схема URL не указана.
constexpr bool tls_insecure = true; // true отключает проверку сертификата сервера.
constexpr int ns_warn_rssi_dbm = -78; // Меньше этого RSSI — предупреждение в журнале.
constexpr uint32_t weak_rssi_log_ms = 8000; // Ограничение частоты сообщений о слабом Wi-Fi.
constexpr uint32_t no_wifi_log_ms = 5000; // Ограничение частоты сообщений об отсутствии сети.
constexpr size_t http_read_chunk_bytes = 512; // Порция чтения за шаг; влияет на отзывчивость цикла.
constexpr size_t http_body_max_bytes = 32 * 1024; // Максимум JSON; превышение завершает запрос ошибкой.
constexpr size_t http_header_max_bytes = 1024; // Максимальная длина одной строки заголовка.

// Заводские локальные настройки. Пороги глюкозы загружаются только из Nightscout.
constexpr int16_t default_timezone_minutes = 0; // UTC; восточные смещения положительны.
constexpr bool default_dst = false;
constexpr uint8_t default_volume_percent = 31; // 0 — тишина, 100 — максимум.
// Источник значка батареи по умолчанию: Blueberry — загрузчик, Pump — помпа.
// Сохранённый выбор в настройках имеет приоритет. Используются проценты, не вольты.
constexpr BatterySource default_battery_source = BatterySource::Blueberry;
constexpr uint8_t default_brightness_percent = 100; // Допустимо 1..100.

// Отсрочка: шаг * номер ступени, затем отмена. По умолчанию 30/60/90/120/отмена.
// Нажатия с интервалом меньше snooze_restart_ms продолжают цикл; после — с 1x.
// Максимум минут должен помещаться в uint8_t (до 255).
constexpr uint8_t snooze_step_minutes = 30;
constexpr uint8_t snooze_steps = 4;
constexpr uint32_t snooze_restart_ms = 2000;
// Общий рисунок звука для обеих плат: два писка. Интервалы считаются от начала
// предыдущего писка, а не от его конца; длительность должна быть меньше интервала.
constexpr uint32_t alarm_frequency_hz = 1800; // Встроенный динамик CoreS3.
constexpr uint32_t alarm_beep_ms = 180;
constexpr uint32_t alarm_second_beep_ms = 360;
constexpr uint32_t alarm_repeat_ms = 4000;
constexpr uint8_t cores3_initial_volume = 80; // Аппаратная шкала 0..255 до первого обновления настроек.

// GPIO13 T-Display: установлен пассивный трёхконтактный модуль с входом active LOW.
// module_input=true — управляющий вход модуля; active=false — сигнал через PWM.
// inactive_high=true — HIGH отключает модуль, LOW включает его.
constexpr bool buzzer_active = false;
constexpr bool buzzer_module_input = true;
constexpr bool buzzer_inactive_high = true;
constexpr uint32_t buzzer_frequency_hz = 2000; // Фиксированная частота; при 100% громкости duty 50%.

// Wi-Fi: начальное подключение блокирующее; дальнейшие попытки идут из tick().
constexpr int wifi_slots = 5; // Число сохраняемых сетей и строк в веб-форме.
constexpr size_t wifi_credential_capacity = 10; // Внутренний запас WifiManager, не меньше wifi_slots.
constexpr char wifi_default_hostname[] = "NS-Monitior";
constexpr char wifi_tdisplay_hostname[] = "NS-Monitior";
constexpr char wifi_cores3_hostname[] = "NS-Monitior";
constexpr uint32_t wifi_connect_timeout_ms = 20000; // Ожидание при начальном выборе сети.
constexpr uint32_t wifi_retry_delay_ms = 500; // Пауза между проверками WiFiMulti при первом подключении.
constexpr uint8_t wifi_begin_attempts = 3;
constexpr bool wifi_auto_reconnect = true;
constexpr uint32_t wifi_reconnect_period_ms = 3000; // Интервал запуска переподключений.
constexpr uint32_t wifi_reconnect_window_ms = 8000; // Ожидание результата переподключения.
constexpr int wifi_warn_rssi_dbm = -75; // Предупреждение при первом успешном подключении.
constexpr char portal_ssid[] = "NS-Monitior-Setup";
constexpr uint8_t portal_password_length = 8; // Генерируемый пароль AP: допустимо 8..63 символа.

// Время: проверка epoch отделяет несинхронизированные часы от рабочего времени.
constexpr int32_t valid_epoch_seconds = 100000;
constexpr uint32_t ntp_resync_seconds = 24 * 60 * 60;
constexpr char ntp_primary[] = "pool.ntp.org";
constexpr char ntp_secondary[] = "time.nist.gov";

// Кнопки T-Display (uButton). Увеличение debounce лучше фильтрует дребезг,
// но замедляет реакцию. hold — порог удержания, step — задержка/период повтора.
constexpr uint16_t button_debounce_ms = 50;
constexpr uint16_t button_hold_ms = 600;
constexpr uint16_t button_step_delay_ms = 400;
constexpr uint16_t button_step_period_ms = 100;
constexpr uint16_t button_click_ms = 500;
static_assert(button_debounce_ms <= 255, "uButton debounce timer is 8-bit");
// CoreS3: ожидание второго нажатия POWER; задерживает одиночное действие.
constexpr uint32_t cores3_double_click_ms = 400;
constexpr int32_t swipe_threshold_px = 35; // Минимальное смещение для распознавания свайпа.
constexpr uint32_t log_scroll_ms = 90; // Минимальная пауза прокрутки кнопками T-Display.

// Журнал: память приблизительно capacity * (message_bytes + служебные поля).
// При заполнении стирается самая старая запись. Длина включает завершающий ноль;
// более длинный текст обрезается. UTF-8 буквы могут занимать несколько байтов.
constexpr size_t log_capacity = 100;
constexpr size_t log_message_bytes = 150;
constexpr int log_visible_rows = 60; // Число создаваемых строк LVGL, не ёмкость истории.
constexpr uint32_t log_refresh_ms = 120; // Частота обновления окна журнала.
constexpr uint32_t serial_baud = 115200; // Должно совпадать с monitor_speed в platformio.ini.

// Яркость и анимации. Список уровней идёт в порядке переключения кнопкой;
// при старте выбирается ближайший к сохранённой яркости. Уровни — 1..100.
constexpr uint8_t brightness_levels[] = {100, 70, 30, 10, 1};
constexpr uint32_t brightness_overlay_ms = 2000;
constexpr uint32_t screen_animation_ms = 220;
constexpr uint32_t default_animation_ms = 250;
constexpr uint32_t portal_refresh_ms = 500; // Обновление информации экрана настроек.
// LEDC относится к T-Display. Каналы buzzer и подсветки должны использовать
// разные таймеры; текущие 2 и 7 не конфликтуют. Не менять без проверки ESP32-S3.
constexpr int buzzer_channel = 2;
constexpr int backlight_channel = 7;
constexpr int backlight_pwm_hz = 12000;
constexpr int backlight_resolution_bits = 10;

// Пороги цветовой индикации; не изменяют условия звуковой тревоги.
constexpr float delta_neutral_mmol = 0.05f; // В пределах ±порога дельта серая.
constexpr int battery_red_percent = 15; // Заряд <= порога красный.
constexpr int battery_yellow_percent = 35; // Далее <= порога жёлтый, выше — зелёный.

static_assert(snooze_step_minutes > 0 && snooze_steps > 0
    && unsigned(snooze_step_minutes) * snooze_steps <= 255, "Invalid snooze range");
static_assert(alarm_beep_ms > 0 && alarm_beep_ms < alarm_second_beep_ms
    && alarm_beep_ms < alarm_repeat_ms, "Alarm beeps must not overlap");
static_assert(buzzer_active || buzzer_module_input || !buzzer_inactive_high,
    "A passive two-pin buzzer to GND must idle LOW; HIGH causes DC current");
static_assert(default_volume_percent <= 100 && default_brightness_percent >= 1
    && default_brightness_percent <= 100, "Invalid default percentage");
static_assert(log_capacity > 0 && log_message_bytes > 1 && log_visible_rows > 0,
    "Log buffer must not be empty");
static_assert(wifi_slots > 0 && wifi_slots <= wifi_credential_capacity, "Too many Wi-Fi slots");
static_assert(portal_password_length >= 8 && portal_password_length <= 63, "Invalid AP password length");
static_assert(data_expiry_seconds > fresh_age_minutes * 60 && data_expiry_seconds % 60 == 0,
    "Data expiry must be in whole minutes and exceed fresh age");
} // namespace app_constants
