#include <pebble.h>

#define MAX_SCORES_SHOWN 8

#ifdef PBL_PLATFORM_EMERY
  #define SCORE_LABEL_HEIGHT 20
  #define INPUT_FONT_SIZE 24
#else
  #define SCORE_LABEL_HEIGHT 12 
  #define INPUT_FONT_SIZE 16
#endif

typedef struct {
  char name[4];
  uint32_t score;
  uint8_t level;
  char date[10];
} GameScore;

void new_score_window_push(uint32_t new_score, uint8_t level);
void all_scores_window_push();