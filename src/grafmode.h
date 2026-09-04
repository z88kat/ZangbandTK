/**
 * \file grafmode.h
 * \brief Load a list of possible graphics modes.
 *
 * Copyright (c) 2011 Brett Reid
 *
 * This work is free software; you can redistribute it and/or modify it
 * under the terms of either:
 *
 * a) the GNU General Public License as published by the Free Software
 *    Foundation, version 2, or
 *
 * b) the "Angband licence":
 *    This software may be copied and distributed for educational, research,
 *    and not for profit purposes provided that this copyright and statement
 *    are included in all such copies.  Other copyrights may also apply.
 */
#ifndef INCLUDED_GRAFMODE_H
#define INCLUDED_GRAFMODE_H

#include "h-basic.h"

/**
 * Default graphic modes
 */
#define GRAPHICS_NONE           0


/**
 * Specifications for graphics modes.
 * 
 * grafID:      ID of tile set should be >0 and unique for anything new.
 * alphablend:  Bool whether or not the tileset needs alpha blending.
 * overdrawRow: Row in the file where tiles in that row or lower draw the tile
 *              above as well.
 * overdrawMax: Row in the file where tiles in that row or above draw the tile
 *              above as well.
 * cycleHues:   If the sheet is laid out with the colour on the row and the
 *              shape on the column, the number of hues in a colour block;
 *              0 if the sheet is not laid out that way, which is every set
 *              but the Neon one.  See graf_cycle_attr().
 * cycleTones:  Number of tones per hue in that layout.
 * cycleSpan:   How many of those hues a shimmer walks through.  Hues beyond
 *              it are still in the sheet and still used for identity; they
 *              are just not part of the rainbow, which is how grey stays out
 *              of it.
 * cell_width:  Width of an individual tile in pixels.
 * cell_height: Height of an individual tile in pixels.
 * pref:        Value of ANGBAND_GRAF variable.
 * file:        Name of PNG file (if any).
 * menuname:    Name of the tileset in menu.
 */
typedef struct _graphics_mode {
	struct _graphics_mode *pNext;
	uint8_t grafID;
	uint8_t alphablend;
	uint8_t overdrawRow;
	uint8_t overdrawMax;
	uint8_t cycleHues;
	uint8_t cycleTones;
	uint8_t cycleSpan;
	uint16_t cell_width;
	uint16_t cell_height;
	char path[256];
	char pref[32];
	char file[32];
	char menuname[32];
} graphics_mode;

extern graphics_mode *graphics_modes;
extern graphics_mode *current_graphics_mode;
extern int graphics_mode_high_id;

bool init_graphics_modes(void);
void close_graphics_modes(void);
graphics_mode* get_graphics_mode(uint8_t id);

int is_dh_tile(int a, wchar_t c);

bool graf_cycles(void);
uint8_t graf_cycle_attr(uint8_t attr, int step);

#endif /* INCLUDED_GRAFMODE_H */
