#include <pebble.h>

#include "helpers.h"
#include "settings_window.h"

#define SETTINGS_COUNT 4
#define SETTINGS_LABEL_HEIGHT 30

#define SETTINGS_LABEL_PAD_L 14
#define SETTINGS_LABEL_TOP_Y 48
#define SETTINGS_INPUT_PAD_R 10
#define SETTINGS_INPUT_TOP_Y 52

static Window *s_window;

static Layer     *s_select_layer;
static Layer     *s_theme_preview_layer;
static TextLayer *s_header_layer;
static TextLayer *s_settings_label_layer[SETTINGS_COUNT];
static TextLayer *s_settings_input_layer[SETTINGS_COUNT];

static GPath     *s_selector_path; // arrow for menu select

static char *MENU_OPTION_LABELS[SETTINGS_COUNT] = {"Drop Shadows", "Rotation", "Backlight", "Theme"};
static char *MENU_INPUT_LABELS[SETTINGS_COUNT][THEMES_COUNT] = {{"OFF", "ON"}, {"Clockwise", "Cnt. Clock."}, {"Default", "Alw. ON"}, {"< 1 >", "< 2 >", "< 3 >", "< 4 >"}};
static int   MENU_INPUT_VALUES[SETTINGS_COUNT] = {0, 0, 0, 0};
#ifdef PBL_PLATFORM_CHALK
  static int MENU_OPTION_ROUND_OFFSET[SETTINGS_COUNT] = {8, 4, 12, 34};
#endif

static int current_setting = 0;

static AppTimer *s_timer = NULL;

static void prv_window_load(Window *window);
static void prv_window_unload(Window *window);

static void prv_draw_select(Layer *layer, GContext *ctx);
static void prv_draw_theme_preview(Layer *layer, GContext *ctx);

static void prv_click_config_provider(void *context);

static void prv_preview_hide_tick(void *data);

static void prv_load_settings();
static void prv_save_settings();

// -------------------------- //
// **** WINDOW FUNCTIONS **** //
// -------------------------- //

void settings_window_push() {
  s_window = window_create();
  window_set_click_config_provider(s_window, prv_click_config_provider);
	window_set_window_handlers(s_window, (WindowHandlers) {
		.load = prv_window_load,
		.unload = prv_window_unload,
	});
  const bool animated = true;
	window_stack_push(s_window, animated);
  prv_load_settings();
}

static void prv_window_load(Window *window){
  Layer *window_layer = window_get_root_layer(window);

  GRect bounds = layer_get_bounds(window_layer);
  int16_t bounds_width = bounds.size.w;
  int16_t bounds_height = bounds.size.h;

  s_select_layer = layer_create(GRect(0, 0, bounds_width, bounds_height));
  layer_set_update_proc(s_select_layer, prv_draw_select);
  layer_add_child(window_layer, s_select_layer);

  s_header_layer = text_layer_create(GRect(0, 10, bounds_width, 20));
  text_layer_set_text(s_header_layer, "Settings");
  text_layer_set_text_alignment(s_header_layer, GTextAlignmentCenter);
  text_layer_set_background_color(s_header_layer, GColorClear);
  text_layer_set_text_color(s_header_layer, theme.window_header_color);
  layer_add_child(window_layer, text_layer_get_layer(s_header_layer));
  
  for (int i=0; i<SETTINGS_COUNT; i++){
    s_settings_label_layer[i] = text_layer_create(
      GRect(
        PBL_IF_RECT_ELSE(
          SETTINGS_LABEL_PAD_L, 
          SETTINGS_LABEL_PAD_L + MENU_OPTION_ROUND_OFFSET[i]
        ),
        SETTINGS_LABEL_TOP_Y + i * SETTINGS_LABEL_HEIGHT, 
        bounds_width, 
        SETTINGS_LABEL_HEIGHT
      ));
    text_layer_set_text(s_settings_label_layer[i], MENU_OPTION_LABELS[i]);
    text_layer_set_font(s_settings_label_layer[i], fonts_get_system_font(FONT_KEY_GOTHIC_18));
    text_layer_set_background_color(s_settings_label_layer[i], GColorClear);
    text_layer_set_text_color(s_settings_label_layer[i], theme.window_label_text_color);
    layer_add_child(window_layer, text_layer_get_layer(s_settings_label_layer[i]));

    s_settings_input_layer[i] = text_layer_create(
      GRect(
        0, 
        SETTINGS_INPUT_TOP_Y + i * SETTINGS_LABEL_HEIGHT, 
        PBL_IF_RECT_ELSE(
          bounds_width - SETTINGS_INPUT_PAD_R, 
          bounds_width - (SETTINGS_INPUT_PAD_R + MENU_OPTION_ROUND_OFFSET[i])
        ), 
        SETTINGS_LABEL_HEIGHT
      ));
    text_layer_set_text(s_settings_input_layer[i], MENU_INPUT_LABELS[i][0]);
    text_layer_set_text_alignment(s_settings_input_layer[i], GTextAlignmentRight);
    text_layer_set_font(s_settings_input_layer[i], fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD));
    text_layer_set_background_color(s_settings_input_layer[i], GColorClear);
    text_layer_set_text_color(s_settings_input_layer[i], theme.window_label_text_color);
    layer_add_child(window_layer, text_layer_get_layer(s_settings_input_layer[i]));
  }

  s_theme_preview_layer = layer_create(GRect(0, 0, bounds_width, bounds_height));
  layer_set_update_proc(s_theme_preview_layer, prv_draw_theme_preview);
  layer_add_child(window_layer, s_theme_preview_layer);
  layer_set_hidden(s_theme_preview_layer, true);
}

static void prv_window_unload(Window *window){
  text_layer_destroy(s_header_layer);
  layer_destroy(s_select_layer);
  layer_destroy(s_theme_preview_layer);
  for (int i=0; i<SETTINGS_COUNT; i++){
    text_layer_destroy(s_settings_label_layer[i]);
    text_layer_destroy(s_settings_input_layer[i]);
  }
  gpath_destroy(s_selector_path);
}

// ------------------------ //
// **** DRAW FUNCTIONS **** //
// ------------------------ //

static void prv_draw_select(Layer *layer, GContext *ctx){
  graphics_context_set_fill_color(ctx, theme.window_bg_color); // this instead of window_set_background_color so that it refreshes when changing theme
  graphics_fill_rect(ctx, GRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT), 0, GCornerNone);

  graphics_context_set_fill_color(ctx, theme.window_label_bg_color);
  if(s_timer != NULL){
    graphics_context_set_fill_color(ctx, theme.window_label_bg_inactive_color);
  }
  for (int i=0; i<SETTINGS_COUNT; i++){
    graphics_fill_rect(ctx, GRect(0, 50 + i * SETTINGS_LABEL_HEIGHT, SCREEN_WIDTH, 22), 0, GCornerNone);
  }

  graphics_context_set_fill_color(ctx, theme.select_color);
  GPoint selector[3];
  int x_off = -6;
  int y_off = current_setting * SETTINGS_LABEL_HEIGHT;

  #ifdef PBL_PLATFORM_CHALK
  x_off += MENU_OPTION_ROUND_OFFSET[current_setting] - 2;
  #endif

  selector[0] = GPoint(x_off + 10, 57 + y_off);
  selector[1] = GPoint(x_off + 10, 65 + y_off);
  selector[2] = GPoint(x_off + 18, 61 + y_off);
  GPathInfo selector_path_info = { 3, selector };
  s_selector_path = gpath_create(&selector_path_info);
  gpath_draw_filled(ctx, s_selector_path);
}

static void prv_draw_theme_preview(Layer *layer, GContext *ctx){
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_context_set_fill_color(ctx, theme.grid_bg_color);
  GRect background = GRect(PBL_IF_ROUND_ELSE(24, 9), 45, SCREEN_WIDTH - PBL_IF_ROUND_ELSE(48, 18), 3 * SETTINGS_LABEL_HEIGHT);
  graphics_fill_rect(ctx, background, 0, GCornerNone);
  graphics_draw_rect(ctx, background);

  for(int i=0; i<7; i++){
    GPoint block[4];
    int block_x = 2 * i - ((i+1) % 2);
    if(i==0){block_x = 0;}
    int block_y = 3 + 4 * (i % 2);
    make_block(block, i, block_x, block_y);
    graphics_context_set_fill_color(ctx, theme.block_color[i]);
    for(int i=0; i<4; i++){
      graphics_fill_rect(ctx, GRect(PBL_IF_ROUND_ELSE(42, 24) + block[i].x * 8, 53 + block[i].y * 8, 8, 8), 0, GCornerNone);
    } 
  }

  graphics_context_set_stroke_color(ctx, theme.grid_lines_color);
  // Draw vertical lines
  for (int i=PBL_IF_ROUND_ELSE(25, 8)+BLOCK_SIZE; i<=(SCREEN_WIDTH - PBL_IF_ROUND_ELSE(24, 9)); i+=BLOCK_SIZE) {
    graphics_draw_line(ctx, GPoint(i, 46), GPoint(i, 43 + 3 * SETTINGS_LABEL_HEIGHT)); 
  }
  // Draw horizontal lines
  for (int i=45+BLOCK_SIZE; i<=45+(3 * SETTINGS_LABEL_HEIGHT); i+=BLOCK_SIZE) {
    graphics_draw_line(ctx, GPoint(PBL_IF_ROUND_ELSE(25, 10), i), GPoint(SCREEN_WIDTH - PBL_IF_ROUND_ELSE(26, 11), i));
  }

}

// -------------------------- //
// ***** CLICK HANDLERS ***** //
// -------------------------- //

static void prv_up_click_handler(ClickRecognizerRef recognizer, void *context) {
  if(current_setting > 0) 
    current_setting -= 1;
  else
    current_setting = SETTINGS_COUNT - 1;
  
  layer_mark_dirty(s_select_layer);
}

static void prv_select_click_handler(ClickRecognizerRef recognizer, void *context) {
  int *value_to_change = &MENU_INPUT_VALUES[current_setting];

  if(current_setting < 3){
    *value_to_change = (*value_to_change + 1) % 2;
  } else {
    *value_to_change = (*value_to_change + 1) % THEMES_COUNT;
  }

  if(current_setting == 2){
    light_enable(*value_to_change%2);
  }

  if(current_setting == 3){
    layer_set_hidden(s_theme_preview_layer, false);
    if(s_timer == NULL){
      s_timer = app_timer_register(1500, prv_preview_hide_tick, NULL);
    } else {
      app_timer_reschedule(s_timer, 1500);
    }
    set_theme(*value_to_change); 
    text_layer_set_text_color(s_header_layer, theme.window_header_color);
    for (int i=0; i<SETTINGS_COUNT; i++){
      text_layer_set_text_color(s_settings_label_layer[i], theme.window_label_text_color);
      text_layer_set_text_color(s_settings_input_layer[i], theme.window_label_text_color);
    }
  }

  text_layer_set_text(s_settings_input_layer[current_setting], MENU_INPUT_LABELS[current_setting][*value_to_change]);
  // APP_LOG(APP_LOG_LEVEL_INFO, "current setting %d, value to change %d, input label = %s", current_setting, *value_to_change, MENU_INPUT_LABELS[current_setting][*value_to_change]);
}

static void prv_down_click_handler(ClickRecognizerRef recognizer, void *context) {
  if(current_setting < SETTINGS_COUNT - 1) 
    current_setting += 1;
  else
    current_setting = 0;
  layer_mark_dirty(s_select_layer);
}

static void prv_back_click_handler(ClickRecognizerRef recognizer, void *context) {
  prv_save_settings();
  s_timer = NULL;
  window_stack_pop(true);
}

static void prv_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, prv_up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, prv_down_click_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select_click_handler);
  window_single_click_subscribe(BUTTON_ID_BACK, prv_back_click_handler);
}

// -------------------------- //
// ***** TICK FUNCTIONS ***** //
// -------------------------- //

static void prv_preview_hide_tick(void *data){
  layer_set_hidden(s_theme_preview_layer, true);
  s_timer = NULL;
}

// -------------------------- //
// ***** DATA FUNCTIONS ***** //
// -------------------------- //

static void prv_load_settings() {
  MENU_INPUT_VALUES[0] = game_settings.set_drop_shadow % 2;
  MENU_INPUT_VALUES[1] = game_settings.set_counterclockwise % 2;
  MENU_INPUT_VALUES[2] = game_settings.set_backlight % 2;
  MENU_INPUT_VALUES[3] = game_settings.set_theme % THEMES_COUNT;
  
  for(int i=0; i<SETTINGS_COUNT; i++) {
    text_layer_set_text(s_settings_input_layer[i], MENU_INPUT_LABELS[i][MENU_INPUT_VALUES[i]]);
  }
}

static void prv_save_settings() {
  game_settings.set_drop_shadow = MENU_INPUT_VALUES[0] % 2;
  game_settings.set_counterclockwise = MENU_INPUT_VALUES[1] % 2;
  game_settings.set_backlight = MENU_INPUT_VALUES[2] % 2;
  game_settings.set_theme = MENU_INPUT_VALUES[3] % THEMES_COUNT;
  persist_write_data(GAME_SETTINGS_KEY, &game_settings, sizeof(GameSettings));
}