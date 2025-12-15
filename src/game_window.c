#include <pebble.h>

#include "helpers.h"
#include "game_window.h"
#include "score_window.h"

static Window *s_window;

static GameState s_game_state;
static GameStatus s_status;
static bool s_grid_blocks[GAME_GRID_BLOCK_WIDTH][GAME_GRID_BLOCK_HEIGHT];
static uint8_t s_grid_colors[GAME_GRID_BLOCK_WIDTH][GAME_GRID_BLOCK_HEIGHT];

static bool s_pauseFromFocus = false;

static int s_lines_cleared_at_once = 0;

static AppTimer *s_game_timer = NULL;
static int s_max_tick = 600;
static int s_tick_time;
static int s_tick_interval = 40;

static AppTimer *s_longpress_timer = NULL;
static int s_longpress_tick = 200;
static int s_longpress_movement_direction;

static AppTimer *s_lockdelay_timer = NULL;
static int s_lockdelay_tick = 500;
static int s_lockdelay_maxmoves = 15;

static AppTimer *s_flash_timer = NULL;
static int s_flash_tick = 100;
static int s_flash_amount = 5;
static GColor s_og_bg_color;
static GColor s_flash_bg_color;

static char s_score_str[18];
static char s_level_str[10];
static char s_lines_str[12];

static Layer     *s_bg_layer = NULL;
static Layer     *s_game_pane_layer = NULL;
static TextLayer *s_score_layer;
static TextLayer *s_level_layer;
static TextLayer *s_lines_layer;
static TextLayer *s_paused_label_layer;

static void prv_game_window_push();

static void prv_window_load(Window *window);
static void prv_window_unload(Window *window);
static void prv_app_focus_handler(bool focus);

static void prv_click_config_provider(void *context);

static void prv_draw_game(Layer *layer, GContext *ctx);
static void prv_draw_bg(Layer *layer, GContext *ctx);

static void prv_game_cycle();
static bool prv_can_drop();
static void prv_game_move_piece(int movement);
static void prv_game_rotate_piece();
static void prv_reset_lock_delay();
static void prv_lock_piece();
static void prv_clear_rows();
static void prv_check_if_lost();

static void prv_game_tick(void *data);
static void prv_s_longpress_tick(void *data);
static void prv_lockdelay_tick(void *data);
static void prv_flash_tick(void *data);

static void prv_setup_game();
static void prv_load_game();
static void prv_quit_after_loss();
static void prv_save_game();

// -------------------------- //
// **** WINDOW FUNCTIONS **** //
// -------------------------- //

void new_game(){
  APP_LOG(APP_LOG_LEVEL_INFO, "NEW GAME");
  prv_game_window_push();
  prv_setup_game();
  prv_game_cycle();
  if (!s_game_timer) {
    s_game_timer = app_timer_register(s_tick_time, prv_game_tick, NULL);
  }
}

void continue_game(){
  APP_LOG(APP_LOG_LEVEL_INFO, "CONTINUE GAME!");
  prv_game_window_push();
  prv_setup_game();
  prv_load_game();
  prv_game_cycle();
  if (!s_game_timer) {
    s_game_timer = app_timer_register(s_tick_time, prv_game_tick, NULL);
  }  
}

static void prv_game_window_push() {
  s_og_bg_color = theme.grid_bg_color;
  if(s_og_bg_color.argb == GColorWhite.argb)
    s_flash_bg_color = GColorBlack;
  else 
    s_flash_bg_color = GColorWhite;

	s_window = window_create();
	window_set_window_handlers(s_window, (WindowHandlers) {
		.load = prv_window_load,
		.unload = prv_window_unload,
	});
  window_set_click_config_provider(s_window, prv_click_config_provider);
  const bool animated = true;
	window_stack_push(s_window, animated);
}

static void prv_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  int16_t bounds_width = bounds.size.w;
  int16_t bounds_height = bounds.size.h;

  APP_LOG(APP_LOG_LEVEL_INFO, "GAME WINDOW LOAD");

  s_bg_layer = layer_create(GRect(0, 0, bounds_width, bounds_height));
  layer_set_update_proc(s_bg_layer, prv_draw_bg);
  layer_add_child(window_layer, s_bg_layer);

  s_game_pane_layer = layer_create(GRect(GRID_ORIGIN_X, GRID_ORIGIN_Y, GRID_ORIGIN_X+GRID_PIXEL_WIDTH, GRID_ORIGIN_Y+GRID_PIXEL_HEIGHT));
  layer_set_update_proc(s_game_pane_layer, prv_draw_game);
  layer_add_child(window_layer, s_game_pane_layer);

  s_score_layer = text_layer_create(GRect(LABEL_SCORE_X, LABEL_SCORE_Y, LABEL_WIDTH, BLOCK_SIZE * 4));
  text_layer_set_font(s_score_layer, s_font_mono_tall);
  text_layer_set_text_alignment(s_score_layer, GTextAlignmentCenter);
  text_layer_set_background_color(s_score_layer, theme.window_label_bg_color);
  text_layer_set_text_color(s_score_layer, theme.window_label_text_color);
  layer_add_child(window_layer, text_layer_get_layer(s_score_layer));

  s_level_layer = text_layer_create(GRect(LABEL_LEVEL_X, LABEL_LEVEL_Y, LABEL_WIDTH, LABEL_HEIGHT));
  text_layer_set_font(s_level_layer, s_font_mono_tall);
  text_layer_set_text_alignment(s_level_layer, GTextAlignmentCenter);
  text_layer_set_background_color(s_level_layer, theme.window_label_bg_color);
  text_layer_set_text_color(s_level_layer, theme.window_label_text_color);
  layer_add_child(window_layer, text_layer_get_layer(s_level_layer));

  s_lines_layer = text_layer_create(GRect(LABEL_LINES_X, LABEL_LINES_Y, LABEL_WIDTH, BLOCK_SIZE * 4));
  text_layer_set_font(s_lines_layer, s_font_mono_tall);
  text_layer_set_text_alignment(s_lines_layer, GTextAlignmentCenter);
  text_layer_set_background_color(s_lines_layer, theme.window_label_bg_color);
  text_layer_set_text_color(s_lines_layer, theme.window_label_text_color);
  layer_add_child(window_layer, text_layer_get_layer(s_lines_layer));

  s_paused_label_layer = text_layer_create(GRect(GRID_ORIGIN_X+1, LABEL_PAUSE_Y, GRID_PIXEL_WIDTH-1, LABEL_PAUSE_H));
  text_layer_set_text(s_paused_label_layer, "PAUSED");
  text_layer_set_font(s_paused_label_layer, s_font_mono_tall);
  text_layer_set_background_color(s_paused_label_layer, theme.window_label_bg_inactive_color);
  text_layer_set_text_color(s_paused_label_layer, theme.window_label_text_color);
  text_layer_set_text_alignment(s_paused_label_layer, GTextAlignmentCenter);

  app_focus_service_subscribe(prv_app_focus_handler);
}

static void prv_window_unload(Window *window) {
  app_focus_service_unsubscribe();
  
  s_game_timer      = NULL;
  s_longpress_timer = NULL;
  s_lockdelay_timer = NULL;
  s_flash_timer     = NULL;

  theme.grid_bg_color = s_og_bg_color;

  text_layer_destroy(s_score_layer);
  text_layer_destroy(s_level_layer);
  text_layer_destroy(s_lines_layer);
  text_layer_destroy(s_paused_label_layer);
  layer_destroy(s_bg_layer);
  layer_destroy(s_game_pane_layer);
}

static void prv_app_focus_handler(bool focus) {
  if (!focus) {
    s_pauseFromFocus = true;
    s_status = GameStatusPaused;
    Layer *window_layer = window_get_root_layer(s_window);
    layer_add_child(window_layer, text_layer_get_layer(s_paused_label_layer));
  }
  else {
    if (s_pauseFromFocus) {
      s_status = GameStatusPlaying;
      s_game_timer = app_timer_register(s_tick_time, prv_game_tick, NULL);
      layer_remove_from_parent(text_layer_get_layer(s_paused_label_layer));
    }
    s_pauseFromFocus = false;
  }
}

// ------------------------- //
// **** BUTTON HANDLERS **** //
// ------------------------- //

static void prv_select_click_handler(ClickRecognizerRef recognizer, void *context) {
  // If game is lost, restart a new one
  if (s_status == GameStatusLost) {
    prv_quit_after_loss();
    return;
  }
  
  // Unpause the game if it's paused
  if (s_status == GameStatusPaused) {
    s_status = GameStatusPlaying;
    s_game_timer = app_timer_register(s_tick_time, prv_game_tick, NULL);
    layer_remove_from_parent(text_layer_get_layer(s_paused_label_layer));
    return;
  }

  prv_game_rotate_piece();

}

static void prv_up_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (s_status != GameStatusPlaying) { return; }
  prv_game_move_piece(LEFT);
}

static void prv_down_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (s_status != GameStatusPlaying) { return; }
  prv_game_move_piece(RIGHT);
}

static void prv_back_click_handler(ClickRecognizerRef recognizer, void *context) {
  switch(s_status){
    case GameStatusLost:
      prv_quit_after_loss();
      return;
      break;
    case GameStatusPaused:
      prv_save_game();
      window_stack_pop(true);
      return;
      break;
    default:
      s_status = GameStatusPaused;
      Layer *window_layer = window_get_root_layer(s_window);
      layer_add_child(window_layer, text_layer_get_layer(s_paused_label_layer));
    break;
  }
}

static void prv_select_long_click_handler(ClickRecognizerRef recognizer, void *context) {
  if(!s_lockdelay_timer) { // check that we're not already locking the piece, or else conflict occurs (no grid color bug)
    int drop_amount = find_max_drop(s_game_state.block, s_grid_blocks);
    for (int i=0; i<4; i++) {
      s_game_state.block[i].y += drop_amount;
    }
    prv_lock_piece(); 
  }
}

static void prv_down_long_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (s_status != GameStatusPlaying) { return; }
  prv_game_move_piece(RIGHT);
  s_longpress_movement_direction = RIGHT;
  s_longpress_timer = app_timer_register(s_longpress_tick, prv_s_longpress_tick, NULL);
}

static void prv_down_long_release_handler(ClickRecognizerRef recognizer, void *context) {
  s_longpress_movement_direction = 0;
  s_longpress_timer = NULL;
}

static void prv_up_long_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (s_status != GameStatusPlaying) { return; }
  prv_game_move_piece(LEFT);
  s_longpress_movement_direction = LEFT;
  s_longpress_timer = app_timer_register(s_longpress_tick, prv_s_longpress_tick, NULL);
}

static void prv_up_long_release_handler(ClickRecognizerRef recognizer, void *context) {
  s_longpress_movement_direction = 0;
  s_longpress_timer = NULL;
}

static void prv_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP,     prv_up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN,   prv_down_click_handler);
  window_single_click_subscribe(BUTTON_ID_BACK,   prv_back_click_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select_click_handler);

  window_long_click_subscribe(BUTTON_ID_SELECT, 500, prv_select_long_click_handler, NULL);
  window_long_click_subscribe(BUTTON_ID_DOWN, 200, prv_down_long_click_handler, prv_down_long_release_handler);
  window_long_click_subscribe(BUTTON_ID_UP, 200, prv_up_long_click_handler, prv_up_long_release_handler);
}

// ------------------------ //
// **** DRAW FUNCTIONS **** //
// ------------------------ //

// Draw current block and its shadow in game area
static void prv_draw_game(Layer *layer, GContext *ctx) {

  if (s_status != GameStatusPlaying || s_game_state.block_type == -1) { return; }

  GPoint *block = s_game_state.block;

  // Fast-drop is instant, so we need to show a guide.
  if(game_settings.set_drop_shadow) { 
    graphics_context_set_fill_color(ctx, theme.drop_shadow_color);
    int max_drop = find_max_drop(block, s_grid_blocks);
      for (int i=0; i<4; i++) {
      GRect brick_ghost = GRect(block[i].x * BLOCK_SIZE, (block[i].y + max_drop)*BLOCK_SIZE, BLOCK_SIZE, BLOCK_SIZE);
      graphics_fill_rect(ctx, brick_ghost, 0, GCornerNone);
    }
  }
  // Draw the actual block.
  graphics_context_set_stroke_color(ctx, theme.block_border_color);
  graphics_context_set_fill_color(ctx, theme.block_color[s_game_state.block_type]);
  for (int i=0; i<4; i++) {
    GRect brick_block = GRect(block[i].x * BLOCK_SIZE, block[i].y * BLOCK_SIZE, BLOCK_SIZE, BLOCK_SIZE);
    graphics_fill_rect(ctx, brick_block, 0, GCornerNone);
    graphics_draw_rect(ctx, brick_block);
  }
}

// Draw rest of game board: bg, board with grid, previous blocks
static void prv_draw_bg(Layer *layer, GContext *ctx) {

  GRect bounds = layer_get_bounds(layer);
  int16_t bounds_width = bounds.size.w;
  int16_t bounds_height = bounds.size.h;

  // Color background
  graphics_context_set_fill_color(ctx, theme.window_bg_color);

  GRect color_bg = GRect(0, 0, bounds_width, bounds_height);
  graphics_fill_rect(ctx, color_bg, 0, GCornerNone);

  // Game BG.
  graphics_context_set_fill_color(ctx, theme.grid_bg_color);
  GRect game_bg = GRect(GRID_ORIGIN_X, GRID_ORIGIN_Y, GRID_PIXEL_WIDTH, GRID_PIXEL_HEIGHT);
  graphics_fill_rect(ctx, game_bg, 0, GCornerNone);

  // Grid colors from leftover blocks.
  for (int i=0; i<GAME_GRID_BLOCK_WIDTH; i++) {
    for (int j=0; j<GAME_GRID_BLOCK_HEIGHT; j++) {
      if (s_grid_colors[i][j] != 255) {
        graphics_context_set_fill_color(ctx, theme.block_color[s_grid_colors[i][j]]);
        GRect brick = GRect((i*BLOCK_SIZE) + GRID_ORIGIN_X, (j*BLOCK_SIZE) + GRID_ORIGIN_Y, BLOCK_SIZE, BLOCK_SIZE);
        graphics_fill_rect(ctx, brick, 0, GCornerNone);
      }
    }
  }

  // Display preview of upcoming block
  graphics_context_set_stroke_color(ctx, theme.block_border_color);
  graphics_context_set_fill_color(ctx, theme.block_color[s_game_state.next_block_type]);

  #if defined(PBL_ROUND)
  int sPosX = (GRID_ORIGIN_X + GRID_PIXEL_WIDTH + BLOCK_SIZE * 2) + next_block_offset(s_game_state.next_block_type);
  int sPosY =  GRID_ORIGIN_Y + BLOCK_SIZE * 5;
  #else
  // for rectangular screen, center the next block display in the right pane
  int sPosX = (GRID_ORIGIN_X + GRID_PIXEL_WIDTH + (bounds_width - (GRID_ORIGIN_X + GRID_PIXEL_WIDTH)) / 2) + next_block_offset(s_game_state.next_block_type) - BLOCK_SIZE/2;
  int sPosY =  GRID_ORIGIN_Y + BLOCK_SIZE * 2;
  #endif

  for (int i=0; i<4; i++) {
    GRect bl = GRect(sPosX+(s_game_state.next_block[i].x * BLOCK_SIZE), sPosY+(s_game_state.next_block[i].y * BLOCK_SIZE), BLOCK_SIZE, BLOCK_SIZE);
    graphics_fill_rect(ctx, bl, 0, GCornerNone);
    graphics_draw_rect(ctx, bl);
  }

  // Game BG grid.
  graphics_context_set_stroke_color(ctx, theme.grid_lines_color);
  // Draw vertical lines
  for (int i=GRID_ORIGIN_X; i<=(GRID_PIXEL_WIDTH+GRID_ORIGIN_X); i+=BLOCK_SIZE) {
    graphics_draw_line(ctx, GPoint(i, GRID_ORIGIN_Y), GPoint(i, GRID_ORIGIN_Y + GRID_PIXEL_HEIGHT)); 
  }
  // Draw horizontal lines
  for (int i=GRID_ORIGIN_Y; i<=(GRID_PIXEL_HEIGHT+GRID_ORIGIN_Y); i+=BLOCK_SIZE) {
    graphics_draw_line(ctx, GPoint(GRID_ORIGIN_X, i), GPoint(GRID_ORIGIN_X+GRID_PIXEL_WIDTH, i));
  }
}


// ------------------------ //
// **** GAME FUNCTIONS **** //
// ------------------------ //

static void prv_game_cycle() {
  if (s_game_state.block_type == -1) {
    // Create a new block.
    s_game_state.rotation = 0;
    s_game_state.block_type = s_game_state.next_block_type;
    s_game_state.next_block_type = rand() % 7;

    GPoint new_block_pos = s_game_state.block_type == LINE ? GPoint(4, 0) : GPoint(5, 1);

    make_block(s_game_state.block, s_game_state.block_type, new_block_pos.x, new_block_pos.y);
    make_block(s_game_state.next_block, s_game_state.next_block_type, 0, 0);
  }
  else {
    // Handle the current block.
    // Drop it, and if it can't drop then stick it in place and make a new block.

    GPoint *block = s_game_state.block;

    if (prv_can_drop()) {
      if(s_lockdelay_timer){
        app_timer_cancel(s_lockdelay_timer);
        s_lockdelay_timer = NULL;
        s_lockdelay_maxmoves = 15;
      }
      for (int i=0; i<4; i++) {
        block[i].y += 1;
      }
    }
    else {
      if(!s_lockdelay_timer){
        s_lockdelay_timer = app_timer_register(s_lockdelay_tick, prv_lockdelay_tick, NULL);
      }
    }
  }

  layer_mark_dirty(s_game_pane_layer);
}

static bool prv_can_drop(){
  GPoint *block = s_game_state.block;

  bool can_drop = true;
  for (int i=0; i<4; i++) {
    int dropped_block_Y = block[i].y + 1;
    if (dropped_block_Y > 19) { can_drop = false; }
    if (s_grid_blocks[block[i].x][dropped_block_Y]) { can_drop = false; }
  }

  return can_drop;
}

static void prv_game_move_piece(int movement){
  if (s_status != GameStatusPlaying) { return; }

  // Move the block left, if possible.
  for (int i=0; i<4; i++) {
    int block_moved_x = s_game_state.block[i].x + movement;
    if ((movement == LEFT && block_moved_x < 0) || (movement == RIGHT && block_moved_x > 9)) { return; }
    if(s_grid_blocks[block_moved_x][s_game_state.block[i].y]) { return; }
  }

  for (int i=0; i<4; i++) {
    s_game_state.block[i].x += movement;
  }

  prv_reset_lock_delay();
  
  layer_mark_dirty(s_game_pane_layer);
}

static void prv_game_rotate_piece() {
  // before rotation logic, is there a piece? (or if yes, is it not a square?)
  if (s_game_state.block_type == -1 || s_game_state.block_type == SQUARE) { return; }

  int new_rotation = (s_game_state.rotation + 1) % 4;
  if(game_settings.set_counterclockwise){
    new_rotation = (s_game_state.rotation - 1 + 4) % 4;
  }

  GPoint rotated_piece[4];
  rotate_mino(rotated_piece, s_game_state.block, s_game_state.block_type, new_rotation);
  
  GPoint kicked_piece[4];
  if(!rotate_try_kicks(kicked_piece, rotated_piece, s_game_state.block_type, s_game_state.rotation, new_rotation, s_grid_blocks)){
    return;
  }

  for (int i=0; i<4; i++) {
    s_game_state.block[i] = kicked_piece[i];
  }

  s_game_state.rotation = new_rotation;

  prv_reset_lock_delay();

  layer_mark_dirty(s_game_pane_layer);
}

static void prv_reset_lock_delay(){
  if(s_lockdelay_timer){
    if(s_lockdelay_maxmoves > 0) { 
      s_lockdelay_maxmoves--; 
      app_timer_reschedule(s_lockdelay_timer, 500);
    } 
  }
}

static void prv_lock_piece(){
  GPoint *block = s_game_state.block;
  int8_t block_type = s_game_state.block_type;

  // if the block type is -1 it means it's already been locked and we need to wait till next game cycle
  if(block_type == -1) { return; }

  // check again that block can't drop
  if(prv_can_drop()) { return; }

  // lock block for good
  for (int i=0; i<4; i++) {
    s_grid_blocks[block[i].x][block[i].y] = true;
    s_grid_colors[block[i].x][block[i].y] = block_type;
  }

  layer_mark_dirty(s_bg_layer);
  
  // Clear rows if possible.
  prv_clear_rows();
  
  // check if you've lost
  prv_check_if_lost();

  // Mark for new block
  s_game_state.block_type = -1;
}

static void prv_clear_rows(){
  for (int j=0; j<20; j++) { // test for all rows
    bool isRow = true;

    for (int i=0; i<10; i++) {
      if (!s_grid_blocks[i][j]) { isRow = false; } // if one block in row j is empty then it's not a row
    }

    if (!isRow) { continue; }

    s_game_state.lines_cleared += 1;
    s_lines_cleared_at_once += 1;

    // Check if we have to level up
    if (s_game_state.level < 10 && s_game_state.lines_cleared >= (10 * s_game_state.level)) { // every 10 line clears, go up 1 level, up to level 10
      s_game_state.level += 1; 
      s_tick_time -= s_tick_interval;
      update_string_num_layer("LV.", s_game_state.level, s_level_str, sizeof(s_level_str), s_level_layer);
      prv_save_game(); // TODO: does this cause lag? needed as anything (such as a background app) that makes you quit the game can make you lose your progress
    }    
    // Drop the above rows.
    for (int k=j; k>0; k--) {
      for (int i=0; i<10; i++) {
        s_grid_blocks[i][k] = s_grid_blocks[i][k-1];
        s_grid_colors[i][k] = s_grid_colors[i][k-1];
      }
    }
    
    for (int i=0; i<10; i++) {
      s_grid_blocks[i][0] = false;
      s_grid_colors[i][0] = 255;
    }
    
  }
  
  switch(s_lines_cleared_at_once) {
    case 1: 
      s_game_state.score += (40 * s_game_state.level);
      break;
    case 2:
      s_game_state.score += (100 * s_game_state.level);
      break;
    case 3:
      s_game_state.score += (300 * s_game_state.level);
      break;
    case 4:
      s_game_state.score += (1200 * s_game_state.level);
      vibes_short_pulse();
      if(!s_flash_timer) {
        s_flash_timer = app_timer_register(s_flash_tick, prv_flash_tick, NULL);
      }
      break;
    default: break;
  }

  if (s_game_state.score >= 999999)
    s_game_state.score = 999999;

  update_string_num_layer("SCORE\n", s_game_state.score, s_score_str, sizeof(s_score_str), s_score_layer);
  update_string_num_layer("LINES\n", s_game_state.lines_cleared, s_lines_str, sizeof(s_lines_str), s_lines_layer);

  s_lines_cleared_at_once = 0;
}

static void prv_check_if_lost(){
  GPoint *block = s_game_state.block;
  // Check whether you've lost by seeing if any block is on the top row (y=0).
  for (int i=0; i<4; i++) {
    if (block[i].y == 0) {
      s_status = GameStatusLost;
      Layer *window_layer = window_get_root_layer(s_window);

      text_layer_set_text(s_paused_label_layer, "You lost!");
      layer_add_child(window_layer, text_layer_get_layer(s_paused_label_layer));
      
      return;
    }
  }
}

// -------------------------- //
// ***** TICK FUNCTIONS ***** //
// -------------------------- //

static void prv_game_tick(void *data) {
  if (s_status != GameStatusPlaying) { return; }

  prv_game_cycle();
  s_game_timer = app_timer_register(s_tick_time, prv_game_tick, NULL);
}

static void prv_s_longpress_tick(void *data) {
  if (s_status != GameStatusPlaying || s_longpress_movement_direction == 0) {
    s_longpress_timer = NULL;
    return; 
  }

  prv_game_move_piece(s_longpress_movement_direction);
  s_longpress_timer = app_timer_register(s_longpress_tick, prv_s_longpress_tick, NULL);
}

static void prv_lockdelay_tick(void *data) {
  if (s_status != GameStatusPlaying) { return; }

  prv_lock_piece();
  s_lockdelay_timer = NULL;
}

static void prv_flash_tick(void *data) {
  if (s_status != GameStatusPlaying) { return; }

  // TODO: No clue if there is a more efficient way of doing this

  if (s_flash_amount == 0) {
    s_flash_timer = NULL;
    s_flash_amount = 5;
    theme.grid_bg_color = s_og_bg_color;
    layer_mark_dirty(s_bg_layer);
    return;
  }

  theme.grid_bg_color = s_flash_amount % 2 ? s_og_bg_color : s_flash_bg_color;
  s_flash_amount--;
  layer_mark_dirty(s_bg_layer);
  s_flash_timer = app_timer_register(s_flash_tick, prv_flash_tick, NULL);
}

// -------------------------- //
// ***** DATA FUNCTIONS ***** //
// -------------------------- //

static void prv_setup_game() {
  s_status = GameStatusPlaying;
  s_game_state.lines_cleared = 0;
  s_game_state.level = 1;
  s_game_state.score = 0;
  s_game_state.rotation = 0;
  s_game_state.next_block_type = rand() % 7;
  s_game_state.block_type = -1;

  for (int i=0; i<10; i++) {
    for (int j=0; j<20; j++) {
      s_grid_blocks[i][j] = false;
      s_grid_colors[i][j] = 255;
    }
  }

  s_tick_time = s_max_tick;
  
  update_string_num_layer("SCORE\n", 0, s_score_str, sizeof(s_score_str), s_score_layer);
  update_string_num_layer("LV.", s_game_state.level, s_level_str, sizeof(s_level_str), s_level_layer);
  update_string_num_layer("LINES\n", 0, s_lines_str, sizeof(s_lines_str), s_lines_layer);

}

static void prv_load_game() {
  // We already know that we have valid data (can_load).

  if (persist_exists(GAME_STATE_KEY)){
    APP_LOG(APP_LOG_LEVEL_INFO, "Reading saved data");
    persist_read_data(GAME_STATE_KEY, &s_game_state, sizeof(GameState));
    persist_read_data(GAME_GRID_BLOCK_KEY, &s_grid_blocks, sizeof(s_grid_blocks));
    persist_read_data(GAME_GRID_COLOR_KEY, &s_grid_colors, sizeof(s_grid_colors));
  } else {
    APP_LOG(APP_LOG_LEVEL_INFO, "No saved data");
    // TODO : Throw error, couldnt find data
    return;
  }

  update_string_num_layer("SCORE\n", s_game_state.score, s_score_str, sizeof(s_score_str), s_score_layer);
  update_string_num_layer("LV.",     s_game_state.level, s_level_str, sizeof(s_level_str), s_level_layer);
  update_string_num_layer("LINES\n", s_game_state.lines_cleared, s_lines_str, sizeof(s_lines_str), s_lines_layer);

  s_tick_time = s_max_tick - (s_tick_interval * s_game_state.level);
}

static void prv_quit_after_loss() {
  prv_save_game();

  s_game_timer = NULL;

  window_stack_pop(true);
  new_score_window_push(s_game_state.score, s_game_state.level);
}

static void prv_save_game() {
  if(s_status != GameStatusLost) {
    persist_write_data(GAME_STATE_KEY, &s_game_state, sizeof(GameState));
    persist_write_data(GAME_GRID_BLOCK_KEY, &s_grid_blocks, sizeof(s_grid_blocks)); // the GRID arrays are too big to fit with the rest of the game state
    persist_write_data(GAME_GRID_COLOR_KEY, &s_grid_colors, sizeof(s_grid_colors)); // ...as the persist keys can only each hold 256 bytes 
    persist_write_bool(GAME_CONTINUE_KEY, true);
    APP_LOG(APP_LOG_LEVEL_INFO, "SAVED GAME");
    return;
  } else {
    APP_LOG(APP_LOG_LEVEL_INFO, "DELETING SAVE");
    persist_delete(GAME_STATE_KEY);
    persist_delete(GAME_GRID_BLOCK_KEY);
    persist_delete(GAME_GRID_COLOR_KEY);
    persist_write_bool(GAME_CONTINUE_KEY, false);
  }
}