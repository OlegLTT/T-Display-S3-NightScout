#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>

class BrightnessOverlay {
public:
  void begin(TFT_eSPI& tft, int screenW, int screenH);

  void show(uint8_t percent, uint32_t durationMs = 2000);

  // true => нужно перерисовать базовый экран (показ/скрытие/изменение)
  bool update();

  // рисует поверх уже отрисованного экрана
  void render();

  bool visible() const { return _visible; }

private:
  void drawContent();

  TFT_eSPI* _tft = nullptr;
  TFT_eSprite* _spr = nullptr;

  int _w = 0, _h = 0;
  uint8_t _pct = 0;

  bool _visible = false;
  bool _dirty = false;

  uint32_t _tShow = 0;
  uint32_t _duration = 0;

  int _boxW = 100;
  int _boxH = 50;
  int _boxX = 0;
  int _boxY = 0;
};
