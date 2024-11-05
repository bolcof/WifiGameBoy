#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h> 



#define BYTES_PER_TILE 16u
#define FRAME_TILE_BG_IDX (0xffu - 8u)

static uint8_t sBGDataIdx = FRAME_TILE_BG_IDX - 1u;