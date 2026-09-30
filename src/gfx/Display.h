// Выбор драйвера дисплея по флагу платы при сборке.
#pragma once
#include "board_config.h"

#if defined(BOARD_TDISPLAY_S3)
  #include "gfx/BoardDisplay_TDisplayS3.h"
  namespace gfx { using Display = BoardDisplay_TDisplayS3; }

#elif defined(BOARD_M5CORE_S3)
  #include "gfx/BoardDisplay_CoreS3.h"
  namespace gfx { using Display = BoardDisplay_CoreS3; }

#else
  #error "No board selected"
#endif
