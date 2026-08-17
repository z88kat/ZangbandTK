/**
 * \file wild.h
 * \brief The wilderness overworld (ZangbandTK)
 *
 * Angband is played in one dungeon beneath one town.  ZangbandTK is played
 * across a generated overworld, following Zangband's design: a grid of blocks
 * whose terrain is chosen by position in a height/population/law parameter
 * space, laid out by a plasma fractal.
 *
 * The world map is small and persistent — a few bytes per block, held for the
 * life of the game and written to the savefile.  Block *contents* are not: a
 * block's 16x16 grids are generated on demand from the world seed and the
 * block's own coordinates, so the same block always regenerates identically
 * and only blocks the player has changed need storing (WLD-03, WLD-04).
 *
 * Copyright (c) 2026 ZangbandTK contributors
 *
 * This work is free software; you can redistribute it and/or modify it under
 * the terms of either:
 *
 * a) the GNU General Public License as published by the Free Software
 *    Foundation, version 2, or
 *
 * b) the "Angband licence":
 *    This software may be copied and distributed for educational, research,
 *    and not for profit purposes provided that this copyright and statement
 *    are included in all such copies.  Other copyrights may also apply.
 */

#ifndef INCLUDED_WILD_H
#define INCLUDED_WILD_H

#include "h-basic.h"
#include "z-type.h"

struct chunk;
struct monster_race;
struct object;
struct player;

/**
 * Terrain kinds a wilderness block may take.
 *
 * Zangband selected from 232 block types in w_info.txt by walking a decision
 * tree over the parameter space.  This is the coarse classification that
 * selection resolves to; the detailed types come later (WLD-07).
 */
enum wild_terrain {
	WILD_TERRAIN_OCEAN = 0,
	WILD_TERRAIN_SHORE,
	WILD_TERRAIN_GRASS,
	WILD_TERRAIN_FOREST,
	WILD_TERRAIN_SWAMP,
	WILD_TERRAIN_WASTE,
	WILD_TERRAIN_MOUNTAIN,
	WILD_TERRAIN_MAX
};

/**
 * Flags on a wilderness block.
 */
#define WILD_INFO_ROAD		0x01	/* A road runs through this block */
#define WILD_INFO_WATER		0x02	/* A river or lake runs through it */

/**
 * One block of the world map.
 *
 * Deliberately small: the whole map is held in memory and written to the
 * savefile, so this is multiplied by blocks squared.  At the default 33x33
 * that is 1089 of these.
 */
struct wild_block {
	uint8_t terrain;	/**< enum wild_terrain */
	uint8_t place;		/**< Town or dungeon index here, 0 for none */
	uint8_t info;		/**< WILD_INFO_* flags */

	/**
	 * Position in the parameter space terrain is chosen from.  Kept after
	 * generation because roads, towns and dungeon placement all read them,
	 * and because regenerating the fractal to recover them would be wasteful.
	 */
	uint8_t hgt;		/**< Height: ocean floor through mountain peak */
	uint8_t pop;		/**< Population: wilderness through city */
	uint8_t law;		/**< Law: bandit country through well-policed */
};

/**
 * Something the player left lying in the world (WLD-04).
 *
 * Held in world coordinates and detached from any chunk, since the surface it
 * was dropped on is torn down and rebuilt as the player walks.  The turn it was
 * left is what decides whether it is still there when they come back.
 */
struct wild_relic {
	struct loc grid;		/**< Where in the world it lies */
	int32_t turn;			/**< The turn it was left there */
	struct object *obj;		/**< The thing itself */
	struct wild_relic *next;
};

/**
 * A unique the player met in the wilderness and did not finish (WLD-04b).
 *
 * Only uniques. Ordinary monsters are re-rolled with the country, which reads
 * as their having recovered and moved on -- which is what would have happened.
 * A named monster is different: if you nearly killed it and walked away, it has
 * to still be out there, and it has to be the one you wounded.
 */
struct wild_unique {
	struct monster_race *race;	/**< Which one */
	struct loc grid;			/**< Where in the world you left it */
	int16_t hp;					/**< How badly hurt, when you left */
	int32_t turn;				/**< The turn you left it */
	struct wild_unique *next;
};

/**
 * The world map.
 */
struct wilderness {
	uint32_t seed;		/**< Seed every block's generation derives from */
	int blocks;			/**< Width and height, in blocks */
	struct wild_block *map;	/**< blocks * blocks entries, row-major */

	/**
	 * The block the starting town stands on (WLD-12).  One town for now;
	 * WLD-10's several towns are M5's business, and will want a list here.
	 */
	struct loc town_block;

	/** What the player has left lying about, most recent first (WLD-04). */
	struct wild_relic *relics;

	/** Uniques met and not finished (WLD-04b). */
	struct wild_unique *uniques;
};

extern struct wilderness *wild;

/* wild.c */
struct wild_block *wild_block_at(struct wilderness *w, int x, int y);
bool wild_in_bounds(const struct wilderness *w, int x, int y);
uint32_t wild_block_seed(const struct wilderness *w, int x, int y);
enum wild_terrain wild_classify(int hgt, int pop, int law);

struct wilderness *wild_new(int blocks, uint32_t seed);
void wild_free(struct wilderness *w);
void wild_generate(struct wilderness *w);

int wild_terrain_feat(enum wild_terrain terrain, int roll);
int wild_water_at(struct wilderness *w, int x, int y);

void wild_ensure(uint32_t seed);
void wild_cleanup(void);
int wild_world_grids(void);
int wild_view_blocks(void);
struct chunk *wild_surface(struct wilderness *w, struct player *p,
						   struct loc centre, struct loc *offset);
bool wild_is_surface(const struct chunk *c);
void wild_carry_knowledge(struct chunk *from, struct loc from_offset,
						  struct chunk *to, struct loc to_offset);
bool wild_needs_recentre(struct player *p);
void wild_track_move(struct player *p, struct loc grid);

void wild_harvest(struct wilderness *w, struct player *p, struct chunk *c,
				  struct loc offset);
void wild_restore(struct wilderness *w, struct player *p, struct chunk *c,
				  struct loc offset);
int wild_relic_count(const struct wilderness *w);
int wild_unique_count(const struct wilderness *w);

struct loc wild_town_origin(const struct wilderness *w);
struct loc wild_town_start(struct wilderness *w, struct player *p);
void wild_town_known(struct wilderness *w, struct player *p, struct chunk *c,
					 struct loc offset);
void wild_town_people(struct wilderness *w, struct player *p, struct chunk *c,
					  struct loc offset);
void wild_town_gates_tick(struct wilderness *w, struct chunk *c,
						  struct loc offset);
void wild_town_free(void);

int wild_danger(struct wilderness *w, int x, int y);
int wild_density(struct wilderness *w, int x, int y);
void wild_populate(struct wilderness *w, struct player *p, struct chunk *c,
				   struct loc offset);

#endif /* INCLUDED_WILD_H */
