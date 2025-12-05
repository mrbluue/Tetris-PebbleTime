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
    // Origin of the grid is in the top left
    // X  Y        * = pivot point at x = 0 & y = 0 at index [0] (part of shape that never rotates)
    {  0,  0 },  // SQUARE
    { -1,  0 },  // |2|3 |
    { -1, -1 },  // |1|0*|
    {  0, -1 }
  },
  { 
    {  0, 0 },
    { -2, 0 },  // LINE
    { -1, 0 },  // |1|2|0*|3|
    {  1, 0 }   
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

  GPoint pivot_point = old_block[0]; // the 1 block out of 4 that never rotates
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
  int i = theme_id % THEMES_COUNT;
  resource_load_byte_range(resource_get_handle(RESOURCE_ID_THEMES), i*THEMES_BYTES, (uint8_t*)&theme, THEMES_BYTES);
}
