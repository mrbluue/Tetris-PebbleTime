#include <pebble.h>

#include "helpers.h"
#include "score_window.h"

#if defined(PBL_PLATFORM_CHALK)
  #define SCREEN_WIDTH 180
  #define SCREEN_HEIGHT 180

  #define GRID_PIXEL_WIDTH (BLOCK_SIZE * GRID_BLOCK_WIDTH)
  #define GRID_PIXEL_HEIGHT (BLOCK_SIZE * GRID_BLOCK_HEIGHT)

  #define GRID_ORIGIN_X ((SCREEN_WIDTH - GRID_PIXEL_WIDTH) / 2)
  #define GRID_ORIGIN_Y ((SCREEN_HEIGHT - GRID_PIXEL_HEIGHT) / 2)
#else
  #define GRID_PIXEL_WIDTH (BLOCK_SIZE * GRID_BLOCK_WIDTH)
  #define GRID_PIXEL_HEIGHT (BLOCK_SIZE * GRID_BLOCK_HEIGHT)

  #define GRID_ORIGIN_X 0
  #define GRID_ORIGIN_Y 0
#endif

#define LABEL_HEIGHT 20 

#define LEFT -1
#define RIGHT 1

static Window *s_window;

static GameState s_game_state;
static GameStatus s_status;
static uint8_t s_grid_blocks[GRID_BLOCK_WIDTH][GRID_BLOCK_HEIGHT];
static uint8_t s_grid_colors[GRID_BLOCK_WIDTH][GRID_BLOCK_HEIGHT];

static bool pauseFromFocus = false;

static int lines_cleared_at_once = 0;

static int max_tick = 600;
static int tick_time;
static int tick_interval = 40;

static int longpress_tick = 300;

static AppTimer *s_game_timer = NULL;
static AppTimer *s_longpress_timer = NULL;
static int longpress_movement_direction;


static char s_score_str[10];
static char s_level_str[10];

static Layer     *s_bg_layer = NULL;
static Layer     *s_game_pane_layer = NULL;
static TextLayer *s_score_label_layer;
static TextLayer *s_score_layer;
static TextLayer *s_level_label_layer;
static TextLayer *s_level_layer;
static TextLayer *s_paused_label_layer;

static void prv_game_window_push();
// static void prv_game_window_pop();
// static void prv_init();
static void prv_window_load(Window *window);
static void prv_window_unload(Window *window);
static void prv_app_focus_handler(bool focus);

static void prv_select_click_handler(ClickRecognizerRef recognizer, void *context);
static void prv_up_click_handler(ClickRecognizerRef recognizer, void *context);
static void prv_down_click_handler(ClickRecognizerRef recognizer, void *context);
static void prv_back_click_handler(ClickRecognizerRef recognizer, void *context);
static void prv_select_long_click_handler(ClickRecognizerRef recognizer, void *context);
// static void prv_up_long_click_handler(ClickRecognizerRef recognizer, void *context);
// static void prv_down_long_click_handler(ClickRecognizerRef recognizer, void *context);
static void prv_click_config_provider(void *context);

static void prv_draw_game(Layer *layer, GContext *ctx);
static void prv_draw_bg(Layer *layer, GContext *ctx);

static void prv_drop_block();
static void prv_move_piece(int movement);
static void prv_rotate_piece();
static void prv_game_tick(void *data);
static void prv_longpress_tick(void *data);
static void prv_setup_game();
static void prv_load_game();
static void prv_quit_after_loss();
static void prv_save_game();


void new_game(){
  APP_LOG(APP_LOG_LEVEL_INFO, "NEW GAME");
  prv_game_window_push();
  prv_setup_game();
  prv_drop_block();
  if (!s_game_timer) {
    s_game_timer = app_timer_register(tick_time, prv_game_tick, NULL);
  }
}

void continue_game(){
  APP_LOG(APP_LOG_LEVEL_INFO, "CONTINUE GAME");
  prv_game_window_push();
  prv_setup_game();
  prv_load_game();
  prv_drop_block();
  if (!s_game_timer) {
    s_game_timer = app_timer_register(tick_time, prv_game_tick, NULL);
  }  
}

static void prv_game_window_push() {
	
  // prv_init();

	s_window = window_create();
	window_set_window_handlers(s_window, (WindowHandlers) {
		.load = prv_window_load,
		.unload = prv_window_unload,
	});
  window_set_click_config_provider(s_window, prv_click_config_provider);
  const bool animated = true;
	window_stack_push(s_window, animated);
}

// static void prv_game_window_pop() {
// 	window_stack_remove(s_window, true);
// }

// static void prv_init(){


// }

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

  s_score_label_layer = text_layer_create((GRect) { 
    .origin = { 
       GRID_ORIGIN_X + GRID_PIXEL_WIDTH + 1,                // + 1 to avoid overlapping with grid edge
      (GRID_ORIGIN_Y + BLOCK_SIZE * PBL_IF_RECT_ELSE(6, 9)) // place layer 6 or 9 blocks from top vertical coordinate of play area
    }, 
    .size = { 
      bounds_width - (GRID_ORIGIN_X + GRID_PIXEL_WIDTH),    // as wide as the space to the right of the play area
      LABEL_HEIGHT 
    } 
  });

  text_layer_set_text(s_score_label_layer, "Score");
  text_layer_set_text_alignment(s_score_label_layer, GTextAlignmentCenter);
  text_layer_set_background_color(s_score_label_layer, theme.window_label_bg_color);
  text_layer_set_text_color(s_score_label_layer, theme.window_label_text_color);
  layer_add_child(window_layer, text_layer_get_layer(s_score_label_layer));

  s_score_layer = text_layer_create((GRect) { 
    .origin = { 
       GRID_ORIGIN_X + GRID_PIXEL_WIDTH + 1, 
      (GRID_ORIGIN_Y + BLOCK_SIZE * PBL_IF_RECT_ELSE(7, 10) + LABEL_HEIGHT) // 7 or 10 blocks from top, below previous label
    }, 
    .size = { 
      bounds_width - (GRID_ORIGIN_X + GRID_PIXEL_WIDTH),
      LABEL_HEIGHT
    } 
  });
  text_layer_set_text_alignment(s_score_layer, GTextAlignmentCenter);
  text_layer_set_background_color(s_score_layer, theme.window_label_bg_color);
  text_layer_set_text_color(s_score_layer, theme.window_label_text_color);
  layer_add_child(window_layer, text_layer_get_layer(s_score_layer));

  s_level_label_layer = text_layer_create((GRect) { 
    .origin = { 
      PBL_IF_RECT_ELSE(GRID_ORIGIN_X + GRID_PIXEL_WIDTH + 1,              0), // align to the left of play area on Round screen
      PBL_IF_RECT_ELSE(GRID_ORIGIN_Y + BLOCK_SIZE * 8 + LABEL_HEIGHT * 2, GRID_ORIGIN_Y + BLOCK_SIZE * 7)
    }, 
    .size = { 
      bounds_width - (GRID_ORIGIN_X + GRID_PIXEL_WIDTH),
      LABEL_HEIGHT 
    } 
  });
  text_layer_set_text(s_level_label_layer, "Level");
  text_layer_set_text_alignment(s_level_label_layer, GTextAlignmentCenter);
  text_layer_set_background_color(s_level_label_layer, theme.window_label_bg_color);
  text_layer_set_text_color(s_level_label_layer, theme.window_label_text_color);
  layer_add_child(window_layer, text_layer_get_layer(s_level_label_layer));

  s_level_layer = text_layer_create((GRect) { 
    .origin = { 
      PBL_IF_RECT_ELSE(GRID_ORIGIN_X + GRID_PIXEL_WIDTH + 1,              0),
      PBL_IF_RECT_ELSE(GRID_ORIGIN_Y + BLOCK_SIZE * 9 + LABEL_HEIGHT * 3, GRID_ORIGIN_Y + BLOCK_SIZE * 8 + LABEL_HEIGHT) 
    }, 
    .size = { 
      bounds_width - (GRID_ORIGIN_X + GRID_PIXEL_WIDTH),
      LABEL_HEIGHT 
    } 
  });
  text_layer_set_text_alignment(s_level_layer, GTextAlignmentCenter);
  text_layer_set_background_color(s_level_layer, theme.window_label_bg_color);
  text_layer_set_text_color(s_level_layer, theme.window_label_text_color);
  layer_add_child(window_layer, text_layer_get_layer(s_level_layer));

  s_paused_label_layer = text_layer_create((GRect) { 
    .origin = { 
      GRID_ORIGIN_X+1, 
      GRID_ORIGIN_Y + (GRID_PIXEL_HEIGHT/2) - LABEL_HEIGHT // place in middle of play area
    }, 
    .size = { 
      GRID_PIXEL_WIDTH-1, 
      LABEL_HEIGHT 
    } 
  });
  text_layer_set_text(s_paused_label_layer, "Paused");
  text_layer_set_background_color(s_paused_label_layer, theme.window_label_bg_color);
  text_layer_set_text_color(s_paused_label_layer, theme.window_label_text_color);
  text_layer_set_text_alignment(s_paused_label_layer, GTextAlignmentCenter);

  app_focus_service_subscribe(prv_app_focus_handler);

}

static void prv_window_unload(Window *window) {
  app_focus_service_unsubscribe();
  
  // app_timer_cancel(s_game_timer);
  s_game_timer      = NULL;
  s_longpress_timer = NULL;

  text_layer_destroy(s_score_label_layer);
  text_layer_destroy(s_score_layer);
  text_layer_destroy(s_level_label_layer);
  text_layer_destroy(s_level_layer);
  text_layer_destroy(s_paused_label_layer);
  layer_destroy(s_bg_layer);
  layer_destroy(s_game_pane_layer);
}

static void prv_app_focus_handler(bool focus) {
  if (!focus) {
    pauseFromFocus = true;
    s_status = GameStatusPaused;
    Layer *window_layer = window_get_root_layer(s_window);
    layer_add_child(window_layer, text_layer_get_layer(s_paused_label_layer));
  }
  else {
    if (pauseFromFocus) {
      s_status = GameStatusPlaying;
      s_game_timer = app_timer_register(tick_time, prv_game_tick, NULL);
      layer_remove_from_parent(text_layer_get_layer(s_paused_label_layer));
    }
    pauseFromFocus = false;
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
    s_game_timer = app_timer_register(tick_time, prv_game_tick, NULL);
    layer_remove_from_parent(text_layer_get_layer(s_paused_label_layer));
    return;
  }

  prv_rotate_piece();

}

static void prv_up_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (s_status != GameStatusPlaying) { return; }
  prv_move_piece(LEFT);
}

static void prv_down_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (s_status != GameStatusPlaying) { return; }
  prv_move_piece(RIGHT);
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
  int drop_amount = find_max_drop(s_game_state.block, s_grid_blocks);
  for (int i=0; i<4; i++) {
    s_game_state.block[i].y += drop_amount;
  }
}

static void prv_down_long_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (s_status != GameStatusPlaying) { return; }
  APP_LOG(APP_LOG_LEVEL_INFO, "LONG DOWN PRESS");
  longpress_movement_direction = RIGHT;
  s_longpress_timer = app_timer_register(longpress_tick, prv_longpress_tick, NULL);
}

static void prv_down_long_release_handler(ClickRecognizerRef recognizer, void *context) {
  APP_LOG(APP_LOG_LEVEL_INFO, "LONG DOWN RELEASE");
  longpress_movement_direction = 0;
  s_longpress_timer = NULL;
}

static void prv_up_long_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (s_status != GameStatusPlaying) { return; }
  APP_LOG(APP_LOG_LEVEL_INFO, "LONG DOWN PRESS");
  longpress_movement_direction = LEFT;
  s_longpress_timer = app_timer_register(longpress_tick, prv_longpress_tick, NULL);
}

static void prv_up_long_release_handler(ClickRecognizerRef recognizer, void *context) {
  APP_LOG(APP_LOG_LEVEL_INFO, "LONG DOWN RELEASE");
  longpress_movement_direction = 0;
  s_longpress_timer = NULL;
}

// static void prv_up_long_click_handler(ClickRecognizerRef recognizer, void *context) {
//   if (s_status != GameStatusPlaying) { return; }
// }

// static void prv_down_long_click_handler(ClickRecognizerRef recognizer, void *context) {
//   if (s_status != GameStatusPlaying) { return; }
// }

static void prv_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP,     prv_up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN,   prv_down_click_handler);
  window_single_click_subscribe(BUTTON_ID_BACK,   prv_back_click_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select_click_handler);

  window_long_click_subscribe(BUTTON_ID_SELECT, 500, prv_select_long_click_handler, NULL);
  window_long_click_subscribe(BUTTON_ID_DOWN, 250, prv_down_long_click_handler, prv_down_long_release_handler);
  window_long_click_subscribe(BUTTON_ID_UP, 250, prv_up_long_click_handler, prv_up_long_release_handler);
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
  for (int i=0; i<GRID_BLOCK_WIDTH; i++) {
    for (int j=0; j<GRID_BLOCK_HEIGHT; j++) {
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

static void prv_drop_block() {
  if (s_game_state.block_type == -1) {
    // Create a new block.
    s_game_state.block_type = s_game_state.next_block_type;
    s_game_state.next_block_type = rand() % 7;
    s_game_state.rotation = 0;
    s_game_state.block_X = 5;
    s_game_state.block_Y = 0;
    s_game_state.next_block_X = 0;
    s_game_state.next_block_Y = 0;
    make_block(s_game_state.block, s_game_state.block_type, s_game_state.block_X, s_game_state.block_Y);
    make_block(s_game_state.next_block, s_game_state.next_block_type, s_game_state.next_block_X, s_game_state.next_block_Y);
  }
  else {
    // Handle the current block.
    // Drop it, and if it can't drop then stick it in place and make a new block.

    GPoint *block = s_game_state.block;
    bool canDrop = true;

    for (int i=0; i<4; i++) {
      int dropped_block_Y = block[i].y + 1;
      if (dropped_block_Y > 19) { canDrop = false; }
      if (s_grid_blocks[block[i].x][dropped_block_Y]) { canDrop = false; }
    }

    if (canDrop) {
      for (int i=0; i<4; i++) {
        block[i].y += 1;
      }
    }
    else {
      for (int i=0; i<4; i++) {
        s_grid_blocks[block[i].x][block[i].y] = true;
        s_grid_colors[block[i].x][block[i].y] = s_game_state.block_type;
      }
      layer_mark_dirty(s_bg_layer);
      s_game_state.block_type = -1;

      // Clear rows if possible.
      for (int j=0; j<20; j++) { // test for all rows
        bool isRow = true;

        for (int i=0; i<10; i++) {
          if (!s_grid_blocks[i][j]) { isRow = false; } // if one block in row j is empty then it's not a row
        }

        if (!isRow) { continue; }

        s_game_state.lines_cleared += 1;
        lines_cleared_at_once += 1;

        if (s_game_state.level < 10 && s_game_state.lines_cleared >= (10 * s_game_state.level)) { // every 10 line clears, go up 1 level, up to level 10
          s_game_state.level += 1; 
          tick_time -= tick_interval;
          update_num_layer(s_game_state.level, s_level_str, s_level_layer);
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
      
      switch(lines_cleared_at_once) {
        case 1: 
        // 40 points
        s_game_state.score += 40;
        break;
        case 2:
        // 100 points
        s_game_state.score += 100;
        break;
        case 3:
        // 300 points
        s_game_state.score += 300;
        break;
        case 4:
        // 1200 points
        s_game_state.score += 1200;
        text_layer_set_text(s_score_layer, "TETRIS");
        // TODO: TETRIS flash and text(?)
        break;
        default: break;
      }

      update_num_layer(s_game_state.score, s_score_str, s_score_layer);

      lines_cleared_at_once = 0;

      // Check whether you've lost.
      for (int i=0; i<4; i++) {
        if (block[i].y == 0) {
          s_status = GameStatusLost;
          Layer *window_layer = window_get_root_layer(s_window);

          text_layer_set_text(s_paused_label_layer, "You lost!");
          layer_add_child(window_layer, text_layer_get_layer(s_paused_label_layer));
          
          // TODO : press any button for LOSING SCREEN

          // layer_remove_from_parent(s_bg_layer);
          // layer_remove_from_parent(s_game_pane_layer);
          // layer_add_child(window_layer, text_layer_get_layer(title_layer));
          // text_layer_set_text(s_title_layer, "You lost!");
          // text_layer_set_text(s_score_label_layer, "");
          // text_layer_set_text(s_score_layer, "");
          // text_layer_set_text(s_level_label_layer, "");
          // text_layer_set_text(s_level_layer, "");

          // TODO When you lose, wipe the save.
          // No need to get fancy; just mark 'has save' false since
          // we'll re-create the whole thing when it gets set to true.
          // persist_write_bool(HAS_SAVE_KEY, false);
          // TODO High score?
          // if (!persist_exists(HIGH_SCORE_KEY) || lines_cleared > persist_read_int(HIGH_SCORE_KEY)) {
          //   persist_write_int(HIGH_SCORE_KEY, lines_cleared);
          //   layer_add_child(window_layer, text_layer_get_layer(high_score_layer));
          //   text_layer_set_text(high_score_layer, "New high score!");
          // }
          return;
        }
      }
    }
  }

  layer_mark_dirty(s_game_pane_layer);
}

static void prv_move_piece(int movement){
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

  layer_mark_dirty(s_game_pane_layer);
}

static void prv_rotate_piece() {
  // before rotation logic, is there a block?
  if (s_game_state.block_type == -1) { return; }

  int new_rotation = (s_game_state.rotation + 1) % 4;
  if(game_settings.set_counterclockwise){
    new_rotation = (s_game_state.rotation - 1 + 4) % 4;
  }

  GPoint rotated_block[4];
  rotate_block(rotated_block, s_game_state.block, s_game_state.block_type, new_rotation);

  bool should_rotate = true;

  for (int i=0; i<4; i++) {
    if (rotated_block[i].x < 0 || rotated_block[i].x >= GRID_BLOCK_WIDTH) { should_rotate = false; } // TODO: Wall kicks, when certain blocks are on the edge, they currently cant rotate when there is actually space
    if (rotated_block[i].y >= GRID_BLOCK_HEIGHT) { should_rotate = false; } 
    else if (rotated_block[i].y < 0) { 
      for (int i=0; i<4; i++) {
        rotated_block[i].y += (s_game_state.block_type == LINE) ? 2 : 1; // kick down pieces too high on the grid to rotate (by 2 for the line)
      }
    } 
    if (s_grid_blocks[rotated_block[i].x][rotated_block[i].y]) { should_rotate = false; }
  }
  
  if (!should_rotate) { return; }

  for (int i=0; i<4; i++) {
    s_game_state.block[i] = rotated_block[i];
  }

  s_game_state.rotation = new_rotation;

  layer_mark_dirty(s_game_pane_layer);
}

static void prv_game_tick(void *data) {
  if (s_status != GameStatusPlaying) { return; }

  prv_drop_block();
  s_game_timer = app_timer_register(tick_time, prv_game_tick, NULL);
}

static void prv_longpress_tick(void *data) {
  if (s_status != GameStatusPlaying || longpress_movement_direction == 0) {
    s_longpress_timer = NULL;
    return; 
  }

  prv_move_piece(longpress_movement_direction);
  s_longpress_timer = app_timer_register(longpress_tick, prv_longpress_tick, NULL);
}

static void prv_setup_game() {

  // APP_LOG(APP_LOG_LEVEL_INFO, "SETUP start");

  s_status = GameStatusPlaying;
  s_game_state.level = 1;
  s_game_state.lines_cleared = 0;
  s_game_state.rotation = 0;
  s_game_state.next_block_type = rand() % 7;
  s_game_state.block_type = -1;

  for (int i=0; i<10; i++) {
    for (int j=0; j<20; j++) {
      s_grid_blocks[i][j] = false;
      s_grid_colors[i][j] = 255;
    }
  }

  tick_time = max_tick;
  
  update_num_layer(0, s_score_str, s_score_layer);
  update_num_layer(0, s_level_str, s_level_layer);  

}

// TODO This should probably go in the helper file with the save logic.
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

  // s_game_state.level = (s_game_state.lines_cleared / 10) + 1;
  // if (s_game_state.level > 10) { s_game_state.level = 10; }

  update_num_layer(s_game_state.score, s_score_str, s_score_layer);
  update_num_layer(s_game_state.level,         s_level_str, s_level_layer);
  tick_time = max_tick - (tick_interval * s_game_state.level);

  make_block(s_game_state.block,      s_game_state.block_type,      s_game_state.block_X,      s_game_state.block_Y);
  make_block(s_game_state.next_block, s_game_state.next_block_type, s_game_state.next_block_X, s_game_state.next_block_Y);

  if (s_game_state.rotation != 0) {
    for (int i=0; i<s_game_state.rotation; i++) {
      GPoint rPoints[4];
      rotate_block(rPoints, s_game_state.block, s_game_state.block_type, i);
      for (int j=0; j<4; j++) {
        s_game_state.block[j] = rPoints[j];
      }
    }
  }
}

// TODO: complete rewrite into opening You Lost screen with high scores
static void prv_quit_after_loss() {

  prv_save_game();

  s_game_timer = NULL;

  window_stack_pop(true);
  new_score_window_push(s_game_state.score);


  // for (int i=0; i<10; i++) {
  //   for (int j=0; j<20; j++) {
  //     s_grid_blocks[i][j] = false;
  //     s_grid_colors[i][j] = 255;
  //   }
  // }
  // s_game_state.block_type = -1;
  // s_game_state.next_block_type = -1;
  // load_choice = 0;
  // Layer *window_layer = window_get_root_layer(s_window);

  // if (persist_exists(HIGH_SCORE_KEY)) {
  //   itoa10(persist_read_int(HIGH_SCORE_KEY), high_score_buffer);
  // }
  // else {
  //   itoa10(0, high_score_buffer);
  // }
  // Fool strcat into thinking the string is empty.
  // high_score_str[0] = '\0';
  // strcat(high_score_str, "High score: ");
  // text_layer_set_text(high_score_layer, strcat(high_score_str, high_score_buffer));
  // layer_mark_dirty(s_title_pane_layer);
}

static void prv_save_game() {
  if(s_status != GameStatusLost) {

    // Normalize the block position for saving. We'll replay the rotation on load.
    while (s_game_state.rotation != 0) {
      GPoint new_points[4];
      rotate_block(new_points, s_game_state.block, s_game_state.block_type, s_game_state.rotation);
      for (int i=0; i<4; i++) {
        s_game_state.block[i] = new_points[i];
      }
      s_game_state.rotation = (s_game_state.rotation+1)%4;
    }

    persist_write_data(GAME_STATE_KEY, &s_game_state, sizeof(GameState));
    persist_write_data(GAME_GRID_BLOCK_KEY, &s_grid_blocks, sizeof(s_grid_blocks)); // the GRID arrays are too big to fit with the rest of the game state
    persist_write_data(GAME_GRID_COLOR_KEY, &s_grid_colors, sizeof(s_grid_colors)); // as the persist keys can only each hold 256 bytes 
    persist_write_bool(GAME_CONTINUE_KEY, true);
    APP_LOG(APP_LOG_LEVEL_INFO, "SAVED GAME");
    return;
  } else {
    APP_LOG(APP_LOG_LEVEL_INFO, "DELETING SAVE");
    persist_delete(GAME_STATE_KEY);
    persist_delete(GAME_GRID_BLOCK_KEY);
    persist_delete(GAME_GRID_COLOR_KEY);
    persist_write_bool(GAME_CONTINUE_KEY, false);
    // TODO high score logic?
  }
}