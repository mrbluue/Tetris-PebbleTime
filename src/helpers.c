#include "helpers.h"

/* simple base 10 only itoa */
// Credit: David C. Rankin, Stackoverflow
// Because seriously, the Pebble SDK has no way to do this.
char *itoa10 (int value, char *result)
{
    char const digit[] = "0123456789";
    char *p = result;
    if (value < 0) {
        *p++ = '-';
        value *= -1;
    }

    /* move number of required chars and null terminate */
    int shift = value;
    do {
        ++p;
        shift /= 10;
    } while (shift);
    *p = '\0';

    /* populate result in reverse order */
    do {
        *--p = digit [value % 10];
        value /= 10;
    } while (value);

    return result;
}

void update_num_layer (int num, char* str, TextLayer *layer) {
  itoa10(num, str);
  text_layer_set_text(layer, str);
}

const GPoint SHAPES[7][4] = {
  { 
    // X  Y        * = pivot point at x = 0 & y = 0 at index [0] (part of shape that never rotates)
    {  0, 0 },  // SQUARE
    { -1, 0 },  // |2|3 |
    { -1, 1 },  // |1|0*|
    {  0, 1 }
  },
  { 
    {  0, 0 },
    { -2, 0 },  // LINE
    { -1, 0 },  // |1|2|0*|3|
    {  1, 0 }   
  },
  {
    {  0, 0 }, // J
    { -1, 1 }, // |1|
    { -1, 0 }, // |2|0*|3|
    {  1, 0 }  
  },
  {
    {  0, 0 }, // L
    { -1, 0 }, //      |3|
    {  1, 0 }, // |1|0*|2|
    {  1, 1 }
  },
  {
    {  0, 0 }, // S
    {  1, 1 }, //   |2 |1|
    {  0, 1 }, // |3|0*|
    { -1, 0 }
  },
  {
    {  0, 0 }, // Z 
    { -1, 1 }, // |1|2 |
    {  0, 1 }, //   |0*|3|
    {  1, 0 }
  },
  {
    {  0, 0 }, // T
    { -1, 0 }, //   |3 |   
    {  1, 0 }, // |1|0*|2|
    {  0, 1 }
  }

};

void make_block (GPoint *create_block, int type, int bX, int bY) {
  for (int i = 0; i < 4; i++) {
    create_block[i] = GPoint(bX + SHAPES[type][i].x, bY + SHAPES[type][i].y);
  }
}

GPoint rotate_point (int old_x, int old_y, int rotation) {
  
  int new_x, new_y;

  switch (rotation % 4) {
    case 0: new_x =  old_x; new_y =  old_y; break; // initial state
    case 1: new_x = -old_y; new_y =  old_x; break;
    case 2: new_x = -old_x; new_y = -old_y; break;
    case 3: new_x =  old_y; new_y = -old_x; break;
    default: new_x = old_x; new_y =  old_y;
  } 

  return GPoint(new_x, new_y);

}

// Rotate the current block.
void rotate_block (GPoint *new_block, GPoint *old_block, int block_type, int rotation) {

  if (block_type == SQUARE) {
    for (int i = 0; i < 4; i++) {
      new_block[i] = old_block[i];
    }
    return;
  }

  if (block_type == LINE) {
    rotation %= 2; 
  }

  GPoint pivot_point = old_block[0];
  new_block[0] = pivot_point;
  for(int i=1; i<4; i++){
    GPoint new_coord = rotate_point(SHAPES[block_type][i].x, SHAPES[block_type][i].y, rotation);
    new_block[i] = GPoint(pivot_point.x+new_coord.x, pivot_point.y+new_coord.y);
  }

}

int find_max_drop (GPoint *block, uint8_t grid[GRID_BLOCK_WIDTH][GRID_BLOCK_HEIGHT]) {
  bool canDrop = true;
  int drop_amount = 0;

  while (canDrop) {
    for (int i=0; i<4; i++) {
      int benthic = block[i].y + 1 + drop_amount;
      if (benthic > 19 || grid[block[i].x][benthic]) { 
        canDrop = false; 
      }
    }
    if (canDrop) {
      drop_amount += 1;
    }
  }

  return drop_amount;
}

// Just to make the 'next block' display nice and centered for blocks which have even width
int next_block_offset (int block_type) {
  if (block_type == SQUARE || block_type == LINE) { return BLOCK_SIZE/2; }
  else { return 0; }
}

void set_theme(int theme_id) {
  switch (theme_id) {
  case 0:
    theme.window_bg_color         = GColorFromRGB(0, 0, 96);  // BLUE
    theme.window_header_color     = GColorWhite;
    theme.window_label_text_color = GColorBlack;
    theme.window_label_bg_color   = GColorWhite;
    theme.window_label_bg_inactive_color = GColorLightGray;
    theme.grid_bg_color           = GColorBlack;
    theme.grid_lines_color        = GColorFromRGB(64, 0, 64);

    theme.block_color[SQUARE] = GColorFromRGB(196, 0, 0);     // RED
    theme.block_color[LINE]   = GColorFromRGB(0, 170, 0);     // GREEN
    theme.block_color[J]      = GColorFromRGB(0, 0, 196);     // BLUE
    theme.block_color[L]      = GColorFromRGB(255, 85, 0);    // YELLOW
    theme.block_color[S]      = GColorFromRGB(196, 0, 196);   // PURPLE
    theme.block_color[Z]      = GColorFromRGB(0, 196, 196);   // CYAN
    theme.block_color[T]      = GColorFromRGB(196, 196, 196); // WHITE

    theme.block_border_color  = GColorBlack;
    theme.drop_shadow_color   = GColorFromRGB(96, 96, 96);
    theme.select_color        = GColorBlack;
    theme.score_accent_color  = GColorPastelYellow;
    break;
  case 1:
    theme.window_bg_color         = GColorFromRGB(0, 96, 0);  // GREEN
    theme.window_header_color     = GColorWhite;
    theme.window_label_text_color = GColorWhite;
    theme.window_label_bg_color   = GColorBlack;
    theme.window_label_bg_inactive_color = GColorBlack;
    theme.grid_bg_color           = GColorWhite;
    theme.grid_lines_color        = GColorCeleste;

    theme.block_color[SQUARE] = GColorFromRGB(196, 0, 0);     // RED
    theme.block_color[LINE]   = GColorFromRGB(0, 170, 0);     // GREEN
    theme.block_color[J]      = GColorFromRGB(0, 0, 196);     // BLUE
    theme.block_color[L]      = GColorFromRGB(255, 85, 0);    // ORANGE
    theme.block_color[S]      = GColorFromRGB(196, 0, 196);   // PURPLE
    theme.block_color[Z]      = GColorFromRGB(0, 196, 196);   // CYAN
    theme.block_color[T]      = GColorChromeYellow; // YELLOW

    theme.block_border_color  = GColorBlack;
    theme.drop_shadow_color   = GColorFromRGB(96, 96, 96);
    theme.select_color        = GColorWhite;
    theme.score_accent_color  = GColorRichBrilliantLavender;
    break;
  case 2:
    theme.window_bg_color         = GColorFromRGB(96, 0, 0);  // RED
    theme.window_header_color     = GColorWhite;
    theme.window_label_text_color = GColorWhite;
    theme.window_label_bg_color   = GColorBlack;
    theme.window_label_bg_inactive_color = GColorBlack;
    theme.grid_bg_color           = GColorWhite;
    theme.grid_lines_color        = GColorCeleste;

    theme.block_color[SQUARE] = GColorFromRGB(196, 0, 0);     // RED
    theme.block_color[LINE]   = GColorFromRGB(0, 170, 0);     // GREEN
    theme.block_color[J]      = GColorFromRGB(0, 0, 196);     // BLUE
    theme.block_color[L]      = GColorFromRGB(255, 85, 0);    // ORANGE
    theme.block_color[S]      = GColorFromRGB(196, 0, 196);   // PURPLE
    theme.block_color[Z]      = GColorFromRGB(0, 196, 196);   // CYAN
    theme.block_color[T]      = GColorChromeYellow; // YELLOW

    theme.block_border_color  = GColorBlack;
    theme.drop_shadow_color   = GColorFromRGB(96, 96, 96);
    theme.select_color        = GColorWhite;
    theme.score_accent_color  = GColorCeleste;
    break;
  default:
    break;
  }
}
