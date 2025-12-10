#include <pebble.h>

#ifdef PBL_PLATFORM_EMERY
  #define LABEL_HEIGHT 28
#else
  #define LABEL_HEIGHT 20 
#endif

#if defined(PBL_PLATFORM_CHALK)
  #define GRID_PIXEL_WIDTH (BLOCK_SIZE * GAME_GRID_BLOCK_WIDTH)
  #define GRID_PIXEL_HEIGHT (BLOCK_SIZE * GAME_GRID_BLOCK_HEIGHT)

  #define GRID_ORIGIN_X ((SCREEN_WIDTH - GRID_PIXEL_WIDTH) / 2)
  #define GRID_ORIGIN_Y ((SCREEN_HEIGHT - GRID_PIXEL_HEIGHT) / 2)

  #define LABEL_SCORE_X 0
  #define LABEL_SCORE_Y (GRID_ORIGIN_Y + BLOCK_SIZE * 7)
  #define LABEL_LEVEL_X (GRID_ORIGIN_X + GRID_PIXEL_WIDTH + 1)
  #define LABEL_LEVEL_Y (LABEL_SCORE_Y)
  #define LABEL_LINES_X (GRID_ORIGIN_X + GRID_PIXEL_WIDTH + 1)
  #define LABEL_LINES_Y (LABEL_LEVEL_Y + LABEL_HEIGHT)
  #define LABEL_PAUSE_Y (LABEL_SCORE_Y + 1)
  #define LABEL_PAUSE_H (BLOCK_SIZE * 3 - 1)

  #define LABEL_WIDTH (SCREEN_WIDTH - (GRID_ORIGIN_X + GRID_PIXEL_WIDTH))
#else
  #define GRID_PIXEL_WIDTH (BLOCK_SIZE * GAME_GRID_BLOCK_WIDTH)
  #define GRID_PIXEL_HEIGHT (BLOCK_SIZE * GAME_GRID_BLOCK_HEIGHT)

  #define GRID_PADDING ((SCREEN_HEIGHT - GRID_PIXEL_HEIGHT) / 2)
  
  #define GRID_ORIGIN_X (GRID_PADDING) // have same padding to the left as top
  #define GRID_ORIGIN_Y (GRID_PADDING)
  
  #define LABEL_X (GRID_ORIGIN_X + GRID_PIXEL_WIDTH + GRID_PADDING + 1)
  #define LABEL_SCORE_X (LABEL_X)
  #define LABEL_SCORE_Y (GRID_ORIGIN_Y + BLOCK_SIZE * 6)
  #define LABEL_LEVEL_X (LABEL_X)
  #define LABEL_LEVEL_Y (GRID_ORIGIN_Y + BLOCK_SIZE * 6 + LABEL_HEIGHT*2)
  #define LABEL_LINES_X (LABEL_X)
  #define LABEL_LINES_Y (GRID_ORIGIN_Y + BLOCK_SIZE * 6 + LABEL_HEIGHT*3)
  #define LABEL_PAUSE_Y (LABEL_SCORE_Y + 1)
  #define LABEL_PAUSE_H (BLOCK_SIZE * 3 - 1)
  #define LABEL_WIDTH (SCREEN_WIDTH - (GRID_ORIGIN_X + GRID_PIXEL_WIDTH) - (2 * GRID_PADDING))
#endif

enum GameStatus {
	GameStatusPlaying,
	GameStatusPaused,
	GameStatusLost
};
typedef enum GameStatus GameStatus;

typedef struct {
  GPoint block[4];
  GPoint next_block[4];
  uint8_t rotation;
  int8_t block_type;
  int8_t next_block_type;
  uint16_t lines_cleared;
  uint8_t level;
  uint32_t score;
} GameState;

void new_game();
void continue_game();