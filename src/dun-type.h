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
#include "monster.h"

/** The most dungeons a world may hold. */
#define DUN_TYPE_MAX 24

/** The most monster bases one dungeon may name as its own. */
#define DUN_DWELLERS_MAX 12

/**
 * What a dungeon tends to yield (CNT-12).
 *
 * Four independent weights, each a percentage: given an object the ordinary
 * rules would have produced, this is the chance of keeping it.  So a dungeon
 * with magic 60 and combat 20 turns up three times as many wands as swords
 * relative to the base allocation, without any object being impossible
 * anywhere.
 *
 * They are written summing to 100 for legibility, but nothing requires it:
 * they are separate probabilities, not shares of one.
 */
struct obj_theme {
	uint8_t treasure;	/**< Chests, crowns, rings, amulets, gold */
	uint8_t combat;		/**< Weapons, launchers, ammunition, armour */
	uint8_t magic;		/**< Wands, staves, rods, scrolls, potions, books */
	uint8_t tools;		/**< Diggers, lights, flasks, food */
};

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

	struct obj_theme theme;	/**< What it tends to yield (CNT-12) */

	/**
	 * What lives in it (CNT-05).
	 *
	 * Zangband carried a habitat flag per dungeon on every one of its nine
	 * hundred monsters, and a monster without the flag could not appear at all.
	 * Expressed here through 4.2's own taxonomy instead -- the monster bases and
	 * flags a dungeon is home to -- which says the same thing in a dozen lines
	 * per dungeon rather than a flag field on a thousand monsters, and covers
	 * the monsters Angband brought as well as the ones Zangband did.
	 *
	 * A preference rather than a wall: a monster from somewhere else is rare
	 * here, not impossible, so a dungeon can never run out of things to put in
	 * itself at a depth where its own kinds are thin.
	 */
	struct monster_base *dwellers[DUN_DWELLERS_MAX];
	int dweller_count;
	bitflag dweller_flags[RF_SIZE];
	bool has_dwellers;
	bool has_theme;		/**< False if it was given no theme, so none applies */

	int floor;			/**< Terrain its floors are made of, or FEAT_NONE */
	char *profile;		/**< Cave profile it prefers, or NULL for any */

	uint8_t index;		/**< Its own place in the list */
};

extern struct dun_type *dun_types;

int dun_type_count(void);
struct dun_type *dun_type_by_index(int idx);
struct dun_type *dun_type_by_name(const char *name);
int obj_theme_weight(const struct obj_theme *theme, int tval);
bool dun_type_dwells(const struct dun_type *type, const struct monster_race *race);

#endif /* INCLUDED_DUN_TYPE_H */
