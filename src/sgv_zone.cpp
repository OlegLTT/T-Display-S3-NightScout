#include "sgv_zone.h"
#include "settings.h"
#include <TFT_eSPI.h> // для TFT_RED/YELLOW/GREEN

uint16_t colorBySgvBounds(float sgv) {
  const float* b = appCfg.sgvBounds;

  // red:  sgv <= b0  OR sgv >= b3
  if (sgv <= b[0] || sgv >= b[3]) return TFT_RED;

  // yellow: (b0 < sgv <= b1) OR (b2 <= sgv < b3)
  if ((sgv > b[0] && sgv <= b[1]) || (sgv >= b[2] && sgv < b[3])) return TFT_YELLOW;

  // green: b1 < sgv < b2
  if (sgv > b[1] && sgv < b[2]) return TFT_GREEN;

  return TFT_YELLOW;
}
