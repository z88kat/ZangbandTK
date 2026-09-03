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

#ifndef INCLUDED_BORG_PREPARED_H
#define INCLUDED_BORG_PREPARED_H

/*
 * must be included before ALLOW_BORG to avoid empty compilation unit
 */
#include "../angband.h"

#ifdef ALLOW_BORG

/*
 * ZangbandTK (BRG-12): how much food the borg wants before descending.
 *
 * `borg_restock()` refuses to go below depth 1 with fewer than this, and
 * `borg_choose_shop()` must go shopping at the same figure or the borg
 * deadlocks -- it will neither dive nor buy. The two were 3 and 0
 * respectively, and the gap between them is where every class sat for three
 * thousand turns. Named so they cannot drift apart again.
 */
#define BORG_FOOD_TO_DIVE 3

/*
 * ZangbandTK (BRG-12): how much food the borg tries to *carry*.
 *
 * Distinct from the figure above, and it has to be comfortably larger. The
 * dive threshold is the minimum to set off with; this is what it stocks up to,
 * and setting off with the minimum is how a run ends at depth 1 with the borg
 * unable to go either way.
 */
#define BORG_FOOD_TO_CARRY 12

extern int          borg_numb_live_unique;
extern unsigned int borg_first_living_unique;
extern int          borg_depth_hunted_unique;

/*
 * Determine what level the borg is prepared to dive to.
 */
extern const char *borg_prepared(int depth);

/*
 * Determine if the Borg is out of "crucial" supplies.
 */
extern const char *borg_restock(int depth);

/*
 * Determine if the Borg should return to town immediately.
 */
extern const char *borg_must_return_to_town(void);

#endif
#endif
