#pragma once

#include <cmath>
#include <cstdint>

struct NightscoutData {
  bool ok = false;
  int http = 0;
  float sgv_mmol = NAN;
  float delta_mmol = NAN;
  uint64_t update_time_ms = 0;
  char trend[24] = "N/A";
  float iob = NAN;
  float cob = NAN;
  // Заряд выбранного источника в процентах; -1 — нет корректных данных.
  int batteryPct = -1;
  float pumpBatV = 0.0f;
  float sgvBounds[4] = {NAN, NAN, NAN, NAN};
  // Возраст вычисляется перед отрисовкой; INT32_MAX означает неизвестное время.
  int32_t ageSec = INT32_MAX;
  bool stale5 = false;
  bool stale15 = false;
};
