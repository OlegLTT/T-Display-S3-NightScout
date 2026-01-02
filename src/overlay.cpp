#include "overlay.h"

void BrightnessOverlay::begin(TFT_eSPI& tft, int screenW, int screenH) {
  _tft = &tft;
  _w = screenW;
  _h = screenH;

  _boxX = (_w - _boxW) / 2;
  _boxY = (_h - _boxH) / 2;

  // если begin() вызовут повторно
  if (_spr) {
    _spr->deleteSprite();
    delete _spr;
    _spr = nullptr;
  }

  _spr = new TFT_eSprite(_tft);
  _spr->setColorDepth(8);          // 8 бит достаточно для простого оверлея
  _spr->createSprite(_boxW, _boxH);

  _spr->fillSprite(TFT_BLUE);
  _spr->setTextFont(2);
  _spr->setTextSize(2);
  _spr->setTextDatum(MC_DATUM);
}

void BrightnessOverlay::show(uint8_t percent, uint32_t durationMs) {
  _pct = percent;
  _duration = durationMs;
  _tShow = millis();
  _visible = true;
  _dirty = true;
}

bool BrightnessOverlay::update() {
  bool needRedraw = false;

  if (_visible) {
    if (_dirty) {
      drawContent();
      _dirty = false;
      needRedraw = true;  // нужно нарисовать оверлей поверх
    }
    if ((millis() - _tShow) >= _duration) {
      _visible = false;
      needRedraw = true;  // нужно перерисовать базовый экран, чтобы убрать оверлей
    }
  }

  return needRedraw;
}

void BrightnessOverlay::drawContent() {
  if (!_spr) return;

   _spr->fillSprite(TFT_BLUE);
 // _spr->drawRoundRect(0, 0, _boxW, _boxH, 10, TFT_WHITE);

  String s = String(_pct) + "%";
  _spr->setTextColor(TFT_WHITE, TFT_BLUE);
  _spr->drawString(s, _boxW / 2, _boxH / 2);
}

void BrightnessOverlay::render() {
  if (!_visible || !_spr) return;
  _spr->pushSprite(_boxX, _boxY);
}
