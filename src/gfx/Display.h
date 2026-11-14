// Выбор драйвера дисплея по флагу платы при сборке.
#pragma once
#include "board_config.h"

#if defined(BOARD_TDISPLAY_S3)
  #include "gfx/BoardDisplay_TDisplayS3.h"
  namespace gfx { using Display = BoardDisplay_TDisplayS3; }

#elif defined(BOARD_USES_M5UNIFIED)
  #include "gfx/BoardDisplay_M5Stack.h"
  namespace gfx { using Display = BoardDisplay_M5Stack; }

#else
  #error "No board selected"
#endif
