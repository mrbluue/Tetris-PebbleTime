#ifndef HELPERS_H
#define HELPERS_H

#include <pebble.h>

#define SQUARE 0
#define LINE   1
#define J      2
#define L      3
#define S      4
#define Z      5
#define T      6

#define BLOCK_SIZE 8 
#define GRID_BLOCK_WIDTH 10
#define GRID_BLOCK_HEIGHT 20

#define GAME_STATE_KEY         737415
#define GAME_GRID_BLOCK_KEY    737415810
#define GAME_GRID_COLOR_KEY    737415601
#define GAME_CONTINUE_KEY      7374150
#define GAME_SETTINGS_KEY      737415537
#define GAME_SCORES_KEY        737415560
#define GAME_NAME_KEY          737415561

#ifdef PBL_PLATFORM_CHALK
  #define SCREEN_WIDTH 180
  #define SCREEN_HEIGHT 180
#else
  #define SCREEN_WIDTH 144
  #define SCREEN_HEIGHT 168
#endif

#define THEMES_COUNT 4
#define THEMES_BYTES 18

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
  uint8_t block_X;
  uint8_t block_Y;
  // uint8_t next_block_X;
  // uint8_t next_block_Y;
  uint16_t lines_cleared;
  uint8_t level;
  uint32_t score;
} GameState;

typedef struct {
  char name[4];
  uint32_t score;
  // uint8_t level; // TODO
  // char date[8];  // TODO
} GameScore;

typedef struct {
  bool set_drop_shadow;
  bool set_counterclockwise;
  bool set_backlight;
  uint8_t set_theme;
} GameSettings;

typedef struct {
  GColor window_bg_color;
  GColor window_header_color;
  GColor window_label_text_color;
  GColor window_label_bg_color;
  GColor window_label_bg_inactive_color;
  GColor grid_bg_color;
  GColor grid_lines_color;
  GColor block_color[7];
  GColor block_border_color;
  GColor drop_shadow_color;
  GColor select_color;
  GColor score_accent_color;
} Theme;

extern GameSettings game_settings;
extern Theme theme;

extern GFont s_font_title;
extern GFont s_font_mono_small;
extern GFont s_font_mono_big;

char *itoa10 (int value, char *result);

void update_num_layer (int num, char *str, TextLayer *layer);

void make_block (GPoint *create_block, int type, int bX, int bY);

void rotate_block (GPoint *new_block, GPoint *old_block, int block_type, int rotation);

int find_max_drop (GPoint *block, uint8_t grid[GRID_BLOCK_WIDTH][GRID_BLOCK_HEIGHT]);

int find_max_horiz_move (GPoint *block, uint8_t grid[GRID_BLOCK_WIDTH][GRID_BLOCK_HEIGHT], bool direction);

int next_block_offset (int block_type);

void set_theme(int theme);

#endif
