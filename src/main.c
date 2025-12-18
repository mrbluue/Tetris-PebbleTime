/* Copyright (c) 2015, WRansohoff
 * MIT License, see LICENSE file for details.
 */
#include <pebble.h>

#include "helpers.h"
#include "game_window.h"
#include "score_window.h"
#include "settings_window.h"

#define MENU_OPTIONS 4 

#ifdef PBL_PLATFORM_EMERY
  #define MENU_TITLE_X 13
  #define MENU_TITLE_Y 18
  #define MENU_TITLE_W 174
  #define MENU_TITLE_H 19
  
  #define MENU_GRID_BLOCK_SIZE 18
  #define MENU_GRID_COLS 10
  #define MENU_GRID_ROWS 9
  #define MENU_GRID_STROKE 2
  #define MENU_GRID_X_OFFSET 9
  #define MENU_GRID_Y 48

  #define MENU_OPTIONS_X 0
  #define MENU_OPTIONS_Y (MENU_GRID_Y + MENU_GRID_BLOCK_SIZE + MENU_GRID_STROKE) - 1
  #define MENU_OPTIONS_H 17
#elif defined(PBL_PLATFORM_CHALK)
  #define MENU_TITLE_X 33
  #define MENU_TITLE_Y 28
  #define MENU_TITLE_W 118
  #define MENU_TITLE_H 13
  
  #define MENU_GRID_BLOCK_SIZE 13
  #define MENU_GRID_COLS 10
  #define MENU_GRID_ROWS 9
  #define MENU_GRID_STROKE 2
  #define MENU_GRID_X_OFFSET 12
  #define MENU_GRID_Y 48

  #define MENU_OPTIONS_X 2
  #define MENU_OPTIONS_Y (MENU_GRID_Y + MENU_GRID_BLOCK_SIZE + MENU_GRID_STROKE) - 2
  #define MENU_OPTIONS_H 10 + 2
#else 
  #define MENU_TITLE_X 13
  #define MENU_TITLE_Y 20
  #define MENU_TITLE_W 118
  #define MENU_TITLE_H 13

  #define MENU_GRID_BLOCK_SIZE 13
  #define MENU_GRID_COLS 10
  #define MENU_GRID_ROWS 9
  #define MENU_GRID_STROKE 2
  #define MENU_GRID_X_OFFSET 7
  #define MENU_GRID_Y 44

  #define MENU_OPTIONS_X 2
  #define MENU_OPTIONS_Y (MENU_GRID_Y + MENU_GRID_BLOCK_SIZE + MENU_GRID_STROKE) - 2
  #define MENU_OPTIONS_H 10 + 2
#endif

GameSettings game_settings;
Theme theme;

GFont s_font_mono_line;
GFont s_font_mono;
GFont s_font_mono_big;
GFont s_font_menu;

static Window *s_window;

static Layer *s_title_pane_layer = NULL;

static GBitmap *s_menu_title_bitmap;
static BitmapLayer *s_menu_title_bitmap_layer;

#ifdef PBL_BW
  static GBitmap *s_menu_option_bg_bitmap;
  static BitmapLayer *s_menu_option_bg_bitmap_layer;
#endif

static char *MENU_OPTIONS_LABELS[MENU_OPTIONS] = { "CONTINUE", "NEW GAME", "HIGH SCORE", "SETTINGS" };

static TextLayer *s_menu_option_text_layer[MENU_OPTIONS];

static bool can_load = false; // is there a game to continue
static bool continue_label_showing = true; 
static int menu_option = -1;

static void find_save(){
  if (persist_exists(GAME_STATE_KEY) && persist_exists(GAME_CONTINUE_KEY)) {
    if (persist_read_bool(GAME_CONTINUE_KEY)) {
      can_load = true;
      return;
    }
  }
  if(menu_option == 0) { menu_option = 1; }
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
      // new_score_window_push(1000, 1); // for testing
      break;
    case 3:
      settings_window_push();
      break;
    default: break;
  }
}

static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
  switch(menu_option){
    case -1:
    case 0:
      menu_option = MENU_OPTIONS - 1;
      break;
    case 1:
      menu_option = can_load ? 0 : MENU_OPTIONS - 1;
      break;
    default:
      menu_option -= 1;
    break;
  }

  layer_mark_dirty(s_title_pane_layer);
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
  if((menu_option == -1) || (menu_option == MENU_OPTIONS - 1))
    menu_option = can_load ? 0 : 1;
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
    grid_origin_y += (2 * MENU_GRID_BLOCK_SIZE); // if there's no CONTINUE option, start the grid 2 blocks down
  }

  graphics_context_set_fill_color(ctx, GColorDarkGray);
  graphics_fill_rect(ctx, GRect(0, grid_origin_y, SCREEN_WIDTH, SCREEN_HEIGHT - grid_origin_y), 0, GCornerNone);

  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_context_set_stroke_width(ctx, MENU_GRID_STROKE);
    
  #ifdef PBL_COLOR
    if(menu_option >= 0){
      graphics_context_set_fill_color(ctx, theme.block_color[menu_option]);
      graphics_fill_rect(ctx, GRect(0, MENU_GRID_Y + (menu_option*2) * MENU_GRID_BLOCK_SIZE, SCREEN_WIDTH, MENU_GRID_BLOCK_SIZE * 3), 0, GCornerNone);
    }
  #endif

  // vertical lines
  for (int i=MENU_GRID_X_OFFSET; i<=SCREEN_WIDTH; i+=MENU_GRID_BLOCK_SIZE) {
    graphics_draw_line(ctx, GPoint(i, grid_origin_y), GPoint(i, SCREEN_HEIGHT)); 
  }
  // horizontal lines
  for (int i=grid_origin_y; i<=SCREEN_HEIGHT; i+=MENU_GRID_BLOCK_SIZE) {
    graphics_draw_line(ctx, GPoint(0, i), GPoint(SCREEN_WIDTH, i)); 
  }

  #ifdef PBL_BW
    if(menu_option >= 0){
      for(int i=0; i<MENU_OPTIONS; i++){
        text_layer_set_text_color(s_menu_option_text_layer[i], GColorWhite);
      }
      layer_set_hidden(bitmap_layer_get_layer(s_menu_option_bg_bitmap_layer), false);
      layer_set_frame(bitmap_layer_get_layer(s_menu_option_bg_bitmap_layer), GRect(0, MENU_GRID_Y+2 + (menu_option*2) * MENU_GRID_BLOCK_SIZE, SCREEN_WIDTH, 36));
      text_layer_set_text_color(s_menu_option_text_layer[menu_option], GColorBlack);
    }
  #endif
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

  find_save();

  menu_option = can_load ? 0 : 1;

  s_title_pane_layer = layer_create(GRect(0, 0, bounds_width, bounds_height));
  layer_set_update_proc(s_title_pane_layer, draw_title_pane);
  layer_add_child(window_layer, s_title_pane_layer);
  
  s_menu_title_bitmap = gbitmap_create_with_resource(RESOURCE_ID_MENU_TITLE);
  s_menu_title_bitmap_layer = bitmap_layer_create(GRect(MENU_TITLE_X, MENU_TITLE_Y, MENU_TITLE_W, MENU_TITLE_H));
  bitmap_layer_set_alignment(s_menu_title_bitmap_layer, GAlignBottom);
  bitmap_layer_set_compositing_mode(s_menu_title_bitmap_layer, GCompOpSet);
  bitmap_layer_set_bitmap(s_menu_title_bitmap_layer, s_menu_title_bitmap);
  layer_add_child(window_layer, bitmap_layer_get_layer(s_menu_title_bitmap_layer));

  #ifdef PBL_BW
    s_menu_option_bg_bitmap = gbitmap_create_with_resource(RESOURCE_ID_MENU_OPTION_BG_BW);
    s_menu_option_bg_bitmap_layer = bitmap_layer_create(GRect(0, MENU_GRID_Y+3+25, SCREEN_WIDTH, 36));
    bitmap_layer_set_compositing_mode(s_menu_option_bg_bitmap_layer, GCompOpSet);
    bitmap_layer_set_bitmap(s_menu_option_bg_bitmap_layer, s_menu_option_bg_bitmap);
    layer_add_child(window_layer, bitmap_layer_get_layer(s_menu_option_bg_bitmap_layer));
    layer_set_hidden(bitmap_layer_get_layer(s_menu_option_bg_bitmap_layer), true);
  #endif

  for(int i=0; i<MENU_OPTIONS; i++){
    s_menu_option_text_layer[i] = text_layer_create(GRect(MENU_OPTIONS_X, MENU_OPTIONS_Y + (i*2) * MENU_GRID_BLOCK_SIZE, SCREEN_WIDTH, MENU_OPTIONS_H));
    text_layer_set_text(s_menu_option_text_layer[i], MENU_OPTIONS_LABELS[i]);
    text_layer_set_text_alignment(s_menu_option_text_layer[i], GTextAlignmentCenter);
    text_layer_set_font(s_menu_option_text_layer[i], s_font_menu);
    text_layer_set_background_color(s_menu_option_text_layer[i], GColorClear);
    text_layer_set_text_color(s_menu_option_text_layer[i], GColorWhite);
    layer_add_child(window_layer, text_layer_get_layer(s_menu_option_text_layer[i]));
  }

}

static void window_appear(Window *window) {
  find_save();

  if(can_load && !continue_label_showing){
    layer_set_hidden(text_layer_get_layer(s_menu_option_text_layer[0]), false);
    continue_label_showing = true;
  } else if(!can_load && continue_label_showing) {
    layer_set_hidden(text_layer_get_layer(s_menu_option_text_layer[0]), true);
    continue_label_showing = false;
  }
  layer_mark_dirty(s_title_pane_layer);
}

static void window_unload(Window *window) {
  // TODO / REMINDER - Add any new objects here for destruction on exit!
  layer_destroy(s_title_pane_layer);

  bitmap_layer_destroy(s_menu_title_bitmap_layer);
  gbitmap_destroy(s_menu_title_bitmap);

  for (int i=0; i<MENU_OPTIONS; i++){
    text_layer_destroy(s_menu_option_text_layer[i]);
  }
}

static void init(void) {
  #ifdef PBL_PLATFORM_EMERY
    s_font_mono      = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_PUBLICPIXEL_11)); // high score page
    s_font_mono_line = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_PUBLICPIXEL_19)); // same with margin on top of characters
    s_font_mono_big  = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_PUBLICPIXEL_24)); // high score name input
    s_font_menu      = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_MENU_17));        // menu options
  #else
    s_font_mono      = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_PUBLICPIXEL_8));   // high score page
    s_font_mono_line = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_PUBLICPIXELS_14)); // same with margin on top of characters
    s_font_mono_big  = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_PUBLICPIXEL_16));  // high score name input
    s_font_menu      = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_MENU_12));         // menu options
  #endif

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
