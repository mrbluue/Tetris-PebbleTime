#ifndef HELPERS_H
#define HELPERS_H

#include <pebble.h>


#define LEFT -1
#define RIGHT 1

#define GAME_GRID_BLOCK_WIDTH 10
#define GAME_GRID_BLOCK_HEIGHT 20

#define GAME_STATE_KEY         737415
#define GAME_GRID_BLOCK_KEY    737415810
#define GAME_GRID_COLOR_KEY    737415601
#define GAME_CONTINUE_KEY      7374150
#define GAME_SETTINGS_KEY      737415537
#define GAME_SCORES_KEY        737415560
#define GAME_NAME_KEY          737415561

#ifdef PBL_PLATFORM_EMERY
  #define BLOCK_SIZE 11
#elif defined (PBL_PLATFORM_GABBRO)
  #define BLOCK_SIZE 12
#else
  #define BLOCK_SIZE 8 
#endif

#define THEMES_COUNT 4
#define THEMES_BYTES 18

typedef enum {
   NONE = -1, 
   O, I, J, L, S, Z, T, 
   BLOCK_TYPES
} Tetromino;

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
  #ifdef PBL_COLOR
  GColor grid_lines_color;
  GColor block_color[BLOCK_TYPES];
  GColor block_border_color;
  GColor drop_shadow_color;
  GColor select_color;
  GColor score_accent_color;
  #endif
} Theme;

extern GameSettings game_settings;
extern Theme theme;

extern GFont s_font_mono_line;
extern GFont s_font_mono;
extern GFont s_font_mono_big;
extern GFont s_font_menu;

void update_string_num_layer (char* str_in, int num, char* str_out, size_t out_size, TextLayer *layer);

void make_block (GPoint *create_block, int type, int bX, int bY);

void rotate_mino(GPoint *new_block, GPoint *old_block, int block_type, int rotation);

bool rotate_try_kicks(GPoint *new_block, GPoint *old_block, int block_type, int old_rotation, int new_rotation, bool grid[GAME_GRID_BLOCK_WIDTH][GAME_GRID_BLOCK_HEIGHT]);

int find_max_drop (GPoint *block, bool grid[GAME_GRID_BLOCK_WIDTH][GAME_GRID_BLOCK_HEIGHT]);

int next_block_offset (int block_type);

void set_theme(int theme);

#endif
