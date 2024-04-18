//AUTOGENERATED FILE FROM png2asset

#include <stdint.h>
#include <gbdk/platform.h>
#include <gbdk/metasprites.h>

BANKREF(ball)

const palette_color_t ball_palettes[4] = {
	RGB8(255,255,255), RGB8(168,168,168), RGB8( 96, 96, 96), RGB8(  0,  0,  0)
	
};

const uint8_t ball_tiles[16] = {
	0x00,0x00,0x00,0x00,
	0x1c,0x1c,0x26,0x26,
	0x3e,0x3e,0x3e,0x3e,
	0x1c,0x1c,0x00,0x00
	
};

const metasprite_t ball_metasprite0[] = {
	METASPR_ITEM(-4, -4, 0, S_PAL(0)),
	METASPR_TERM
};

const metasprite_t* const ball_metasprites[1] = {
	ball_metasprite0
};