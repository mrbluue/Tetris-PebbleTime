/* Copyright (c) 2015, WRansohoff
 * MIT License, see LICENSE file for details.
 */
#include <pebble.h>

#include "helpers.h"

#define BLOCK_SIZE 8 
#define GRID_BLOCK_WIDTH 10
#define GRID_BLOCK_HEIGHT 20

#if defined(PBL_RECT)
  #define GRID_PIXEL_WIDTH (BLOCK_SIZE * GRID_BLOCK_WIDTH)
  #define GRID_PIXEL_HEIGHT (BLOCK_SIZE * GRID_BLOCK_HEIGHT)

  #define GRID_ORIGIN_X 0
  #define GRID_ORIGIN_Y 0
#endif

#if defined(PBL_PLATFORM_CHALK)
  #define SCREEN_WIDTH 180
  #define SCREEN_HEIGHT 180

  #define GRID_PIXEL_WIDTH (BLOCK_SIZE * GRID_BLOCK_WIDTH)
  #define GRID_PIXEL_HEIGHT (BLOCK_SIZE * GRID_BLOCK_HEIGHT)

  #define GRID_ORIGIN_X ((SCREEN_WIDTH - GRID_PIXEL_WIDTH) / 2)
  #define GRID_ORIGIN_Y ((SCREEN_HEIGHT - GRID_PIXEL_HEIGHT) / 2)
#endif

#define LABEL_HEIGHT 20 


// arbitrary persistant storage keys.
const uint32_t SCORE_KEY = 1234;
const uint32_t GRID_KEY = 5678;
const uint32_t GRID_COL_KEY = 90;
const uint32_t BLOCK_TYPE_KEY = 88888888;
const uint32_t NEXT_BLOCK_TYPE_KEY = 1820;
const uint32_t BLOCK_X_KEY = 2163;
const uint32_t BLOCK_Y_KEY = 1523;
const uint32_t ROTATION_KEY = 8278;
const uint32_t HAS_SAVE_KEY = 4510;
const uint32_t HIGH_SCORE_KEY = 3735928559;
const uint32_t OPTION_SHADOWS_KEY = 3535929778;

// Sorry some of the variable names are a little weird.
// I'm going to try to come back and clean this up a bit.
static Window *window;
static TextLayer *title_layer;
static TextLayer *new_game_label_layer;
static TextLayer *load_game_label_layer;
static TextLayer *score_label_layer;
static TextLayer *score_layer;
static TextLayer *level_label_layer;
static TextLayer *level_layer;
static TextLayer *paused_label_layer;
static TextLayer *option_shadows_layer;
static TextLayer *high_score_layer;
static GPath *selector_path;

static uint8_t grid[GRID_BLOCK_WIDTH][GRID_BLOCK_HEIGHT];
static uint8_t grid_col[GRID_BLOCK_WIDTH][GRID_BLOCK_HEIGHT];
static GColor col_map[7];
// Normally we can't rely on stack-allocated GPoints.
// But these do not need to retain their values for long,
// plus they never go out of scope.
static GPoint block[4];
static GPoint nextBlock[4];
static bool playing = false;
static bool paused = false;
static bool pauseFromFocus = false;
static bool lost = false;
static bool can_load = false;
static bool option_shadows_buffer;
static int load_choice = 0;
static int rotation = 0;
static int max_tick = 600;
static int tick_time;
static int tick_interval = 40;
static int blockType = -1;
static int nextBlockType = -1;
static int blockX = 5;
static int blockY = -1;
static int nextBlockX = 0;
static int nextBlockY = 0;
static int lines_cleared = 0;
static int level = 1;
static AppTimer *s_timer = NULL;

static char scoreStr[10];
static char levelStr[10];
static char high_score_buffer[10];
static char high_score_str[22];
static Layer *s_bg_layer = NULL;
static Layer *s_game_pane_layer = NULL;
static Layer *s_title_pane_layer = NULL;

static GColor blueish;
static GColor gray;
static GColor purple;

// ------------------------ //
// **** GAME FUNCTIONS **** //
// ------------------------ //

static void drop_block() {
  if (blockType == -1) {
    // Create a new block.
    blockType = nextBlockType;
    nextBlockType = rand() % 7;
    rotation = 0;
    int adjust = 0;
    if (blockType == Z || blockType == J) { adjust = -1; }
    if (blockType == LINE) { adjust = -2; }
    blockX = 5 + adjust;
    blockY = 0;
    nextBlockX = 0;
    nextBlockY = 0;
    make_block(block, blockType, blockX, blockY);
    make_block(nextBlock, nextBlockType, nextBlockX, nextBlockY);
  }
  else {
    // Handle the current block.
    // Drop it, and if it can't drop then fuck it.
    // Uh, I mean stick it in place and make a new block.
    bool canDrop = true;
    for (int i=0; i<4; i++) {
      int benthic = block[i].y + 1;
      if (benthic > 19) { canDrop = false; }
      if (grid[block[i].x][benthic]) { canDrop = false; }
    }

    if (canDrop) {
      for (int i=0; i<4; i++) {
        block[i].y += 1;
      }
    }
    else {
      for (int i=0; i<4; i++) {
        grid[block[i].x][block[i].y] = true;
        grid_col[block[i].x][block[i].y] = blockType;
      }
      layer_mark_dirty(s_bg_layer);
      blockType = -1;
      // Clear rows if possible.
      for (int j=0; j<20; j++) {
        bool isRow = true;
        for (int i=0; i<10; i++) {
          if (!grid[i][j]) { isRow = false; }
        }
        if (isRow) {
          // I hate all these nested loops...
          // But yeah, drop the above rows.
          lines_cleared += 1;
          if (level < 10 && lines_cleared >= (10 * level)) {
            level += 1;
            tick_time -= tick_interval;
            update_num_layer (level, levelStr, level_layer);
          }
          update_num_layer (lines_cleared, scoreStr, score_layer);
          for (int k=j; k>0; k--) {
            for (int i=0; i<10; i++) {
              grid[i][k] = grid[i][k-1];
              grid_col[i][k] = grid_col[i][k-1];
            }
          }
          for (int i=0; i<10; i++) {
            grid[i][0] = false;
            grid_col[i][0] = 255;
          }
        }
      }

      // Check whether you've lost.
      for (int i=0; i<4; i++) {
        if (block[i].y == 0) {
          playing = false;
          lost = true;
          Layer *window_layer = window_get_root_layer(window);
          layer_remove_from_parent(s_bg_layer);
          layer_remove_from_parent(s_game_pane_layer);
          layer_add_child(window_layer, text_layer_get_layer(title_layer));
          text_layer_set_text(title_layer, "You lost!");
          text_layer_set_text(score_label_layer, "");
          text_layer_set_text(score_layer, "");
          text_layer_set_text(level_label_layer, "");
          text_layer_set_text(level_layer, "");

          // When you lose, wipe the save.
          // No need to get fancy; just mark 'has save' false since
          // we'll re-create the whole thing when it gets set to true.
          persist_write_bool(HAS_SAVE_KEY, false);
          // High score?
          if (!persist_exists(HIGH_SCORE_KEY) ||
              lines_cleared > persist_read_int(HIGH_SCORE_KEY)) {
            persist_write_int(HIGH_SCORE_KEY, lines_cleared);
            layer_add_child(window_layer, text_layer_get_layer(high_score_layer));
            text_layer_set_text(high_score_layer, "New high score!");
          }
          return;
        }
      }
    }
  }

  layer_mark_dirty(s_game_pane_layer);
}

static void game_tick(void *data) {
  if (paused || !playing || lost) { return; }

  drop_block();
  s_timer = app_timer_register(tick_time, game_tick, NULL);
}

static void setup_game() {
    playing = true;
    Layer *window_layer = window_get_root_layer(window);
    layer_add_child(window_layer, s_bg_layer);
    layer_add_child(window_layer, s_game_pane_layer);
    layer_remove_from_parent(s_title_pane_layer);
    layer_remove_from_parent(text_layer_get_layer(new_game_label_layer));
    layer_remove_from_parent(text_layer_get_layer(load_game_label_layer));
    layer_remove_from_parent(text_layer_get_layer(high_score_layer));
    layer_remove_from_parent(text_layer_get_layer(option_shadows_layer));
    layer_add_child(window_layer, text_layer_get_layer(score_label_layer));
    layer_add_child(window_layer, text_layer_get_layer(score_layer));
    layer_add_child(window_layer, text_layer_get_layer(level_label_layer));
    layer_add_child(window_layer, text_layer_get_layer(level_layer));
    level = 0;
    lines_cleared = 0;
    text_layer_set_text(title_layer, "");
    text_layer_set_text(score_label_layer, "Score");
    update_num_layer(lines_cleared, scoreStr, score_layer);
    text_layer_set_text(level_label_layer, "Level");
    update_num_layer(level, levelStr, level_layer);
    tick_time = max_tick;
    rotation = 0;
    nextBlockType = rand() % 7;
    drop_block();
    layer_mark_dirty(s_bg_layer);
    layer_mark_dirty(s_game_pane_layer);
}

// TODO This should probably go in the helper file with the save logic.
// But it's late and I want to get saving/loading working.
static void load_game() {
  // We already know that we have valid data (can_load).
  lines_cleared = persist_read_int(SCORE_KEY);
  level = (lines_cleared / 10) + 1;
  if (level > 10) { level = 10; }
  update_num_layer(lines_cleared, scoreStr, score_layer);
  update_num_layer(level, levelStr, level_layer);
  tick_time = max_tick - (tick_interval * level);
  persist_read_data(GRID_KEY, grid, sizeof(grid));
  persist_read_data(GRID_COL_KEY, grid_col, sizeof(grid_col));
  blockType = persist_read_int(BLOCK_TYPE_KEY);
  nextBlockType = persist_read_int(NEXT_BLOCK_TYPE_KEY);
  blockX = persist_read_int(BLOCK_X_KEY);
  blockY = persist_read_int(BLOCK_Y_KEY);
  make_block(block, blockType, blockX, blockY);
  make_block(nextBlock, nextBlockType, nextBlockX, nextBlockY);
  rotation = persist_read_int(ROTATION_KEY);
  if (rotation != 0) {
    for (int i=0; i<rotation; i++) {
      GPoint rPoints[4];
      rotate_block(rPoints, block, blockType, i);
      for (int j=0; j<4; j++) {
        block[j] = rPoints[j];
      }
    }
  }
}

static void restart_after_loss() {
  lost = false;
  can_load = false;
  s_timer = NULL;
  for (int i=0; i<10; i++) {
    for (int j=0; j<20; j++) {
      grid[i][j] = false;
      grid_col[i][j] = 255;
    }
  }
  blockType = -1;
  nextBlockType = -1;
  load_choice = 0;
  Layer *window_layer = window_get_root_layer(window);
  layer_add_child(window_layer, text_layer_get_layer(title_layer));
  text_layer_set_text(title_layer, "Tebbletris"); // such a dumb name that's crazy, but a victory for not being eligible for a cease & desist
  layer_add_child(window_layer, text_layer_get_layer(new_game_label_layer));
  layer_add_child(window_layer, text_layer_get_layer(high_score_layer));
  layer_add_child(window_layer, text_layer_get_layer(option_shadows_layer));
  layer_add_child(window_layer, s_title_pane_layer);
  if (persist_exists(HIGH_SCORE_KEY)) {
    itoa10(persist_read_int(HIGH_SCORE_KEY), high_score_buffer);
  }
  else {
    itoa10(0, high_score_buffer);
  }
  // Fool strcat into thinking the string is empty.
  high_score_str[0] = '\0';
  strcat(high_score_str, "High score: ");
  text_layer_set_text(high_score_layer, strcat(high_score_str, high_score_buffer));
  layer_mark_dirty(s_title_pane_layer);
}

// ------------------------- //
// **** BUTTON HANDLERS **** //
// ------------------------- //

static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (!playing && lost) {
    restart_after_loss();
    return;
  }

  if (!playing && !lost && (load_choice < 2)) {
    setup_game();
    if (can_load && (load_choice == 1)) {
      load_game();
    }
    if (!s_timer) {
      drop_block();
      s_timer = app_timer_register(tick_time, game_tick, NULL);
    }
    return;
  }

  if (!playing && (load_choice == 2)) { // kamotswind - had to modify several things in here to add new option
    option_shadows_buffer = !option_shadows_buffer;
    persist_write_bool(OPTION_SHADOWS_KEY, option_shadows_buffer);
    if(option_shadows_buffer) {
      text_layer_set_text(option_shadows_layer, "Drop Shadows ON");
    }
    else {
      text_layer_set_text(option_shadows_layer, "Drop Shadows OFF");
    }
    return;
  }
  
  if (paused) {
    paused = false;
    s_timer = app_timer_register(tick_time, game_tick, NULL);
    layer_remove_from_parent(text_layer_get_layer(paused_label_layer));
    return;
  }

  if (blockType == -1) { return; }
  GPoint newPos[4];
  rotate_block(newPos, block, blockType, rotation);

  bool should_rotate = true;
  for (int i=0; i<4; i++) {
    if (newPos[i].x < 0 || newPos[i].x > 9) { should_rotate = false; }
    if (newPos[i].y < 0 || newPos[i].y > 19) { should_rotate = false; }
    if (grid[newPos[i].x][newPos[i].y]) { should_rotate = false; }
  }
  if (!should_rotate) { return; }

  for (int i=0; i<4; i++) {
    block[i] = newPos[i];
  }
  rotation = (rotation + 1) % 4;
  layer_mark_dirty(s_game_pane_layer);
}

static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (!playing && !lost) {
    if(load_choice == 1) load_choice = 0;
    if((load_choice == 2) && (can_load)) load_choice = 1;
    else load_choice = 0;
    layer_mark_dirty(s_title_pane_layer);
  }
  if (!playing || paused) { return; }

  // Move the block left, if possible.
  bool canMove = true;
  for (int i=0; i<4; i++) {
    int toDaIzquierda = block[i].x - 1;
    if (toDaIzquierda < 0) { canMove = false; }
    if (grid[toDaIzquierda][block[i].y]) { canMove = false; }
  }

  if (!canMove) { return; }
  for (int i=0; i<4; i++) {
    block[i].x -= 1;
  }

  layer_mark_dirty(s_game_pane_layer);
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (!playing && !lost) {
    if(load_choice == 1) load_choice = 2;
    if((load_choice == 0) && (can_load)) load_choice = 1;
    else load_choice = 2;
    layer_mark_dirty(s_title_pane_layer);
  }
  if (!playing || paused) { return; }

  // Move the block right, if possible.
  bool canMove = true;
  for (int i=0; i<4; i++) {
    int toDaRight = block[i].x + 1;
    if (toDaRight > 9) { canMove = false; }
    if (grid[toDaRight][block[i].y]) { canMove = false; }
  }

  if (!canMove) { return; }
  for (int i=0; i<4; i++) {
    block[i].x += 1;
  }

  layer_mark_dirty(s_game_pane_layer);
}

static void select_long_click_handler(ClickRecognizerRef recognizer, void *context) {
  int drop_amount = find_max_drop(block, grid);
  for (int i=0; i<4; i++) {
    block[i].y += drop_amount;
  }
}

static void back_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (!playing && lost) {
    restart_after_loss();
    return;
  }
  if (!playing || paused) { window_stack_pop(true); }
  paused = true;
  Layer *window_layer = window_get_root_layer(window);
  layer_add_child(window_layer, text_layer_get_layer(paused_label_layer));
}

static void up_long_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (!playing || paused) { return; }
  int moveAmt = find_max_horiz_move(block, grid, LEFT);
  for (int i=0; i<4; i++) {
    block[i].x -= moveAmt;
  }
}

static void down_long_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (!playing || paused) { return; }
  int moveAmt = find_max_horiz_move(block, grid, RIGHT);
  for (int i=0; i<4; i++) {
    block[i].x += moveAmt;
  }
}

static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
  window_single_click_subscribe(BUTTON_ID_BACK, back_click_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
  window_long_click_subscribe(BUTTON_ID_SELECT, 500, select_long_click_handler, NULL);

  // Deactivated Up long click and Down long click (didn't like it)
  // window_long_click_subscribe(BUTTON_ID_UP, 250, up_long_click_handler, NULL);
  // window_long_click_subscribe(BUTTON_ID_DOWN, 250, down_long_click_handler, NULL);

}

// ------------------------ //
// **** DRAW FUNCTIONS **** //
// ------------------------ //

// Draw blocks and block shadow in game area
static void draw_game(Layer *layer, GContext *ctx) {
  if (!playing || blockType == -1) { return; }

  // Fast-drop is instant, so we need to show a guide.
  if(option_shadows_buffer) { // kamotswind - don't display the guide if option is false ("OFF")
    graphics_context_set_fill_color(ctx, gray);
    int max_drop = find_max_drop (block, grid);
      for (int i=0; i<4; i++) {
      GRect brick_ghost = GRect(block[i].x*8, (block[i].y + max_drop)*8, 8, 8);
      graphics_fill_rect(ctx, brick_ghost, 0, GCornerNone);
    }
  }
  // Draw the actual block.
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_context_set_fill_color(ctx, col_map[blockType]);
  for (int i=0; i<4; i++) {
    GRect brick_block = GRect(block[i].x*8, block[i].y*8, 8, 8);
    graphics_fill_rect(ctx, brick_block, 0, GCornerNone);
    graphics_draw_rect(ctx, brick_block);
  }
}

static void draw_bg(Layer *layer, GContext *ctx) {
  if (!playing) { return; }

  GRect bounds = layer_get_bounds(layer);
  int16_t boundsWidth = bounds.size.w;
  int16_t boundsHeight = bounds.size.h;

  // Color background
  graphics_context_set_fill_color(ctx, blueish);

  GRect color_bg = GRect(0, 0, boundsWidth, boundsHeight);
  graphics_fill_rect(ctx, color_bg, 0, GCornerNone);

  // Attempt at adding a Radial instead of rectangular labels to fit the Time Round aesthetic but meh, we'll see.
  // TODO - Keeping this as a note for later

  // #if defined(PBL_ROUND)
  // graphics_context_set_fill_color(ctx, blueishish);
  // graphics_fill_radial(ctx, bounds, GOvalScaleModeFitCircle, GRID_ORIGIN_X, DEG_TO_TRIGANGLE(45), DEG_TO_TRIGANGLE(135)); // GRID_ORIGIN_X for Radius
  // graphics_fill_radial(ctx, bounds, GOvalScaleModeFitCircle, GRID_ORIGIN_X, DEG_TO_TRIGANGLE(235), DEG_TO_TRIGANGLE(305)); // 225-10 / 315-10 
  // #endif

  // Game BG.
  graphics_context_set_fill_color(ctx, GColorBlack);
  GRect game_bg = GRect(GRID_ORIGIN_X, GRID_ORIGIN_Y, GRID_PIXEL_WIDTH, GRID_PIXEL_HEIGHT);
  graphics_fill_rect(ctx, game_bg, 0, GCornerNone);

  // Grid colors from leftover blocks.
  for (int i=0; i<GRID_BLOCK_WIDTH; i++) {
    for (int j=0; j<GRID_BLOCK_HEIGHT; j++) {
      if (grid_col[i][j] != 255) {
        graphics_context_set_fill_color(ctx, col_map[grid_col[i][j]]);
        GRect brick = GRect((i*BLOCK_SIZE) + GRID_ORIGIN_X, (j*BLOCK_SIZE) + GRID_ORIGIN_Y, BLOCK_SIZE, BLOCK_SIZE);
        graphics_fill_rect(ctx, brick, 0, GCornerNone);
      }
    }
  }

  // Display preview of upcoming block
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_context_set_fill_color(ctx, col_map[nextBlockType]);

  #if defined(PBL_ROUND)
  int sPosX = (GRID_ORIGIN_X+GRID_PIXEL_WIDTH+BLOCK_SIZE*2) + next_block_offset(nextBlockType);
  int sPosY = GRID_ORIGIN_Y + BLOCK_SIZE*5;
  #else
  // for rectangular screen, center the next block display in the right pane
  int sPosX = (GRID_ORIGIN_X+GRID_PIXEL_WIDTH+((boundsWidth-GRID_PIXEL_WIDTH)/2)) + next_block_offset(nextBlockType);
  int sPosY = GRID_ORIGIN_Y + BLOCK_SIZE*2;
  #endif

  for (int i=0; i<4; i++) {
    GRect bl = GRect(sPosX+(nextBlock[i].x*8), sPosY+(nextBlock[i].y*8), 8, 8);
    graphics_fill_rect(ctx, bl, 0, GCornerNone);
    graphics_draw_rect(ctx, bl);
  }

  // Game BG grid.
  graphics_context_set_stroke_color(ctx, purple);
  // Draw vertical lines
  for (int i=GRID_ORIGIN_X; i<(GRID_PIXEL_WIDTH+GRID_ORIGIN_X); i+=BLOCK_SIZE) {
    graphics_draw_line(ctx, GPoint(i, GRID_ORIGIN_Y), GPoint(i, GRID_ORIGIN_Y + GRID_PIXEL_HEIGHT)); 
  }
  // Draw horizontal lines
  for (int i=GRID_ORIGIN_Y; i<(GRID_PIXEL_HEIGHT+GRID_ORIGIN_Y); i+=BLOCK_SIZE) {
    graphics_draw_line(ctx, GPoint(GRID_ORIGIN_X, i), GPoint(GRID_ORIGIN_X+GRID_PIXEL_WIDTH, i));
  }
}

static void draw_title_pane(Layer *layer, GContext *ctx) {
  graphics_context_set_fill_color(ctx, GColorBlack);
  GPoint selector[3];
  int xOff = 20; // kamotswind - Needed for better arrow alignment
  int yOff = load_choice * 20;
  if(load_choice == 1) xOff = 24;
  if(load_choice == 2) xOff = 0;
  selector[0] = GPoint(xOff + 6, 61 + yOff);
  selector[1] = GPoint(xOff + 6, 69 + yOff);
  selector[2] = GPoint(xOff + 16, 65 + yOff);
  GPathInfo selector_path_info = { 3, selector };
  selector_path = gpath_create(&selector_path_info);
  gpath_draw_filled(ctx, selector_path);
}

static void app_focus_handler(bool focus) {
  if (!focus) {
    pauseFromFocus = true;
    paused = true;
    Layer *window_layer = window_get_root_layer(window);
    layer_add_child(window_layer, text_layer_get_layer(paused_label_layer));
  }
  else {
    if (pauseFromFocus) {
      paused = false;
      s_timer = app_timer_register(tick_time, game_tick, NULL);
      layer_remove_from_parent(text_layer_get_layer(paused_label_layer));
    }
    pauseFromFocus = false;
  }
}

// -------------------------- //
// **** WINDOW FUNCTIONS **** //
// -------------------------- //

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  int16_t boundsWidth = bounds.size.w;
  int16_t boundsHeight = bounds.size.h;

  // ** TITLE WINDOW LAYERS ** //

  title_layer = text_layer_create((GRect) { .origin = { 0, 16 }, .size = { boundsWidth, 20 } });
  text_layer_set_text(title_layer, "Tetris");
  text_layer_set_text_alignment(title_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(title_layer));

  new_game_label_layer = text_layer_create((GRect) { .origin = { 0, 56 }, .size = { boundsWidth, 20 } });
  text_layer_set_text(new_game_label_layer, "New Game");
  text_layer_set_text_alignment(new_game_label_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(new_game_label_layer));

  load_game_label_layer = text_layer_create((GRect) { .origin = { 0, 76 }, .size = { boundsWidth, 20 } });
  text_layer_set_text(load_game_label_layer, "Continue");
  text_layer_set_text_alignment(load_game_label_layer, GTextAlignmentCenter);
  if (can_load) {
    layer_add_child(window_layer, text_layer_get_layer(load_game_label_layer));
  }
  
  // kamotswind - Add option to toggle drop shadow. Had to move all things around on the screen.
  option_shadows_layer = text_layer_create((GRect) { .origin = { 0, 96 }, .size = { boundsWidth, 20 } });
  if (persist_exists(OPTION_SHADOWS_KEY)) {
    option_shadows_buffer = persist_read_bool(OPTION_SHADOWS_KEY);
  }
  else {
    option_shadows_buffer = true;
  }
  if(option_shadows_buffer) {
    text_layer_set_text(option_shadows_layer, "Drop Shadows ON");
  }
  else {
    text_layer_set_text(option_shadows_layer, "Drop Shadows OFF");
  }
  text_layer_set_text_alignment(option_shadows_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(option_shadows_layer));
  
  high_score_layer = text_layer_create((GRect) { .origin = { 0, boundsHeight*0.75 }, .size = { boundsWidth, 20 } });

  if (persist_exists(HIGH_SCORE_KEY)) {
    itoa10(persist_read_int(HIGH_SCORE_KEY), high_score_buffer);
  }
  else {
    itoa10(0, high_score_buffer);
  }
  high_score_str[0] = '\0';
  strcat(high_score_str, "High score: ");
  text_layer_set_text(high_score_layer, strcat(high_score_str, high_score_buffer));
  text_layer_set_text_alignment(high_score_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(high_score_layer));


  // *** PLAY WINDOW LAYERS ***

  s_bg_layer = layer_create(GRect(0, 0, boundsWidth, boundsHeight));
  layer_set_update_proc(s_bg_layer, draw_bg);

  s_game_pane_layer = layer_create(GRect(GRID_ORIGIN_X, GRID_ORIGIN_Y, GRID_ORIGIN_X+GRID_PIXEL_WIDTH, GRID_ORIGIN_Y+GRID_PIXEL_HEIGHT));
  layer_set_update_proc(s_game_pane_layer, draw_game);

  s_title_pane_layer = layer_create(GRect(0, 0, boundsWidth, boundsHeight));
  layer_set_update_proc(s_title_pane_layer, draw_title_pane);
  layer_add_child(window_layer, s_title_pane_layer);

  score_label_layer = text_layer_create((GRect) { 
    .origin = { 
      GRID_ORIGIN_X+GRID_PIXEL_WIDTH, 
      (GRID_ORIGIN_Y + BLOCK_SIZE * PBL_IF_RECT_ELSE(6, 9)) // place layer 6 or 9 blocks from top vertical coordinate of play area
    }, 
    .size = { 
      boundsWidth-(GRID_ORIGIN_X+GRID_PIXEL_WIDTH), // as wide as the space to the right of the play area
      LABEL_HEIGHT 
    } 
  });
  text_layer_set_text_alignment(score_label_layer, GTextAlignmentCenter);

  score_layer = text_layer_create((GRect) { 
    .origin = { 
      GRID_ORIGIN_X + GRID_PIXEL_WIDTH, 
      (GRID_ORIGIN_Y + BLOCK_SIZE * PBL_IF_RECT_ELSE(7, 10) + LABEL_HEIGHT) // 7 or 10 blocks from top, below previous label
    }, 
    .size = { 
      boundsWidth - (GRID_ORIGIN_X + GRID_PIXEL_WIDTH), 
      LABEL_HEIGHT
    } 
  });
  text_layer_set_text_alignment(score_layer, GTextAlignmentCenter);

  level_label_layer = text_layer_create((GRect) { 
    .origin = { 
      PBL_IF_RECT_ELSE(GRID_ORIGIN_X + GRID_PIXEL_WIDTH, 0), // align to the left of play area on Round screen
      PBL_IF_RECT_ELSE(GRID_ORIGIN_Y + BLOCK_SIZE * 8 + LABEL_HEIGHT * 2, GRID_ORIGIN_Y + BLOCK_SIZE * 7)
    }, 
    .size = { 
      boundsWidth-(GRID_ORIGIN_X + GRID_PIXEL_WIDTH), 
      LABEL_HEIGHT 
    } 
  });
  text_layer_set_text_alignment(level_label_layer, GTextAlignmentCenter);
  
  level_layer = text_layer_create((GRect) { 
    .origin = { 
      PBL_IF_RECT_ELSE(GRID_ORIGIN_X + GRID_PIXEL_WIDTH, 0), 
      PBL_IF_RECT_ELSE(GRID_ORIGIN_Y + BLOCK_SIZE * 9 + LABEL_HEIGHT * 3, GRID_ORIGIN_Y + BLOCK_SIZE * 8 + LABEL_HEIGHT) 
    }, 
    .size = { 
      boundsWidth - (GRID_ORIGIN_X + GRID_PIXEL_WIDTH), 
      LABEL_HEIGHT 
    } 
  });
  text_layer_set_text_alignment(level_layer, GTextAlignmentCenter);

  paused_label_layer = text_layer_create((GRect) { 
    .origin = { 
      GRID_ORIGIN_X, 
      GRID_ORIGIN_Y + (GRID_PIXEL_HEIGHT/2) - LABEL_HEIGHT // place in middle of play area
    }, 
    .size = { 
      GRID_PIXEL_WIDTH, 
      LABEL_HEIGHT 
    } 
  });
  text_layer_set_text(paused_label_layer, "Paused");
  text_layer_set_text_alignment(paused_label_layer, GTextAlignmentCenter);

  app_focus_service_subscribe(app_focus_handler);
}

static void window_unload(Window *window) {
  app_focus_service_unsubscribe();
  // TODO / REMINDER - Add any new objects here for destruction on exit!
  text_layer_destroy(title_layer);
  text_layer_destroy(new_game_label_layer);
  text_layer_destroy(load_game_label_layer);
  text_layer_destroy(score_label_layer);
  text_layer_destroy(score_layer);
  text_layer_destroy(level_label_layer);
  text_layer_destroy(level_layer);
  text_layer_destroy(paused_label_layer);
  text_layer_destroy(option_shadows_layer);
  text_layer_destroy(high_score_layer);
  layer_destroy(s_bg_layer);
  layer_destroy(s_game_pane_layer);
  layer_destroy(s_title_pane_layer);
  gpath_destroy(selector_path);
}

static void init(void) {
  // Does valid save data exist?
  if (persist_exists(HAS_SAVE_KEY)) {
    if (persist_read_bool(HAS_SAVE_KEY) &&
        persist_exists(SCORE_KEY) && persist_exists(GRID_KEY) &&
        persist_exists(GRID_COL_KEY) && persist_exists(BLOCK_TYPE_KEY) &&
        persist_exists(NEXT_BLOCK_TYPE_KEY) && persist_exists(BLOCK_X_KEY) &&
        persist_exists(BLOCK_Y_KEY) && persist_exists(ROTATION_KEY)) {
      can_load = true;
    }
  }

  window = window_create();
  window_set_click_config_provider(window, click_config_provider);
  window_set_window_handlers(window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  const bool animated = true;
  window_stack_push(window, animated);

  blueish = GColorFromRGB(0, 0, 96);
  gray = GColorFromRGB(96, 96, 96);
  purple = GColorFromRGB(64, 0, 64);

  col_map[RED] = GColorFromRGB(196, 0, 0);
  col_map[GREEN] = GColorFromRGB(0, 196, 0);
  col_map[BLUE] = GColorFromRGB(0, 0, 196);
  col_map[YELLOW] = GColorFromRGB(196, 196, 0);
  col_map[PURPLE] = GColorFromRGB(196, 0, 196);
  col_map[CYAN] = GColorFromRGB(0, 196, 196);
  col_map[WHITE] = GColorFromRGB(196, 196, 196);

  for (int i=0; i<10; i++) {
    for (int j=0; j<20; j++) {
      grid[i][j] = false;
      grid_col[i][j] = 255;
    }
  }
}

static void deinit(void) {
  // Save if the player is playing.
  if (playing) {
    persist_write_int(SCORE_KEY, lines_cleared);
    persist_write_data(GRID_KEY, grid, sizeof(grid));
    persist_write_data(GRID_COL_KEY, grid_col, sizeof(grid_col));
    persist_write_int(BLOCK_TYPE_KEY, blockType);
    persist_write_int(NEXT_BLOCK_TYPE_KEY, nextBlockType);
    persist_write_int(ROTATION_KEY, rotation);
    // Normalize the block position for saving. We'll replay the rotation on load.
    while (rotation != 0) {
      GPoint new_points[4];
      rotate_block(new_points, block, blockType, rotation);
      for (int i=0; i<4; i++) {
        block[i] = new_points[i];
      }
      rotation = (rotation+1)%4;
    }
    persist_write_int(BLOCK_X_KEY, block[0].x);
    persist_write_int(BLOCK_Y_KEY, block[0].y);
    persist_write_bool(HAS_SAVE_KEY, true);
  }
  else if (lost) {
    persist_write_bool(HAS_SAVE_KEY, false);
  }

  window_destroy(window);
}

int main(void) {
  init();

  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p", window);

  app_event_loop();
  deinit();
}
