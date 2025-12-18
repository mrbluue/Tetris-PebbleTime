#include <pebble.h>
#include <inttypes.h>

#include "helpers.h"
#include "score_window.h"

static Window *s_window;

static Layer     *s_name_picker_layer;
static TextLayer *s_header_layer;
static TextLayer *s_score_text_layer[MAX_SCORES_SHOWN];
static TextLayer *s_bad_score_text_layer;

static GPath     *s_selector_path;

static GameScore s_game_scores[MAX_SCORES_SHOWN];
static char s_game_score_strings[MAX_SCORES_SHOWN][17];
static char s_game_details_strings[MAX_SCORES_SHOWN][17];
static char s_bad_score_string[17];
static int  s_scores_exist_bool = false;

static uint32_t s_new_score;
static uint8_t s_new_score_level;
static char s_new_score_name[4] = "AAA\0";
static int  s_current_char = 0;
static int  s_new_score_pos = MAX_SCORES_SHOWN;

static bool s_showing_details = false;

static void prv_score_window_push();
static void prv_window_load(Window *window);
static void prv_window_unload(Window *window);
static void prv_draw_name_select(Layer *layer, GContext *ctx);

static void prv_click_config_provider(void *context);

static void prv_load_scores();
// static void prv_test_scores();
static void prv_add_new_score();
static char * prv_get_score_string(int i);
static char * prv_get_details_string(int i);

static void prv_show_scores();
static void prv_show_details();

// -------------------------- //
// **** WINDOW FUNCTIONS **** //
// -------------------------- //

void new_score_window_push(uint32_t new_score, uint8_t level) {
  prv_load_scores();
  // prv_test_scores();
  prv_score_window_push();
  s_new_score = new_score;
  s_new_score_level = level;
  s_current_char = 0;
  s_new_score_pos = MAX_SCORES_SHOWN;
  if(s_new_score == 0){
    layer_set_hidden(s_name_picker_layer, true);
    return;
  }
  persist_read_string(GAME_NAME_KEY, s_new_score_name, 4);
}

void all_scores_window_push() {
  prv_load_scores();
  s_new_score_pos = MAX_SCORES_SHOWN;
  prv_score_window_push();
  layer_set_hidden(s_name_picker_layer, true);
}

void prv_score_window_push() {
  s_window = window_create();
	window_set_window_handlers(s_window, (WindowHandlers) {
		.load = prv_window_load,
		.unload = prv_window_unload,
	});
  window_set_click_config_provider(s_window, prv_click_config_provider);
  const bool animated = true;
	window_stack_push(s_window, animated);
}

static void prv_window_load(Window *window){
  Layer *window_layer = window_get_root_layer(window);

  window_set_background_color(window, theme.window_bg_color);

  GRect bounds = layer_get_bounds(window_layer);
  int16_t bounds_width = bounds.size.w;
  int16_t bounds_height = bounds.size.h;

  s_header_layer = text_layer_create(GRect(0, 16, bounds_width, 39));
  text_layer_set_text(s_header_layer, PBL_IF_ROUND_ELSE("HIGH\nSCORES", "HIGH SCORE"));
  text_layer_set_font(s_header_layer, s_font_menu);
  text_layer_set_text_alignment(s_header_layer, GTextAlignmentCenter);
  text_layer_set_background_color(s_header_layer, GColorClear);
  text_layer_set_text_color(s_header_layer, theme.window_header_color);
  layer_add_child(window_layer, text_layer_get_layer(s_header_layer));
  
  for (int i=0; i<MAX_SCORES_SHOWN; i++){
    s_score_text_layer[i] = text_layer_create(GRect(0, SCORES_START_Y + i * SCORE_LABEL_HEIGHT, bounds_width, SCORE_LABEL_HEIGHT));
    text_layer_set_font(s_score_text_layer[i], s_font_mono);
    text_layer_set_text_alignment(s_score_text_layer[i], GTextAlignmentCenter);
    text_layer_set_background_color(s_score_text_layer[i], GColorClear);
    text_layer_set_text_color(s_score_text_layer[i], theme.window_header_color);
    if(s_game_scores[i].score)
      text_layer_set_text(s_score_text_layer[i], prv_get_score_string(i));
    else if(!s_game_scores[0].score && i == MAX_SCORES_SHOWN/2-1)
      text_layer_set_text(s_score_text_layer[i], "No scores yet");
    else
      text_layer_set_text(s_score_text_layer[i], "-");

    layer_add_child(window_layer, text_layer_get_layer(s_score_text_layer[i]));
  }

  s_bad_score_text_layer = text_layer_create(GRect(0, SCORES_START_Y + MAX_SCORES_SHOWN * SCORE_LABEL_HEIGHT + 4, bounds_width, SCORE_LABEL_HEIGHT));
  text_layer_set_font(s_bad_score_text_layer, s_font_mono);
  text_layer_set_text_alignment(s_bad_score_text_layer, GTextAlignmentCenter);
  text_layer_set_background_color(s_bad_score_text_layer, GColorClear);
  text_layer_set_text_color(s_bad_score_text_layer, PBL_IF_COLOR_ELSE(theme.score_accent_color, theme.window_header_color));

  s_name_picker_layer = layer_create(GRect(0, 0, bounds_width, bounds_height));
  layer_set_update_proc(s_name_picker_layer, prv_draw_name_select);
  layer_add_child(window_layer, s_name_picker_layer);
}

static void prv_window_unload(Window *window){
  layer_destroy(s_name_picker_layer);

  text_layer_destroy(s_header_layer);
  if(s_scores_exist_bool){
    for (int i=0; i<MAX_SCORES_SHOWN; i++){
      text_layer_destroy(s_score_text_layer[i]);
    }
  }
  text_layer_destroy(s_bad_score_text_layer);
  
  gpath_destroy(s_selector_path);
}

// ------------------------ //
// **** DRAW FUNCTIONS **** //
// ------------------------ //

static void prv_draw_name_select(Layer *layer, GContext *ctx) {
  // BACKGROUND
  graphics_context_set_fill_color(ctx, theme.window_bg_color);
  graphics_fill_rect(ctx, GRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT), 0, GCornerNone);

  GRect dialog_box = GRect(PBL_IF_ROUND_ELSE(24, 8), 45, SCREEN_WIDTH - PBL_IF_ROUND_ELSE(48, 16), SCREEN_HEIGHT - 90);
  
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, dialog_box, 0, GCornerNone);
  graphics_draw_rect(ctx, dialog_box);

  // LETTERS INPUT
  GRect let1_box = GRect(SCREEN_WIDTH/2 - (INPUT_FONT_SIZE * 2.5), SCREEN_HEIGHT/2 - (INPUT_FONT_SIZE * 0.5) + 5, INPUT_FONT_SIZE, INPUT_FONT_SIZE);
  GRect let2_box = GRect(SCREEN_WIDTH/2 - (INPUT_FONT_SIZE * 0.5), SCREEN_HEIGHT/2 - (INPUT_FONT_SIZE * 0.5) + 5, INPUT_FONT_SIZE, INPUT_FONT_SIZE);
  GRect let3_box = GRect(SCREEN_WIDTH/2 + (INPUT_FONT_SIZE * 1.5), SCREEN_HEIGHT/2 - (INPUT_FONT_SIZE * 0.5) + 5, INPUT_FONT_SIZE, INPUT_FONT_SIZE);

  char let1[2] = { s_new_score_name[0], '\0' };
  char let2[2] = { s_new_score_name[1], '\0' };
  char let3[2] = { s_new_score_name[2], '\0' };

  graphics_draw_text(ctx, let1, s_font_mono_big, let1_box, GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
  graphics_draw_text(ctx, let2, s_font_mono_big, let2_box, GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
  graphics_draw_text(ctx, let3, s_font_mono_big, let3_box, GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
  
  // ARROWS
  graphics_context_set_fill_color(ctx, GColorWhite);
  
  // ^ UP arrow
  GPoint selector[3];
  int x_off = SCREEN_WIDTH / 2 - (INPUT_FONT_SIZE * 2) - 2 + s_current_char * (INPUT_FONT_SIZE * 2);
  int y_off = SCREEN_HEIGHT / 2 - INPUT_FONT_SIZE + 5;
  selector[0] = GPoint(x_off - 6, y_off); 
  selector[1] = GPoint(x_off + 6, y_off);
  selector[2] = GPoint(x_off,     y_off - 12);
  GPathInfo selector_path_info_up = { 3, selector };
  s_selector_path = gpath_create(&selector_path_info_up);
  gpath_draw_filled(ctx, s_selector_path);
  
  // V DOWN arrow
  y_off = SCREEN_HEIGHT/2 + INPUT_FONT_SIZE + 5;
  selector[0] = GPoint(x_off - 6, y_off); 
  selector[1] = GPoint(x_off + 6, y_off);
  selector[2] = GPoint(x_off,     y_off + 12);
  GPathInfo selector_path_info_down = { 3, selector };
  s_selector_path = gpath_create(&selector_path_info_down);
  gpath_draw_filled(ctx, s_selector_path);
  
  // HEADER TITLE
  GRect header = GRect(NAME_INPUT_PAD, 50, SCREEN_WIDTH - (NAME_INPUT_PAD*2), 16);
  graphics_draw_text(ctx, "ENTER YOUR NAME", s_font_mono, header, GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
}

// -------------------------- //
// ***** CLICK HANDLERS ***** //
// -------------------------- //

static void prv_back_click_handler(ClickRecognizerRef recognizer, void *context) {
  if(!layer_get_hidden(s_name_picker_layer)){
    if(s_current_char > 0)
      s_current_char--;
    else
      return;
    layer_mark_dirty(s_name_picker_layer);
  } else {
    window_stack_pop(true);
  }
}

static void prv_select_click_handler(ClickRecognizerRef recognizer, void *context) {
  if(!layer_get_hidden(s_name_picker_layer)){
    if(s_current_char == 2) {
      persist_write_string(GAME_NAME_KEY, s_new_score_name);
      prv_add_new_score(s_new_score);
      layer_set_hidden(s_name_picker_layer, true);
    } else if(s_current_char < 2)
      s_current_char++;
    else
      s_current_char = 0;

    layer_mark_dirty(s_name_picker_layer);
  } else {
    if(s_showing_details){
      s_showing_details = false;
      prv_show_scores();
    } else {
      s_showing_details = true;
      prv_show_details();
    }
  }
}

static void prv_up_click_handler(ClickRecognizerRef recognizer, void *context) {
  if(layer_get_hidden(s_name_picker_layer)) return;

  char *c = &s_new_score_name[s_current_char];
  (*c) -= 1;
  if(*c > 'Z') *c = 'A';
  else if (*c < 'A') *c = 'Z';
  layer_mark_dirty(s_name_picker_layer);
}

static void prv_down_click_handler(ClickRecognizerRef recognizer, void *context) {
  if(layer_get_hidden(s_name_picker_layer)) return;

  char *c = &s_new_score_name[s_current_char];
  (*c) += 1;
  if(*c > 'Z') *c = 'A';
  else if (*c < 'A') *c = 'Z';
  layer_mark_dirty(s_name_picker_layer);
}

static void prv_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, prv_up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, prv_down_click_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select_click_handler);
  window_single_click_subscribe(BUTTON_ID_BACK, prv_back_click_handler);
}

// ------------------------ //
// **** DATA FUNCTIONS **** //
// ------------------------ //

// static void prv_test_scores(){
//   uint16_t random = (rand() % 100);
//   uint32_t test_score = random * 1000;
//   APP_LOG(APP_LOG_LEVEL_DEBUG, "test score : %d", test_score);
//   for (int i=0; i<MAX_SCORES_SHOWN; i++){
//     strcpy(s_game_scores[i].name, "VAL");
//     test_score -= random * 100;
//     s_game_scores[i].score = test_score;
//     // snprintf(s_game_score_strings[i], sizeof(s_game_score_strings[i]), "%s - %6"PRIu32"", s_game_scores[i].name, s_game_scores[i].score);
//     APP_LOG(APP_LOG_LEVEL_DEBUG, "score %d : %d", i, s_game_scores[i].score);
//   }
// }

static void prv_load_scores(){
  if (persist_exists(GAME_SCORES_KEY)){
    APP_LOG(APP_LOG_LEVEL_INFO, "Reading saved score data");
    persist_read_data(GAME_SCORES_KEY, &s_game_scores, sizeof(s_game_scores));
    s_scores_exist_bool = true;
  } else {
    APP_LOG(APP_LOG_LEVEL_INFO, "Found no saved score data");
  }
}

static void prv_show_scores(){
  for (int i=0; i<MAX_SCORES_SHOWN; i++){
    if(s_game_scores[i].score){
      text_layer_set_text(s_score_text_layer[i], prv_get_score_string(i));
      #ifdef PBL_COLOR
        if(i == s_new_score_pos) {
          text_layer_set_text_color(s_score_text_layer[i], theme.score_accent_color);
        }
      #endif
    } else 
      text_layer_set_text(s_score_text_layer[i], "-");
  }
}

static void prv_show_details(){
  for (int i=0; i<MAX_SCORES_SHOWN; i++){
    if(s_game_scores[i].score){
      text_layer_set_text(s_score_text_layer[i], prv_get_details_string(i));
    } else 
      text_layer_set_text(s_score_text_layer[i], "-");
  }
}

static void prv_add_new_score(){
  if(!s_new_score) { return; }

  s_scores_exist_bool = true;

  int pos = MAX_SCORES_SHOWN; 
  for(int i=0; i<MAX_SCORES_SHOWN; i++){
    if (s_new_score > s_game_scores[i].score) {
      pos = i;
      break;
    }
  }

  // If it’s lower than all existing scores, show it at the bottom and stop there
  if (pos == MAX_SCORES_SHOWN) {
    snprintf(s_bad_score_string, sizeof(s_bad_score_string), "X.%s-%06"PRIu32"", s_new_score_name, s_new_score);
    text_layer_set_text(s_bad_score_text_layer, s_bad_score_string);
    layer_add_child(window_get_root_layer(s_window), text_layer_get_layer(s_bad_score_text_layer));
    return;
  }

  // Shift lower scores down to make space (dropping the last one)
  for (int i = MAX_SCORES_SHOWN - 1; i > pos; i--) {
    s_game_scores[i] = s_game_scores[i - 1];
  }

  snprintf(s_game_scores[pos].name, sizeof(s_game_scores[pos].name), s_new_score_name);

  s_game_scores[pos].score = s_new_score;
  s_game_scores[pos].level = s_new_score_level;
  
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  strftime(s_game_scores[pos].date, sizeof(s_game_scores[pos].date), "%y/%m/%d", t);

  s_new_score_pos = pos;

  persist_write_data(GAME_SCORES_KEY, &s_game_scores, sizeof(s_game_scores));
  prv_show_scores();
}

static char * prv_get_score_string(int i){
  snprintf(s_game_score_strings[i], sizeof(s_game_score_strings[i]), "%d.%s-%06"PRIu32"", i+1, s_game_scores[i].name, s_game_scores[i].score);
  // APP_LOG(APP_LOG_LEVEL_DEBUG, "score %d : %s", i, s_game_score_strings[i]);
  return s_game_score_strings[i];
}

static char * prv_get_details_string(int i){
  snprintf(s_game_details_strings[i], sizeof(s_game_details_strings[i]), "%d.LV.%02d-%s", i+1, s_game_scores[i].level, s_game_scores[i].date);
  return s_game_details_strings[i];
}