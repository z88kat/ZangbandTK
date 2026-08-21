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
#define WILD_INFO_SEEN		0x04	/* The player has been near enough to see it */

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
 * How many towns the world may hold (WLD-10).
 */
#define WILD_TOWNS_MAX 24

/** The most dungeon entrances a world may hold. */
#define WILD_DUNGEONS_MAX 24

/**
 * One town.
 *
 * Zangband ran four size bands keyed on population -- small, town, city,
 * castle.  Ours are the same idea: the starting town is the smallest there is,
 * and bigger places are something to travel towards, since they hold shops the
 * village does not (WLD-11, WLD-11a).
 */
/*
 * Store indices.
 *
 * build_store() takes a store index and finds the terrain whose shopnum is one
 * more than it; shopnums are handed out in f_info order, which is the order the
 * shop features appear in list-terrain.h, which is the order the entries appear
 * in store.txt.  Named here rather than written as bare numbers, and checked by
 * a test rather than trusted.
 */
enum {
	WILD_STORE_GENERAL = 0,
	WILD_STORE_ARMOR,
	WILD_STORE_WEAPON,
	WILD_STORE_BOOK,
	WILD_STORE_ALCHEMY,
	WILD_STORE_MAGIC,
	WILD_STORE_BLACK,
	WILD_STORE_HOME
};

/**
 * A dungeon's mouth, somewhere in the world (WLD-14).
 *
 * The dungeon itself is defined in dungeon.txt; this is only where the way in
 * happens to be, and how deep the player has got down it.
 */
struct wild_dungeon {
	struct loc block;		/**< The block it opens in */
	struct loc grid;		/**< Which grid of that block, in world coordinates */
	uint8_t type;			/**< Index into dungeon.txt's list */
	uint8_t max_depth;		/**< The deepest level reached, for recall */
};

struct wild_town {
	struct loc block;	/**< The block it centres on */
	uint16_t wid, hgt;	/**< Its size, in grids */
	uint16_t stores;	/**< Bit per store index: which ones it holds */
	uint8_t band;		/**< 0 village, 1 town, 2 city, 3 great city */
};

/**
 * The world map.
 */
struct wilderness {
	uint32_t seed;		/**< Seed every block's generation derives from */
	int blocks;			/**< Width and height, in blocks */
	struct wild_block *map;	/**< blocks * blocks entries, row-major */

	/** The towns, the first of which is where the player starts (WLD-12). */
	struct wild_dungeon dungeons[WILD_DUNGEONS_MAX];
	int dungeon_count;

	struct wild_town towns[WILD_TOWNS_MAX];
	int town_count;

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

void wild_mark_seen(struct wilderness *w, struct loc grid);
bool wild_seen(struct wilderness *w, int x, int y);
bool wild_in_town(struct wilderness *w, int bx, int by);
bool wild_road_at(struct wilderness *w, int bx, int by);
int wild_dungeon_count(const struct wilderness *w);
int wild_dungeon_at(struct wilderness *w, struct loc grid);
bool wild_dungeon_in_block(struct wilderness *w, int bx, int by);
struct wild_dungeon *wild_dungeon_by_index(struct wilderness *w, int idx);
struct loc wild_scroll_delta(void);
void wild_adopt_window(struct loc offset);
int wild_block_feat(struct wilderness *w, int x, int y);

void wild_ensure(uint32_t seed);
void wild_cleanup(void);
int wild_world_grids(void);
int wild_view_blocks(void);
struct chunk *wild_surface(struct wilderness *w, struct player *p,
						   struct loc centre, struct loc *offset);
bool wild_is_surface(const struct chunk *c);
void wild_settle_player(struct chunk *c, struct player *p);
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

int wild_town_count(const struct wilderness *w);
struct loc wild_town_origin_of(const struct wilderness *w, int town);
struct loc wild_town_origin(const struct wilderness *w);
int wild_town_at(struct wilderness *w, int bx, int by);
struct loc wild_town_start(struct wilderness *w, struct player *p);
void wild_town_known(struct wilderness *w, struct player *p, struct chunk *c,
					 struct loc offset);
void wild_town_people(struct wilderness *w, struct player *p, struct chunk *c,
					  struct loc offset);
void wild_town_gates_tick(struct wilderness *w, struct chunk *c,
						  struct loc offset);
void wild_town_free(void);
void wild_keep_knowledge(struct chunk *known, struct loc offset);
struct chunk *wild_take_knowledge(struct loc *offset);
struct chunk *wild_held_knowledge(struct loc *offset);

int wild_danger(struct wilderness *w, int x, int y);
int wild_density(struct wilderness *w, int x, int y);
void wild_populate(struct wilderness *w, struct player *p, struct chunk *c,
				   struct loc offset);

#endif /* INCLUDED_WILD_H */
