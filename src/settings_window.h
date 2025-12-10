#include <pebble.h>

#define SETTINGS_COUNT 4

#ifdef PBL_PLATFORM_EMERY
  #define SETTINGS_LABEL_HEIGHT 42
  #define SETTINGS_LABEL_DISTANCE 12

  #define PREV_BOX_X 12
#elif defined(PBL_PLATFORM_CHALK)
  #define SETTINGS_LABEL_HEIGHT 30
  #define SETTINGS_LABEL_DISTANCE 8

  #define PREV_BOX_X 26
#else
  #define SETTINGS_LABEL_HEIGHT 30
  #define SETTINGS_LABEL_DISTANCE 8

  #define PREV_BOX_X 9
#endif

#define PREV_BOX_H (11*BLOCK_SIZE)
#define PREV_BOX_Y 46

#define SETTINGS_LABEL_PAD_L 14
#define SETTINGS_LABEL_TOP_Y 48
#define SETTINGS_INPUT_PAD_R 12
#define SETTINGS_INPUT_TOP_Y 52

void settings_window_push();