/**
 * \file borg-cave.c
 * \brief Track what the borg thinks the dungeon looks like
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

#include "borg-cave.h"

#ifdef ALLOW_BORG

#include "../init.h"
#include "../wild.h"

#include "borg-init.h"
#include "borg-io.h"

borg_grid *borg_grids[AUTO_MAX_Y]; /* The grids */

/**
 * The largest level this game can generate, in grids (ZangbandTK, BRG-02).
 *
 * Asked of the game rather than assumed, because the answer is data: the
 * wilderness surface is square at the view span times the block size, and
 * either of those can be changed in `constants.txt`.
 */
static void borg_largest_level(int *hgt, int *wid)
{
    int surface = wild_view_blocks() * z_info->wild_block_size;

    *hgt = MAX((int) z_info->dungeon_hgt, surface);
    *wid = MAX((int) z_info->dungeon_wid, surface);
}

void borg_init_cave(void)
{
    int max_hgt, max_wid;

    /*
     * Sanity check (ZangbandTK, BRG-02).
     *
     * Upstream compared its own constants against `z_info->dungeon_wid/hgt`
     * and those still match, so the check passed while the borg was about to
     * index off the end of its own arrays on a wilderness level. It was asking
     * whether the *dungeon* had changed shape, when what matters is whether
     * the largest level the borg may be put on fits in what has been
     * allocated for it. That is what this asks.
     */
    borg_largest_level(&max_hgt, &max_wid);
    if (max_hgt > AUTO_MAX_Y || max_wid > AUTO_MAX_X) {
        borg_note(format("**STARTUP FAILURE** the largest level is %dx%d and "
                         "the borg has room for %dx%d",
            max_hgt, max_wid, AUTO_MAX_Y, AUTO_MAX_X));
        borg_init_failure = true;
    }

    /* Make each row of grids */
    for (int y = 0; y < AUTO_MAX_Y; y++) {
        /* Make each row */
        borg_grids[y] = mem_zalloc(AUTO_MAX_X * sizeof(borg_grid));
    }
}

void borg_free_cave(void)
{
    for (int y = 0; y < AUTO_MAX_Y; ++y) {
        mem_free(borg_grids[y]);
        borg_grids[y] = NULL;
    }
}

#endif
