/**
 * \file dun-type.h
 * \brief Named dungeons, each with its own depth range and character (WLD-14)
 *
 * Angband has one dungeon: depth is the only thing that distinguishes level 40
 * from level 4. Zangband's world had a dozen of them, each with a floor of its
 * own, a range of depths it covered, and a bottom you had to leave it to get
 * past -- which is what makes finding a new one worth the walk, and what makes
 * WLD-10's several places mean something.
 *
 * Copyright (c) 2026 ZangbandTK contributors
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

#ifndef INCLUDED_DUN_TYPE_H
#define INCLUDED_DUN_TYPE_H

#include "h-basic.h"

/** The most dungeons a world may hold. */
#define DUN_TYPE_MAX 24

/**
 * A kind of dungeon.
 *
 * Loaded from dungeon.txt, in the order it appears there; the order is the
 * index, and the savefile stores the name rather than the index so that adding
 * a dungeon does not renumber the others.
 */
struct dun_type {
	struct dun_type *next;

	char *name;			/**< What it is called */
	char *desc;			/**< A line for the player on arriving */

	uint8_t min_depth;	/**< Shallowest level it has */
	uint8_t max_depth;	/**< Deepest level it has; there is no way past it */

	uint8_t rarity;		/**< Weight for choosing which dungeons a world holds */
	uint8_t pop;		/**< The population of country it is found in */
	uint8_t height;		/**< The height of ground it is found in */

	int floor;			/**< Terrain its floors are made of, or FEAT_NONE */
	char *profile;		/**< Cave profile it prefers, or NULL for any */

	uint8_t index;		/**< Its own place in the list */
};

extern struct dun_type *dun_types;

int dun_type_count(void);
struct dun_type *dun_type_by_index(int idx);
struct dun_type *dun_type_by_name(const char *name);

#endif /* INCLUDED_DUN_TYPE_H */
