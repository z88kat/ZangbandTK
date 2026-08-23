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

	/**
	 * Magic: how thick the country is with it (WLD-15).
	 *
	 * Zangband scored a building on population, magic and law, and we had the
	 * first and third already.  Its own fractal, so it varies smoothly and
	 * independently -- a lawful, populous, mundane city and a thinly settled
	 * place steeped in magic are both worlds this can build.  Costs nothing to
	 * carry: the world map is never written to a savefile, it regenerates from
	 * the seed (WLD-03).
	 */
	uint8_t magic;
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
 * One rung of the shop quality ladder, loaded from quality.txt (WLD-16a).
 *
 * Tier 0 is the plain trade and has no record: the array holds what is above
 * it, so tier n is quality_tiers[n - 1].
 */
struct quality_tier {
	char *name;		/**< Adjective put in front of the trade's own name */
	int level;		/**< Added to the level its goods are generated at */
	int stock;		/**< Extra slots on the shelves */
};

extern struct quality_tier *quality_tiers;
extern int quality_tier_count;

/**
 * The names a world's towns are drawn from, loaded from town.txt (WLD-11).
 */
struct town_names {
	char **settled;			/**< Names for governed country */
	int settled_count;
	char **lawless;			/**< Names for country that has fallen */
	int lawless_count;
};

extern struct town_names town_names;

/**
 * The services a town may keep (WLD-16, WLD-16c).
 *
 * Buildings with behaviour behind them, as against shops, which 4.2's store
 * system already handles.  Zangband had eight with implemented behaviour and
 * WLD-16c kept six of them; the quest giver waits for M6 and the Chaos Tower
 * for M8, since neither has anything to do before those exist.
 */
enum wild_service {
	WILD_SERVICE_MAGETOWER = 0,	/**< Travel between places already found */
	WILD_SERVICE_HEALER,		/**< Cure, heal, and restore what is drained */
	WILD_SERVICE_INN,			/**< Rest until morning */
	WILD_SERVICE_ENCHANT,		/**< The magesmith: put magic on an item */
	WILD_SERVICE_RECHARGE,		/**< Put charges back in a wand or a staff */
	WILD_SERVICE_MAX
};

/**
 * Who lives in a town (WLD-11).
 *
 * Zangband declared six kinds -- villager, elves, dwarf, lizard, monster,
 * abandoned -- and implemented three.  TOWN_MONST_ELVES, _DWARF and _LIZARD
 * appear exactly once each in the whole of its source, in their own #define,
 * and are referenced nowhere else; every ordinary town it built was a villager
 * town.  So the taxonomy the requirement calls a reference was three unused
 * constants -- and under DEC-30 they are also the generic fantasy filler the
 * game is being steered away from rather than towards.
 *
 * What is here instead: the three Zangband gave behaviour to, and beasts in
 * place of the three it did not.  A shadow of Amber standing empty, or with
 * something living in it that should not be, is what the books are full of.
 */
enum wild_folk {
	WILD_FOLK_VILLAGER = 0,	/**< People, going about their business */
	WILD_FOLK_BEAST,		/**< Emptied once, and the animals moved in */
	WILD_FOLK_MONSTER,		/**< Taken, and still held */
	WILD_FOLK_ABANDONED,	/**< Nobody at all */
	WILD_FOLK_MAX
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
	uint8_t folk;		/**< enum wild_folk: who lives there (WLD-11) */
	const char *name;	/**< What it is called (WLD-11) */
	uint16_t services;	/**< Bit per enum wild_service (WLD-16) */
	uint8_t visited;	/**< The player has stood inside it (WLD-16c) */
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
const char *wild_folk_name(int folk);
const char *wild_band_name(int band);
const char *wild_service_name(int service);
int wild_service_at(struct chunk *c, struct loc grid);
int wild_store_quality(struct wilderness *w, int town, int store);
const char *wild_quality_name(int tier);
int wild_town_here(struct wilderness *w, struct loc grid);
void wild_note_visit(struct wilderness *w, struct loc grid);
bool wild_dungeon_found(struct wilderness *w, int idx);

/** Somewhere the magetower will carry the player (WLD-16c). */
struct wild_place {
	struct loc grid;		/**< Where it is, in world grids */
	const char *name;		/**< What it is called */
	const char *what;		/**< What kind of place it is */
	int32_t cost;			/**< What the journey costs, in gold */
};

int wild_travel_places(struct wilderness *w, struct loc from,
					   struct wild_place *dest, int max);
int32_t wild_travel_cost(struct wilderness *w, struct loc from, struct loc to);
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
