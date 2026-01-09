#include "backlight.h"

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

  static const uint8_t defaultLevels[] = {100, 70, 30, 10, 1};
  if (!_levels || _count == 0) {
    setLevels(defaultLevels, sizeof(defaultLevels) / sizeof(defaultLevels[0]));
  }

  pinMode(_cfg.pin, OUTPUT);

  backlightInit();
  setPercent(currentPercent());
}

void Backlight::backlightInit() {
  ledcSetup(_cfg.channel, _cfg.freq, _cfg.resBits);
  ledcAttachPin(_cfg.pin, _cfg.channel);
}

void Backlight::backlightForceMax() {
  ledcWrite(_cfg.channel, 0);
  delay(1);

  ledcDetachPin(_cfg.pin);
  delay(2);

  pinMode(_cfg.pin, OUTPUT);
  digitalWrite(_cfg.pin, HIGH);
  delay(2);

  Serial.printf("FORCE MAX: gpio=%d\n", digitalRead(_cfg.pin));
}

void Backlight::setPercent(uint8_t percent) {
  if (percent >= 100) {
    backlightForceMax();
    return;
  }
  if (percent < 1) percent = 1;

  ledcAttachPin(_cfg.pin, _cfg.channel);
  uint32_t duty = (uint32_t)percent * _maxDuty / 100;
  ledcWrite(_cfg.channel, duty);

  Serial.printf("BL %u%% duty=%u/%u\n", percent, duty, _maxDuty);
}

void Backlight::next() {
  if (!_levels || _count == 0) return;
  _idx = (_idx + 1) % _count;
  uint8_t p = _levels[_idx];
  Serial.printf("Brightness step: %u%%\n", p);
  setPercent(p);
}

void Backlight::restoreFromPercent(uint8_t percent) {
  // Если уровни не заданы — просто выставляем процент как есть
  if (!_levels || _count == 0) {
    setPercent(percent);
    return;
  }

  if (percent < 1)   percent = 1;
  if (percent > 100) percent = 100;

  // Подбираем ближайший уровень в _levels к заданному percent
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
  setPercent(_levels[_idx]); // это уже учтёт 100% через backlightForceMax()
}
