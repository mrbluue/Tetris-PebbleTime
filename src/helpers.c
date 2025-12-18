#include "helpers.h"

void update_string_num_layer (char* str_in, int num, char* str_out, size_t out_size, TextLayer *layer) {
  snprintf(str_out, out_size, "%s%d", str_in, num);
  text_layer_set_text(layer, str_out);
}

const GPoint SHAPES[7][4] = {
  { 
    // Origin of the grid is in the top left
    // X  Y        * = pivot point at x = 0 & y = 0 at index [0] (part of shape that never rotates)
    {  0,  0 },  // SQUARE
    { -1,  0 },  // |2|3 |
    { -1, -1 },  // |1|0*|
    {  0, -1 }
  },
  { 
    {  0, 0 },
    { -1, 0 },  // LINE
    {  1, 0 },  // |1|0*|2|3|
    {  2, 0 }   
  },
  {
    {  0,  0 }, // J
    { -1, -1 }, // |1|
    { -1,  0 }, // |2|0*|3|
    {  1,  0 }  
  },
  {
    {  0,  0 }, // L
    { -1,  0 }, //      |3|
    {  1,  0 }, // |1|0*|2|
    {  1, -1 }
  },
  {
    {  0,  0 }, // S
    {  1, -1 }, //   |2 |1|
    {  0, -1 }, // |3|0*|
    { -1,  0 }
  },
  {
    {  0,  0 }, // Z 
    { -1, -1 }, // |1|2 |
    {  0, -1 }, //   |0*|3|
    {  1,  0 }
  },
  {
    {  0,  0 }, // T
    { -1,  0 }, //   |3 |   
    {  1,  0 }, // |1|0*|2|
    {  0, -1 }
  }

};

// offset data from SRS obtained thanks to https://harddrop.com/wiki/SRS#How_guideline_SRS_actually_works
const GPoint JLSTZ_KICKS[4][5] = { // ROTATION / OFFSET
  { // rot 0
    { 0, 0},
    { 0, 0},
    { 0, 0},
    { 0, 0},
    { 0, 0}
  },
  { // rot R
    { 0, 0},
    { 1, 0},
    { 1, 1},
    { 0,-2},
    { 1,-2}
  },
  { // rot 2
    { 0, 0},
    { 0, 0},
    { 0, 0},
    { 0, 0},
    { 0, 0}
  },
  { // rot L
    { 0, 0},
    {-1, 0},
    {-1, 1},
    { 0,-2},
    {-1,-2}
  }
};

const GPoint I_KICKS[4][5] = {
  { // rot 0
    { 0, 0},
    {-1, 0},
    { 2, 0},
    {-1, 0},
    { 2, 0}
  },
  { // rot R
    {-1, 0},
    { 0, 0},
    { 0, 0},
    { 0,-1},
    { 0, 2}
  },
  { // rot 2
    {-1,-1},
    { 1,-1},
    {-2,-1},
    { 1, 0},
    {-2, 0}
  },
  { // rot L
    { 0,-1},
    { 0,-1},
    { 0,-1},
    { 0, 1},
    { 0,-2}
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

// Rotate the current tetromino.
void rotate_mino(GPoint *new_block, GPoint *old_block, int block_type, int rotation) {
  GPoint pivot_point = old_block[0]; // the 1 block out of 4 that never rotates
  new_block[0] = pivot_point;
  
  for(int i=1; i<4; i++){
    GPoint new_coord = rotate_point(SHAPES[block_type][i].x, SHAPES[block_type][i].y, rotation);
    new_block[i] = GPoint(pivot_point.x+new_coord.x, pivot_point.y+new_coord.y);
  }

}

bool rotate_try_kicks(GPoint *new_block, GPoint *old_block, int block_type, int old_rotation, int new_rotation, bool grid[GAME_GRID_BLOCK_WIDTH][GAME_GRID_BLOCK_HEIGHT]){
  const GPoint const (*kick_registry)[4][5] = (block_type == LINE) ? &I_KICKS : &JLSTZ_KICKS;

  // look through each offset option
  for(int i=0; i<5; i++){ 
    // get temp mino from old_block
    GPoint mino_try[4];
    memcpy(mino_try, old_block, sizeof(mino_try));
    // look through each block of the temp mino
    for(int j=0; j<4; j++){ 
      // apply kick offset to each block, see https://harddrop.com/wiki/SRS#How_guideline_SRS_actually_works
      mino_try[j].x += (*kick_registry)[old_rotation][i].x - (*kick_registry)[new_rotation][i].x;
      mino_try[j].y += (*kick_registry)[old_rotation][i].y - (*kick_registry)[new_rotation][i].y;

      // is it in bounds?
      if(mino_try[j].x <  0  || mino_try[j].y <  0) { break; }
      if(mino_try[j].x >= 10 || mino_try[j].y >= 20) { break; }

      // is it overlapping anything else
      if(grid[mino_try[j].x][mino_try[j].y]) { break; }

      // if we're still here after all this, then it's all right!
      if(j == 3) { 
        for (int k = 0; k < 4; k++) {
          new_block[k] = mino_try[k];
        }
        return true;
      }
    }
  }
  // if nothing came out of this we still need to do something about new_block or else the compiler gets angry at me
  new_block = NULL;
  return false;
}

int find_max_drop (GPoint *block, bool grid[GAME_GRID_BLOCK_WIDTH][GAME_GRID_BLOCK_HEIGHT]) {
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
  if (block_type == SQUARE) { return BLOCK_SIZE/2; }
  if (block_type == LINE) { return -BLOCK_SIZE/2; }
  else { return 0; }
}

void set_theme(int theme_id) {
  // Load themes from the raw data (generated with the Python script from the json files in the themes folder)
  #ifdef PBL_COLOR
    int i = theme_id % THEMES_COUNT;
    resource_load_byte_range(resource_get_handle(RESOURCE_ID_THEMES), i*THEMES_BYTES, (uint8_t*)&theme, THEMES_BYTES);
  #else
    theme.window_bg_color = GColorBlack;
    theme.window_header_color = GColorWhite;
    theme.window_label_text_color = GColorBlack;
    theme.window_label_bg_color = GColorWhite;
    theme.window_label_bg_inactive_color = GColorLightGray;
    theme.grid_bg_color = GColorWhite;
  #endif
}
