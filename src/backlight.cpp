// Выбранная яркость хранится отдельно от временного максимума во время тревоги.
#include "app_constants.h"
#include "backlight.h"
#include <esp_arduino_version.h>
#include <driver/gpio.h>

#if defined(BOARD_M5CORE_S3)
  #include "gfx/Display.h"
#endif

Backlight* Backlight::_self = nullptr;

void Backlight::Use(Backlight& instance) {
  _self = &instance;
}

void Backlight::beginStatic() {
  if (_self) _self->begin();
}
void Backlight::nextStatic() {
  if (_self) _self->next();
}
uint8_t Backlight::currentPercentStatic() {
  return _self ? _self->currentPercent() : 100;
}
void Backlight::setPercentStatic(uint8_t p) {
  if (_self) _self->setPercent(p);
}

void Backlight::setLevels(const uint8_t* levels, size_t count) {
  _levels = levels;
  _count = count;
  _idx = 0;
}

// Возвращает выбранный уровень, независимо от временного повышения яркости.
uint8_t Backlight::currentPercent() const {
  if (!_levels || _count == 0) return 100;
  return _levels[_idx];
}

void Backlight::begin() {
  BacklightConfig cfg;
  begin(cfg);
}

void Backlight::begin(const BacklightConfig& cfg) {
  _cfg = cfg;
  _maxDuty = (1u << _cfg.resBits) - 1;

  if (!_levels || _count == 0) {
    setLevels(app_constants::brightness_levels, sizeof(app_constants::brightness_levels) / sizeof(app_constants::brightness_levels[0]));
  }

#if !defined(BOARD_M5CORE_S3)
  pinMode(_cfg.pin, OUTPUT);
#endif

  backlightInit();
  setPercent(currentPercent());
}

void Backlight::backlightInit() {
#if !defined(BOARD_M5CORE_S3)
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  _pwmAttached = ledcAttachChannel(_cfg.pin, _cfg.freq, _cfg.resBits, _cfg.channel);
#else
  _pwmAttached = ledcSetup(_cfg.channel, _cfg.freq, _cfg.resBits) > 0;
  if (_pwmAttached) ledcAttachPin(_cfg.pin, _cfg.channel);
#endif
#endif
}

// На T-Display отключаем PWM и подаём постоянный высокий уровень на подсветку.
void Backlight::backlightForceMax() {
#if defined(BOARD_M5CORE_S3)
  gfx::Display::instance().set_backlight(gfx::Display::brightness_level::max);
#else
  // Готовим HIGH до detach; GPIO не проваливается в плавающее состояние.
  gpio_set_level(static_cast<gpio_num_t>(_cfg.pin), HIGH);
  if (_pwmAttached) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcDetach(_cfg.pin);
#else
    ledcDetachPin(_cfg.pin);
#endif
    _pwmAttached = false;
  }
  pinMode(_cfg.pin, OUTPUT);
  digitalWrite(_cfg.pin, HIGH);

  Serial.printf("Backlight 100%%: GPIO=%d\n", digitalRead(_cfg.pin));
#endif
}

// Во время тревоги запоминаем выбор, но оставляем физическую подсветку на максимуме.
void Backlight::setPercent(uint8_t percent) {
  if (percent < 1) percent = 1;
  if (percent > 100) percent = 100;
  _requestedPercent = percent;
  if (!_alarmBoost) applyPercent(percent);
}

// Меняем аппаратную яркость только при включении или снятии временного максимума.
void Backlight::setAlarmBoost(bool active) {
  if (_alarmBoost == active) return;
  _alarmBoost = active;
  applyPercent(active ? 100 : _requestedPercent);
}

void Backlight::applyPercent(uint8_t percent) {
#if defined(BOARD_M5CORE_S3)
  if (percent < 1) percent = 1;
  if (percent > 100) percent = 100;
  const uint8_t level = (uint16_t)percent * 255 / 100;
  gfx::Display::instance().set_backlight(
      static_cast<gfx::Display::brightness_level>(level));
#else
  if (percent >= 100) {
    backlightForceMax();
    return;
  }
  if (percent < 1) percent = 1;

  if (!_pwmAttached) backlightInit();
  if (!_pwmAttached) {
    backlightForceMax(); // Ошибка PWM не оставляет экран без подсветки.
    return;
  }
  uint32_t duty = (uint32_t)percent * _maxDuty / 100;
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(_cfg.pin, duty);
#else
  ledcWrite(_cfg.channel, duty);
#endif

  Serial.printf("Backlight %u%%: duty=%u/%u\n", percent, duty, _maxDuty);
#endif
}

void Backlight::next() {
  if (!_levels || _count == 0) return;
  _idx = (_idx + 1) % _count;
  uint8_t p = _levels[_idx];
  Serial.printf("Brightness step: %u%%\n", p);
  setPercent(p);
}

// После загрузки выбираем ближайшую ступень яркости из доступных уровней.
void Backlight::restoreFromPercent(uint8_t percent) {

  if (!_levels || _count == 0) {
    setPercent(percent);
    return;
  }

  if (percent < 1)   percent = 1;
  if (percent > 100) percent = 100;

  size_t bestIdx = 0;
  uint8_t bestDiff = 255;

  for (size_t i = 0; i < _count; ++i) {
    uint8_t lvl = _levels[i];
    uint8_t diff = (lvl > percent) ? (lvl - percent) : (percent - lvl);
    if (diff < bestDiff) {
      bestDiff = diff;
      bestIdx = i;
    }
  }

  _idx = bestIdx;
  setPercent(_levels[_idx]);
}
