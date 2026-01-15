#pragma once
#include "board_config.h"

#if BOARD_TDISPLAY_S3
  #include "gfx/BoardDisplay_TDisplayS3.h"
  namespace gfx { using Display = BoardDisplay_TDisplayS3; }
#else
  #error "No board selected"
#endif
