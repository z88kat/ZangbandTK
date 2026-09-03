/**
 *
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 * Copyright (c) 2007-9 Andi Sidwell, Chris Carr, Ed Graham, Erik Osheim
 *
 * This work is free software; you can redistribute it and/or modify it
 * under the terms of either:
 *
 * a) the GNU General Public License as published by the Free Software
 *    Foundation, version 2, or
 *
 * b) the "Angband License":
 *    This software may be copied and distributed for educational, research,
 *    and not for profit purposes provided that this copyright and statement
 *    are included in all such copies.  Other copyrights may also apply.
 */

#ifndef INCLUDED_BORG_CAVE_H
#define INCLUDED_BORG_CAVE_H

/*
 * must be included before ALLOW_BORG to avoid empty compilation unit
 */
#include "../angband.h"

#ifdef ALLOW_BORG

/*
 * Maximum possible dungeon size
 */
/* NOTE: this corresponds to z_info->dungeon_hgt/dungeon_wid */
/* a test is done at the start of borg to make sure the values are right */
#define DUNGEON_WID 198
#define DUNGEON_HGT 66

/*
 * ZangbandTK (BRG-01): the borg's arrays are sized by the largest level the
 * game can put it on, which is not the dungeon.
 *
 * Upstream set these to the dungeon's own 66 x 198 because in Angband every
 * level is the dungeon. Here depth 0 is a wilderness surface, built square at
 * `wild_view_blocks() * z_info->wild_block_size` -- 144 x 144 as the data
 * ships -- so the tallest level the borg stands on is half again the height
 * these were sized for. `borg_update_map()` scanned the panel, checked only
 * `square_in_bounds(cave, ...)`, and then indexed `borg_grids[y][x]` with a
 * `y` reaching 143: it walked off the end of a 66-element array of row
 * pointers and dereferenced whatever followed. That was the segfault on the
 * first turn of every game.
 *
 * These are **ceilings with headroom, not copies of game data.** The standing
 * constraint in the borg plan is that no borg constant may be duplicated from
 * game data, and it is a good one -- three of the four that were duplicated
 * had gone wrong. So nothing here is derived from `wild:block-size` or
 * `wild:cache-blocks`; instead `borg_init_cave()` asks the game at run time
 * what its largest level is and refuses to start if it does not fit inside
 * these. A data change that outgrows the borg gets a clear startup failure
 * rather than the corruption above.
 *
 * Deliberately not as generous as they could be. Several loops scan the whole
 * array every turn, so the ceiling is paid for on every one of them; 160 x 208
 * covers 144 x 198 with room and costs about two and a half times upstream's
 * scan rather than fifteen.
 */
#define BORG_MAX_HGT 160
#define BORG_MAX_WID 208

#define AUTO_MAX_X BORG_MAX_WID
#define AUTO_MAX_Y BORG_MAX_HGT

/*
 * Forward declare
 */
typedef struct borg_grid borg_grid;

/*
 * A grid in the dungeon.
 *
 * There is a terrain feature type, which may be incorrect.  It is
 * more or less based on the standard "feature" values, but some of
 * the legal values are never used, such as "secret door", and others
 * are used in bizarre ways, such as "invisible trap".
 *
 * There is an object (take) index into the "object tracking" array.
 *
 * There is a monster (kill) index into the "monster tracking" array.
 *
 * There is a byte "xtra" which tracks how much "searching" has been done
 * in the grid or in any grid next to the grid.
 *
 * To perform "navigation" from one place to another, the "flow" routines
 * are used, which place "cost" information into the "cost" fields.  Then,
 * if the path is clear, the "cost" information is copied into the "flow"
 * fields, which are used for the actual navigation.  This allows multiple
 * routines to check for "possible" flowing, without hurting the current
 * flow, which may have taken a long time to construct.  We also assume
 * that the Borg never needs to follow a path longer than 250 grids long.
 * Note that the "cost" fields have been moved into external arrays.
 *
 * traps and glyphs are now separate flags
 *
 * Note that the "char" zero will often crash the system!
 * !FIX !TODO Verify if this is still true and, if so, document how/why
 * char zero causes crashes.
 */
struct borg_grid {
    uint8_t  feat; /* Grid type */
    uint16_t info; /* Grid flags */
    bool     trap;
    bool     glyph;
    bool     web;
    uint8_t  store;

    uint8_t  take; /* Object index */
    uint8_t  kill; /* Monster index */

    uint8_t  xtra; /* Extra field (search count) */
};
/*
 * The current map
 */
extern borg_grid *borg_grids[AUTO_MAX_Y]; /* The grids */

extern void borg_init_cave(void);
extern void borg_free_cave(void);

#endif
#endif
