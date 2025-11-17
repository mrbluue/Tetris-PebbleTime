/* Copyright (c) 2015, WRansohoff
 * MIT License, see LICENSE file for details.
 */
#include <pebble.h>

#include "helpers.h"
#include "game_window.h"
#include "score_window.h"
#include "settings_window.h"

#define MENU_OPTIONS 4 
#define MENU_OPTION_HEIGHT 26 

#define MENU_GRID_COL_WIDTH 13
#define MENU_GRID_ROW_HEIGHT 13
#define MENU_GRID_BLOCK_SIZE 13

#define MENU_GRID_COLS 10
#define MENU_GRID_ROWS 9

#if defined(PBL_PLATFORM_CHALK)
  #define MENU_GRID_X_OFFSET 12
  #define MENU_GRID_X 25
  #define MENU_GRID_Y 48
#else 
  #define MENU_GRID_X_OFFSET 7
  #define MENU_GRID_X 7
  #define MENU_GRID_Y 48
#endif

#define MENU_GRID_WIDTH  (MENU_GRID_COLS * MENU_GRID_COL_WIDTH)
#define MENU_GRID_HEIGHT (MENU_GRID_ROWS * MENU_GRID_ROW_HEIGHT)
#define MENU_GRID_X_END  (MENU_GRID_X + MENU_GRID_COLS * MENU_GRID_COL_WIDTH)
#define MENU_GRID_Y_END  (MENU_GRID_Y + MENU_GRID_ROWS * MENU_GRID_ROW_HEIGHT)

GameSettings game_settings;
Theme theme;

GFont s_font_title;
GFont s_font_menu;
GFont s_font_mono_small;
GFont s_font_mono_big;

char *MENU_OPTION_LABELS[4] = {"CONTINUE", "NEW GAME", "HIGH SCORE", "SETTINGS"};

static Window    *s_window;
static TextLayer *s_title_layer;
static TextLayer *s_menu_option_text_layer[4];

static Layer *s_title_pane_layer = NULL;

static bool can_load = false; // is there a game to continue
static bool continue_label_showing = false; 
static int menu_option = -1;

static void find_save(){
  if (persist_exists(GAME_STATE_KEY) && persist_exists(GAME_CONTINUE_KEY)) {
    if (persist_read_bool(GAME_CONTINUE_KEY)) {
      can_load = true;
      return;
    }
  }
  if(menu_option == 0) { menu_option = 1;}
  can_load = false;
}

// ------------------------- //
// ***** CLICK HANDLERS **** //
// ------------------------- //

static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
  switch(menu_option){
    case -1:
      break;
    case 0: 
      continue_game();
      break;
    case 1:
      new_game();
      break;
    case 2:
      all_scores_window_push();
      // new_score_window_push(1000); // for testing
      break;
    case 3:
      settings_window_push();
      break;
    default: break;
  }
}

static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
  if(menu_option > 1) 
    menu_option -= 1;
  else if(menu_option == 1 && can_load) 
    menu_option = 0;

  layer_mark_dirty(s_title_pane_layer);
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
  if(menu_option == -1 && can_load)
    menu_option = 0;
  else if(menu_option == -1 && !can_load)
    menu_option = 1;
  else if(menu_option < MENU_OPTIONS - 1) 
    menu_option += 1;

  layer_mark_dirty(s_title_pane_layer);
}

static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
}

// ------------------------ //
// **** DRAW FUNCTIONS **** //
// ------------------------ //

static void draw_title_pane(Layer *layer, GContext *ctx) {
  int grid_origin_y = MENU_GRID_Y;
  if(!can_load) {
    grid_origin_y += (2 * MENU_GRID_BLOCK_SIZE);
  }

  graphics_context_set_fill_color(ctx, GColorDarkGray);
  graphics_fill_rect(ctx, GRect(0, grid_origin_y, SCREEN_WIDTH, SCREEN_HEIGHT - grid_origin_y), 0, GCornerNone);

  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_context_set_stroke_width(ctx, 2);

  if(menu_option >= 0){
    graphics_context_set_fill_color(ctx, theme.block_color[menu_option]);
    graphics_fill_rect(ctx, GRect(0, MENU_GRID_Y + (menu_option*2) * MENU_GRID_BLOCK_SIZE, SCREEN_WIDTH, MENU_GRID_BLOCK_SIZE * 3), 0, GCornerNone);
  }

  // vertical lines
  for (int i=MENU_GRID_X_OFFSET; i<=SCREEN_WIDTH; i+=MENU_GRID_BLOCK_SIZE) {
    graphics_draw_line(ctx, GPoint(i, grid_origin_y), GPoint(i, SCREEN_HEIGHT)); 
  }
  // horizontal lines
  for (int i=grid_origin_y; i<=SCREEN_HEIGHT; i+=MENU_GRID_BLOCK_SIZE) {
    graphics_draw_line(ctx, GPoint(0, i), GPoint(SCREEN_WIDTH, i)); 
  }

}

// -------------------------- //
// **** WINDOW FUNCTIONS **** //
// -------------------------- //

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);

  window_set_background_color(window, GColorFromRGB(0, 0, 0));

  GRect bounds = layer_get_bounds(window_layer);
  int16_t bounds_width = bounds.size.w;
  int16_t bounds_height = bounds.size.h;

  s_title_pane_layer = layer_create(GRect(0, 0, bounds_width, bounds_height));
  layer_set_update_proc(s_title_pane_layer, draw_title_pane);
  layer_add_child(window_layer, s_title_pane_layer);
  
  s_title_layer = text_layer_create((GRect) { .origin = { 0, 20 }, .size = { bounds_width, 20 } });
  text_layer_set_text(s_title_layer, "Pebtris"); 
  text_layer_set_font(s_title_layer, s_font_title);
  text_layer_set_text_alignment(s_title_layer, GTextAlignmentCenter);
  text_layer_set_background_color(s_title_layer, GColorClear);
  text_layer_set_text_color(s_title_layer, GColorWhite);
  layer_add_child(window_layer, text_layer_get_layer(s_title_layer));
  
  for (int i=0; i<MENU_OPTIONS; i++){
    s_menu_option_text_layer[i] = text_layer_create((GRect) { .origin = { 0, 60 + i * MENU_OPTION_HEIGHT }, .size = { bounds_width, MENU_OPTION_HEIGHT } });
    text_layer_set_text(s_menu_option_text_layer[i], MENU_OPTION_LABELS[i]);
    text_layer_set_font(s_menu_option_text_layer[i], s_font_menu);
    text_layer_set_text_alignment(s_menu_option_text_layer[i], GTextAlignmentCenter);
    text_layer_set_background_color(s_menu_option_text_layer[i], GColorClear);
    text_layer_set_text_color(s_menu_option_text_layer[i], GColorWhite);
    if (i > 0) {
      layer_add_child(window_layer, text_layer_get_layer(s_menu_option_text_layer[i]));
    }
  }

}

static void window_appear(Window *window) {
  Layer *window_layer = window_get_root_layer(window);

  find_save();

  if(can_load && !continue_label_showing){
    layer_add_child(window_layer, text_layer_get_layer(s_menu_option_text_layer[0]));
    continue_label_showing = true;
  } else if(!can_load && continue_label_showing) {
    text_layer_destroy(s_menu_option_text_layer[0]);
    continue_label_showing = false;
  }
}

static void window_unload(Window *window) {
  // TODO / REMINDER - Add any new objects here for destruction on exit!
  text_layer_destroy(s_title_layer);
  for (int i=0; i<MENU_OPTIONS; i++){
    text_layer_destroy(s_menu_option_text_layer[i]);
  }
  layer_destroy(s_title_pane_layer);
}

static void init(void) {
  s_font_title = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_AQUARIUS_BOLD_20));   // keeping the title as a font until I figure out for good what this thing should be named
  s_font_menu = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_AQUARIUS_MONO_13));    
  s_font_mono_small = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_PUBLICPIXEL_8)); // font for high score page
  s_font_mono_big = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_PUBLICPIXEL_16));  // font for high score name input

  if(!persist_exists(GAME_SETTINGS_KEY)){
    game_settings.set_drop_shadow = true;
    game_settings.set_counterclockwise = false;
    game_settings.set_backlight = 0;
    game_settings.set_theme = 0;
  } else {
    persist_read_data(GAME_SETTINGS_KEY, &game_settings, sizeof(GameSettings));
  }

  set_theme(game_settings.set_theme);
  if(game_settings.set_backlight){
    light_enable(true);
  }

  s_window = window_create();
  window_set_click_config_provider(s_window, click_config_provider);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .appear = window_appear,
    .unload = window_unload,
  });
  const bool animated = true;
  window_stack_push(s_window, animated);
}

static void deinit(void) {
  window_destroy(s_window);
}

// --------------------------- //
// ****** BASE APP LOOP ****** //
// --------------------------- //

int main(void) {
  init();

  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p", s_window);

  app_event_loop();
  deinit();
}
