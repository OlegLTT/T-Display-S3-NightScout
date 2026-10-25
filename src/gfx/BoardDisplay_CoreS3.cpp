// Дисплей, динамик, сенсор и кнопка питания CoreS3 через M5Unified.
#include "../app_constants.h"
#include "board_config.h"

#if BOARD_M5CORE_S3

#include "BoardDisplay_CoreS3.h"
#include <Arduino.h>
#include <M5Unified.h>

namespace gfx {

namespace {
constexpr uint32_t DOUBLE_CLICK_MS = app_constants::cores3_double_click_ms;
}

// Первый вызов из setup создаёт драйвер после инициализации Arduino.
BoardDisplay_CoreS3& BoardDisplay_CoreS3::instance() {

    static BoardDisplay_CoreS3 display;
    return display;
}

BoardDisplay_CoreS3::BoardDisplay_CoreS3() {
    auto cfg = M5.config();
    cfg.internal_spk = true;
    cfg.internal_mic = false;
    cfg.internal_imu = false;
    cfg.internal_rtc = false;
    M5.begin(cfg);
    M5.Speaker.setVolume(app_constants::cores3_initial_volume);
    m_sound_available = M5.Speaker.begin() && M5.Speaker.isRunning();

    M5.Display.setRotation(1);
    M5.Display.setColorDepth(16);

    M5.Display.setSwapBytes(true);
    set_backlight(brightness_level::max);
}

uint16_t BoardDisplay_CoreS3::width() const {
    return M5.Display.width();
}

uint16_t BoardDisplay_CoreS3::height() const {
    return M5.Display.height();
}

// Яркость преобразуется в аппаратные ступени контроллера питания CoreS3.
void BoardDisplay_CoreS3::set_backlight(brightness_level level) {

    const uint8_t raw = static_cast<uint8_t>(level);
    const uint16_t step = (uint16_t(raw) + 16) / 32;
    const uint8_t mapped = raw == 0 ? 0
        : step == 0 ? 1
        : static_cast<uint8_t>(step * 32 - 1);
    M5.Display.setBrightness(mapped);
}

void BoardDisplay_CoreS3::set_alarm_volume(uint8_t percent) {
    M5.Speaker.setVolume((uint16_t(percent > 100 ? 100 : percent) * 255 + 50) / 100);
}

void BoardDisplay_CoreS3::set_alarm_sound(bool active, bool critical) {
    active = active && m_sound_available;
    if (active == m_alarm_active && (!active || critical == m_critical_alarm)) return;
    m_alarm_active = active;
    M5.Speaker.stop(0);
    if (!active) {
        return;
    }
    m_critical_alarm = critical;
    m_beep_index = 0;
    const uint32_t now = millis();
    m_last_beep = now - (critical ? app_constants::alarm_critical_repeat_ms : app_constants::alarm_repeat_ms);
    update_alarm_sound(now);
}

// Количество и интервалы совпадают с T-Display; конечная длительность не зависит от задержек цикла.
void BoardDisplay_CoreS3::update_alarm_sound(uint32_t now) {
    if (!m_alarm_active) return;
    const uint32_t interval = m_beep_index
        ? (m_critical_alarm ? app_constants::alarm_critical_beep_interval_ms : app_constants::alarm_second_beep_ms)
        : (m_critical_alarm ? app_constants::alarm_critical_repeat_ms : app_constants::alarm_repeat_ms);
    if (now - m_last_beep < interval) return;

    const uint32_t duration = m_critical_alarm ? app_constants::alarm_critical_beep_ms : app_constants::alarm_beep_ms;
    if (!M5.Speaker.tone(app_constants::alarm_frequency_hz, duration, 0, true)) {
        M5.Speaker.stop(0);
        m_alarm_active = false;
        m_sound_available = false;
        return;
    }
    m_last_beep = now;
    m_beep_index = (m_beep_index + 1) % (m_critical_alarm
        ? app_constants::alarm_critical_beep_count : app_constants::alarm_beep_count);
}

void BoardDisplay_CoreS3::set_transfer_done_callback(transfer_done_callback_t cb, void* user_data) {
    m_on_transfer_done_callback = cb;
    m_on_transfer_done_user_data = user_data;
}

void BoardDisplay_CoreS3::draw_bitmap(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t* data) {
    const int32_t w = static_cast<int32_t>(x2) - x1 + 1;
    const int32_t h = static_cast<int32_t>(y2) - y1 + 1;
    M5.Display.pushImage(x1, y1, w, h, data);
    M5.Display.waitDMA();

    if (m_on_transfer_done_callback) {
        m_on_transfer_done_callback(m_on_transfer_done_user_data);
    }
}

void BoardDisplay_CoreS3::update() {
    M5.update();

    const uint32_t now = millis();
    update_alarm_sound(now);

    // Обрабатывается POWER через контроллер питания; аппаратная кнопка RST в эти события не входит.
    if (M5.BtnPWR.wasHold()) {
        m_button_click_pending = false;
        m_button_event = button_event::hold;
    } else if (M5.BtnPWR.wasClicked()) {
        if (m_button_click_pending && now - m_button_clicked_at <= DOUBLE_CLICK_MS) {
            m_button_click_pending = false;
            m_button_event = button_event::double_click;
        } else {
            if (m_button_click_pending) {
                m_button_event = button_event::click;
            }
            m_button_click_pending = true;
            m_button_clicked_at = now;
        }
    } else if (m_button_click_pending && now - m_button_clicked_at >= DOUBLE_CLICK_MS) {
        m_button_click_pending = false;
        m_button_event = button_event::click;
    }
}

BoardDisplay_CoreS3::button_event BoardDisplay_CoreS3::take_button_event() {
    const auto event = m_button_event;
    m_button_event = button_event::none;
    return event;
}

void BoardDisplay_CoreS3::reset_button_events() {

    M5.Power.getKeyState();
    M5.BtnPWR.setState(millis(), m5::Button_Class::button_state_t::state_nochange);
    m_button_click_pending = false;
    m_button_clicked_at = 0;
    m_button_event = button_event::none;
}

bool BoardDisplay_CoreS3::get_touch(uint16_t& x, uint16_t& y) const {
    if (M5.Touch.getCount() == 0) {
        return false;
    }

    const auto& touch = M5.Touch.getDetail();
    if (!touch.isPressed() || touch.x < 0 || touch.y < 0
        || touch.x >= width() || touch.y >= height()) {
        return false;
    }

    x = static_cast<uint16_t>(touch.x);
    y = static_cast<uint16_t>(touch.y);
    return true;
}

}

#endif
