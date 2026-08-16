/**
 * \file wild.c
 * \brief The wilderness overworld (ZangbandZK)
 *
 * See wild.h for the design. This file lays out the world map: a plasma
 * fractal over height, population and law, then a terrain kind chosen from
 * each block's position in that space.
 *
 * The fractal is ported from Zangband's frac_block() in wild3.c, under DEC-20.
 * The algorithm is the value — it is what makes coastlines look like
 * coastlines — while the structures it fills are ours.
 *
 * Copyright (c) 1989, 1999 James E. Wilson, Robert A. Koeneke, Robert Ruehlmann
 * Copyright (c) 2026 ZangbandZK contributors
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

#include "angband.h"
#include "cave.h"
#include "game-world.h"
#include "generate.h"
#include "init.h"
#include "player.h"
#include "wild.h"

struct wilderness *wild = NULL;

/**
 * Sea covers roughly this fraction of the world, as in Zangband.
 */
#define WILD_SEA_FRACTION 4

/**
 * Names for the terrain kinds, for the map display and for diagnostics.
 */
static const char *wild_terrain_names[WILD_TERRAIN_MAX] = {
	"ocean", "shore", "grassland", "forest", "swamp", "wasteland", "mountains"
};

const char *wild_terrain_name(enum wild_terrain terrain)
{
	if (terrain >= WILD_TERRAIN_MAX)
		return "unknown";
	return wild_terrain_names[terrain];
}

bool wild_in_bounds(const struct wilderness *w, int x, int y)
{
	return w && x >= 0 && y >= 0 && x < w->blocks && y < w->blocks;
}

struct wild_block *wild_block_at(struct wilderness *w, int x, int y)
{
	if (!wild_in_bounds(w, x, y))
		return NULL;
	return &w->map[y * w->blocks + x];
}

/**
 * The seed a given block's contents generate from (WLD-03).
 *
 * Must depend on the world seed and the block's coordinates and nothing else,
 * so that a block regenerates identically however the player reached it and in
 * whatever order blocks were visited.  Anything drawn from global RNG state
 * would make the world depend on the route taken through it.
 *
 * The mixing is a small integer hash: multiply each coordinate by a large odd
 * constant, combine, and stir. Quality matters only in that neighbouring
 * blocks must not produce visibly related results.
 */
uint32_t wild_block_seed(const struct wilderness *w, int x, int y)
{
	uint32_t h = w ? w->seed : 0;

	h ^= (uint32_t) x * 0x9E3779B1u;
	h ^= (uint32_t) y * 0x85EBCA77u;
	h ^= h >> 15;
	h *= 0x2545F491u;
	h ^= h >> 13;

	return h;
}

/**
 * Choose a terrain kind from a block's position in parameter space (WLD-07).
 *
 * Zangband walked a decision tree cutting on height, population and law in
 * turn, with 232 leaf types.  This is the coarse form of the same idea: height
 * dominates, since it decides sea from land and lowland from peak, with
 * population and law distinguishing between kinds of land at the same
 * elevation.
 *
 * \param hgt is elevation, 0 (ocean floor) to 255 (peak).
 * \param pop is population, 0 (empty) to 255 (dense).
 * \param law is order, 0 (lawless) to 255 (policed).
 */
enum wild_terrain wild_classify(int hgt, int pop, int law)
{
	/* Below sea level. The world is roughly a quarter water. */
	if (hgt < 256 / WILD_SEA_FRACTION)
		return WILD_TERRAIN_OCEAN;

	/* Just above it: the coast. */
	if (hgt < 256 / WILD_SEA_FRACTION + 12)
		return WILD_TERRAIN_SHORE;

	/* The heights, where nothing much lives. */
	if (hgt > 200)
		return WILD_TERRAIN_MOUNTAIN;

	/*
	 * Lowland. Wet ground where the land is low and lawless — swamps sit
	 * beyond the reach of anyone who would drain them.
	 */
	if (hgt < 96 && law < 96)
		return WILD_TERRAIN_SWAMP;

	/* Settled country is cleared country. */
	if (pop > 128)
		return WILD_TERRAIN_GRASS;

	/* Lawless and empty land goes to waste; empty but ordered land wooded. */
	if (law < 64)
		return WILD_TERRAIN_WASTE;

	return WILD_TERRAIN_FOREST;
}

/**
 * A plasma fractal over one parameter of the world map.
 *
 * Ported from Zangband's frac_block() (wild3.c:386) under DEC-20, adapted to
 * run over the world map rather than a single block, and to take its
 * randomness from a caller-supplied stream so that world generation is
 * reproducible from the world seed alone.
 *
 * The array is (size x size) with size = 2^n + 1, so that repeated halving
 * always lands on a sample point and both edges are included.
 *
 * \param values is the map being filled, size * size, row-major.
 * \param size is the map's width and height.
 * \param roughness scales the random displacement at each step; larger values
 * give a more broken landscape.
 */
/**
 * Comparator for the rank sort in wild_equalise().
 */
static int wild_cmp_int(const void *a, const void *b)
{
	int x = *(const int *) a, y = *(const int *) b;

	return (x > y) - (x < y);
}

/**
 * Spread a parameter map evenly across the byte range, by rank.
 *
 * The plasma fractal produces a roughly bell-shaped distribution: most blocks
 * land near the middle and few reach the extremes.  Under fixed thresholds that
 * makes composition unpredictable and wrong — measured before this was added, a
 * world came out 49% ocean and 1% grassland against thresholds asking for
 * roughly 25% and rather more.  Merely stretching to the full range does not
 * help, because it moves the tails without changing where the bulk sits.
 *
 * Replacing each value with its rank makes the distribution uniform, so the
 * thresholds in wild_classify() mean exactly what they say: a cut at a quarter
 * of the range takes a quarter of the world.
 *
 * Worlds still differ in *shape* — where the ocean is, how the coast runs, how
 * broken the mountains are — which is what the fractal is for.  What no longer
 * varies is the proportion of each terrain, and that is a fair trade for
 * composition we can reason about and tune.
 */
static void wild_equalise(int *values, int count)
{
	int *sorted = mem_alloc(count * sizeof(int));
	int i, j;

	memcpy(sorted, values, count * sizeof(int));
	sort(sorted, count, sizeof(int), wild_cmp_int);

	for (i = 0; i < count; i++) {
		/* Binary search for this value's rank among the sorted values. */
		int low = 0, high = count - 1, rank = 0;

		while (low <= high) {
			int mid = (low + high) / 2;

			if (sorted[mid] < values[i]) {
				rank = mid + 1;
				low = mid + 1;
			} else {
				high = mid - 1;
			}
		}

		values[i] = (rank * 255) / (count > 1 ? count - 1 : 1);
	}

	/* Guard: ranks must stay inside the byte range even at the top. */
	for (j = 0; j < count; j++)
		values[j] = MAX(0, MIN(255, values[j]));

	mem_free(sorted);
}

static void wild_plasma(int *values, int size, int roughness)
{
	int step, half, x, y;

	/* Seed the four corners. Everything else interpolates between them. */
	values[0] = randint0(256);
	values[size - 1] = randint0(256);
	values[(size - 1) * size] = randint0(256);
	values[(size - 1) * size + size - 1] = randint0(256);

	for (step = size - 1; step > 1; step = half) {
		half = step / 2;

		/* Diamond: the centre of each square, from its four corners. */
		for (y = half; y < size; y += step) {
			for (x = half; x < size; x += step) {
				int sum = values[(y - half) * size + (x - half)]
					+ values[(y - half) * size + (x + half)]
					+ values[(y + half) * size + (x - half)]
					+ values[(y + half) * size + (x + half)];
				int jitter = randint0(step * roughness) - (step * roughness) / 2;

				values[y * size + x] = sum / 4 + jitter;
			}
		}

		/*
		 * Square: the centre of each diamond, from its neighbours.  Edge
		 * points have three rather than four, so average what is present.
		 */
		for (y = 0; y < size; y += half) {
			int offset = ((y / half) % 2) ? 0 : half;

			for (x = offset; x < size; x += step) {
				int sum = 0, count = 0;
				int jitter = randint0(step * roughness) - (step * roughness) / 2;

				if (x >= half) { sum += values[y * size + x - half]; count++; }
				if (x + half < size) { sum += values[y * size + x + half]; count++; }
				if (y >= half) { sum += values[(y - half) * size + x]; count++; }
				if (y + half < size) { sum += values[(y + half) * size + x]; count++; }

				if (count)
					values[y * size + x] = sum / count + jitter;
			}
		}
	}

	wild_equalise(values, size * size);
}

struct wilderness *wild_new(int blocks, uint32_t seed)
{
	struct wilderness *w = mem_zalloc(sizeof *w);

	w->blocks = blocks;
	w->seed = seed;
	w->map = mem_zalloc((size_t) blocks * blocks * sizeof(struct wild_block));

	return w;
}

void wild_free(struct wilderness *w)
{
	if (!w)
		return;
	mem_free(w->map);
	mem_free(w);
}

/**
 * How many blocks the town's footprint covers, in each direction.
 *
 * The town is a fixed rectangle of grids, so how many blocks it lies across
 * depends on the block size.  A block of margin is added on every side: a town
 * that runs right up to the edge of its blocks has nowhere for a road to leave
 * from, and looks wrong butted against whatever the next block turns out to be.
 */
static void wild_town_extent(int *bw, int *bh)
{
	int size = z_info->wild_block_size;

	*bw = (z_info->town_wid + size - 1) / size + 2;
	*bh = (z_info->town_hgt + size - 1) / size + 2;
}

/**
 * Choose where the starting town stands (WLD-12).
 *
 * Towns go where people would put them: dry, settled, orderly ground, with the
 * whole footprint on land rather than half of it in the sea.  Ties go towards
 * the middle of the world, so a new character does not begin in a corner with
 * three quarters of the map behind them.
 *
 * Zangband scattered twenty towns across the map and used a minimum separation
 * to keep them apart.  This places the one town the game currently has; the
 * scoring is written so that adding the rest (WLD-10) is a matter of looping
 * and excluding what has already been taken.
 */
static void wild_place_town(struct wilderness *w)
{
	int size = w->blocks;
	int centre = size / 2;
	int best = -1;
	int bw, bh, x, y;

	wild_town_extent(&bw, &bh);

	for (y = bh; y < size - bh; y++) {
		for (x = bw; x < size - bw; x++) {
			struct wild_block *block = &w->map[y * size + x];
			int score, dist, fx, fy;
			bool ok = true;

			switch (block->terrain) {
				case WILD_TERRAIN_GRASS: score = 120; break;
				case WILD_TERRAIN_FOREST: score = 90; break;
				case WILD_TERRAIN_WASTE: score = 40; break;
				default: continue;	/* sea, shore, swamp, mountain */
			}

			/* The whole footprint has to be land you could build on. */
			for (fy = y - bh / 2; fy <= y + bh / 2 && ok; fy++)
				for (fx = x - bw / 2; fx <= x + bw / 2 && ok; fx++) {
					struct wild_block *f = wild_block_at(w, fx, fy);

					if (!f || f->terrain == WILD_TERRAIN_OCEAN ||
						f->terrain == WILD_TERRAIN_MOUNTAIN)
						ok = false;
				}
			if (!ok) continue;

			/* People and order both argue for a town being here. */
			score += block->pop / 4 + block->law / 8;

			/* All else equal, nearer the middle of the world. */
			dist = MAX(ABS(x - centre), ABS(y - centre));
			score -= dist * 2;

			if (score > best) {
				best = score;
				w->town_block = loc(x, y);
			}
		}
	}

	/*
	 * A world with no habitable land at all is possible in principle -- the
	 * fractal could come out as ocean and mountain -- so fall back to the
	 * middle rather than leaving the town at (0, 0), off the map's edge.
	 */
	if (best < 0)
		w->town_block = loc(centre, centre);

	/* Mark the footprint, and give the town a road to sit on. */
	{
		int fx, fy;

		for (fy = w->town_block.y - bh / 2; fy <= w->town_block.y + bh / 2; fy++)
			for (fx = w->town_block.x - bw / 2;
				 fx <= w->town_block.x + bw / 2; fx++) {
				struct wild_block *f = wild_block_at(w, fx, fy);

				if (!f) continue;
				f->place = 1;
				f->info |= WILD_INFO_ROAD;
			}
	}
}

/**
 * The world grid of the town rectangle's top-left corner.
 */
struct loc wild_town_origin(const struct wilderness *w)
{
	int size = z_info->wild_block_size;
	int cx = w->town_block.x * size + size / 2;
	int cy = w->town_block.y * size + size / 2;

	return loc(cx - z_info->town_wid / 2, cy - z_info->town_hgt / 2);
}

/**
 * Lay out the world map (WLD-07, WLD-08).
 *
 * Three independent fractals give each block a position in parameter space,
 * and its terrain follows from that.  Height is the roughest, since coastlines
 * and mountain ranges should be ragged; population and law vary more smoothly,
 * because settlement and order spread by contiguity.
 *
 * Reproducible: seeded from the world seed, so the same seed always yields the
 * same world.
 */
void wild_generate(struct wilderness *w)
{
	int size = w->blocks;
	int count = size * size;
	int *hgt = mem_zalloc(count * sizeof(int));
	int *pop = mem_zalloc(count * sizeof(int));
	int *law = mem_zalloc(count * sizeof(int));
	int i;

	/* World generation draws from a stream fixed by the world seed alone. */
	Rand_quick = true;
	Rand_value = w->seed;

	wild_plasma(hgt, size, 24);
	wild_plasma(pop, size, 12);
	wild_plasma(law, size, 12);

	for (i = 0; i < count; i++) {
		struct wild_block *block = &w->map[i];

		block->hgt = (uint8_t) hgt[i];
		block->pop = (uint8_t) pop[i];
		block->law = (uint8_t) law[i];
		block->terrain = (uint8_t) wild_classify(hgt[i], pop[i], law[i]);
		block->place = 0;
		block->info = 0;
	}

	wild_place_town(w);

	Rand_quick = false;

	mem_free(hgt);
	mem_free(pop);
	mem_free(law);
}

/**
 * ------------------------------------------------------------------------
 * Turning a map block into ground you can walk on
 * ------------------------------------------------------------------------ */

/**
 * The terrain feature a given wilderness kind lays down.
 *
 * Each block is mostly one thing with a scattering of others, which is what
 * keeps open country from looking like graph paper.  \p roll is a value in
 * 0..99 drawn from the block's own stream, so the mix is reproducible.
 */
int wild_terrain_feat(enum wild_terrain terrain, int roll)
{
	switch (terrain) {
		case WILD_TERRAIN_OCEAN:
			return (roll < 90) ? FEAT_DEEP_WATER : FEAT_WATER;

		case WILD_TERRAIN_SHORE:
			if (roll < 60) return FEAT_SAND;
			if (roll < 85) return FEAT_WATER;
			return FEAT_GRASS;

		case WILD_TERRAIN_GRASS:
			if (roll < 85) return FEAT_GRASS;
			if (roll < 95) return FEAT_TREE;
			return FEAT_DIRT;

		case WILD_TERRAIN_FOREST:
			if (roll < 70) return FEAT_TREE;
			if (roll < 95) return FEAT_GRASS;
			return FEAT_DIRT;

		case WILD_TERRAIN_SWAMP:
			if (roll < 55) return FEAT_MUD;
			if (roll < 80) return FEAT_WATER;
			if (roll < 95) return FEAT_TREE;
			return FEAT_GRASS;

		case WILD_TERRAIN_WASTE:
			if (roll < 85) return FEAT_DIRT;
			if (roll < 95) return FEAT_ROCK;
			return FEAT_GRASS;

		case WILD_TERRAIN_MOUNTAIN:
			if (roll < 65) return FEAT_ROCK;
			if (roll < 90) return FEAT_DIRT;
			return FEAT_GRASS;

		default:
			return FEAT_GRASS;
	}
}

/**
 * Build the walkable chunk for one block of the world map (WLD-01, WLD-03).
 *
 * Generated entirely from the block's seed, so the same block always comes out
 * the same however the player arrived at it.  The game's RNG state is saved and
 * restored around this: generating scenery must not disturb the game's own
 * random stream, or merely walking about would change every later roll.
 */
struct chunk *wild_block_chunk(struct wilderness *w, int x, int y)
{
	struct wild_block *block = wild_block_at(w, x, y);
	int size = z_info->wild_block_size;
	struct chunk *c;
	struct loc grid;
	bool saved_quick = Rand_quick;
	uint32_t saved_value = Rand_value;

	if (!block)
		return NULL;

	c = cave_new(size, size);
	c->depth = 0;
	c->name = string_make(format("wild:%d,%d", x, y));

	Rand_quick = true;
	Rand_value = wild_block_seed(w, x, y);

	for (grid.y = 0; grid.y < size; grid.y++) {
		for (grid.x = 0; grid.x < size; grid.x++) {
			int feat = wild_terrain_feat(block->terrain, randint0(100));

			square_set_feat(c, grid, feat);
		}
	}

	/* A road runs east-west through the middle of any block it passes. */
	if (block->info & (WILD_INFO_ROAD | WILD_INFO_TRACK)) {
		grid.y = size / 2;
		for (grid.x = 0; grid.x < size; grid.x++)
			square_set_feat(c, grid, FEAT_ROAD);
	}

	Rand_quick = saved_quick;
	Rand_value = saved_value;

	return c;
}

/**
 * ------------------------------------------------------------------------
 * The block cache (WLD-05)
 * ------------------------------------------------------------------------ */

/**
 * Blocks held in memory, with the coordinates they were generated for.
 *
 * A plain array rather than a hash: the cache is bounded at a few dozen entries
 * by design, the working set is the blocks around the player, and a linear scan
 * over that is faster than hashing.  This is deliberately not the mistake
 * chunk_find_name() makes -- that scan is over an unbounded list and compares
 * strings.
 */
struct wild_cache_entry {
	struct chunk *chunk;
	int x, y;
	bool used;
};

static struct wild_cache_entry *wild_cache = NULL;
static int wild_cache_size = 0;

void wild_cache_free(void)
{
	int i;

	if (!wild_cache)
		return;

	for (i = 0; i < wild_cache_size; i++)
		if (wild_cache[i].used)
			cave_free(wild_cache[i].chunk);

	mem_free(wild_cache);
	wild_cache = NULL;
	wild_cache_size = 0;
}

void wild_cache_init(int capacity)
{
	wild_cache_free();
	wild_cache_size = capacity;
	wild_cache = mem_zalloc(capacity * sizeof(struct wild_cache_entry));
}

int wild_cache_count(void)
{
	int i, count = 0;

	for (i = 0; i < wild_cache_size; i++)
		if (wild_cache[i].used)
			count++;

	return count;
}

/**
 * Fetch a block's chunk, generating it if it is not already resident.
 */
struct chunk *wild_cache_get(struct wilderness *w, int x, int y)
{
	int i, free_slot = -1;

	if (!wild_in_bounds(w, x, y) || !wild_cache)
		return NULL;

	for (i = 0; i < wild_cache_size; i++) {
		if (wild_cache[i].used) {
			if (wild_cache[i].x == x && wild_cache[i].y == y)
				return wild_cache[i].chunk;
		} else if (free_slot < 0) {
			free_slot = i;
		}
	}

	/*
	 * Full.  Callers trim by distance before the cache fills, so reaching here
	 * means the working set outgrew the cache; evict the first slot rather than
	 * failing, since a block is cheap to regenerate.
	 */
	if (free_slot < 0) {
		cave_free(wild_cache[0].chunk);
		wild_cache[0].used = false;
		free_slot = 0;
	}

	wild_cache[free_slot].chunk = wild_block_chunk(w, x, y);
	wild_cache[free_slot].x = x;
	wild_cache[free_slot].y = y;
	wild_cache[free_slot].used = true;

	return wild_cache[free_slot].chunk;
}

/**
 * Drop blocks that are no longer near the player (WLD-05).
 *
 * Distance is Chebyshev, matching the squared-off live area around the player
 * rather than a circular one.  Deliberately simple: a block is cheap to
 * regenerate, so evicting one needed again shortly costs little, and the cache
 * exists to avoid regenerating on every step rather than to be clever.
 */
void wild_cache_trim(int centre_x, int centre_y)
{
	int radius = 1;
	int i;

	if (!wild_cache)
		return;

	/* Keep the largest square that comfortably fits the cache. */
	while ((2 * (radius + 1) + 1) * (2 * (radius + 1) + 1) <= wild_cache_size)
		radius++;

	for (i = 0; i < wild_cache_size; i++) {
		int dx, dy;

		if (!wild_cache[i].used)
			continue;

		dx = ABS(wild_cache[i].x - centre_x);
		dy = ABS(wild_cache[i].y - centre_y);

		if (MAX(dx, dy) > radius) {
			cave_free(wild_cache[i].chunk);
			wild_cache[i].chunk = NULL;
			wild_cache[i].used = false;
		}
	}
}

/**
 * ------------------------------------------------------------------------
 * The wilderness surface (WLD-23, WLD-24)
 * ------------------------------------------------------------------------ */

/**
 * Make sure the world exists, generating it on first use.
 *
 * Generated once per game from a seed the savefile already carries, so a
 * character always returns to the same world.
 *
 * Keyed on the seed rather than merely on the world existing: one process can
 * see several characters, through starting again after a death or through
 * loading a different savefile, and each of them lives in a different world.
 * Handing the second character the first one's world would be a hard fault to
 * find, because everything about it would look right.
 */
void wild_ensure(uint32_t seed)
{
	uint32_t use = seed ? seed : 1;

	if (wild && wild->seed == use)
		return;

	wild_cleanup();

	wild = wild_new(z_info->wild_blocks, use);
	wild_generate(wild);
	wild_cache_init(z_info->wild_cache_blocks);
}

/**
 * Let go of the world.  Called when the game shuts down, and whenever a
 * different world is about to be generated.
 */
void wild_cleanup(void)
{
	wild_town_free();
	wild_cache_free();
	wild_free(wild);
	wild = NULL;
}

/**
 * How many blocks across the live surface is.
 *
 * Zangband kept 9x9 blocks live around the player.  Derived from the cache
 * size so the two cannot disagree: the surface is the largest odd square that
 * the cache can hold.
 */
int wild_view_blocks(void)
{
	int view = 1;

	while ((view + 2) * (view + 2) <= z_info->wild_cache_blocks)
		view += 2;

	return view;
}

/**
 * The width of the world, in grids.
 */
int wild_world_grids(void)
{
	return wild ? wild->blocks * z_info->wild_block_size : 0;
}

/**
 * ------------------------------------------------------------------------
 * The town on the surface (WLD-24)
 * ------------------------------------------------------------------------ */

/**
 * The town, generated once and kept for as long as the world lasts.
 *
 * Held here rather than in chunk_list because the town is not a level: it is a
 * patch of the surface, drawn in wherever the live window happens to cover it.
 */
static struct chunk *wild_town = NULL;

void wild_town_free(void)
{
	if (wild_town) {
		cave_free(wild_town);
		wild_town = NULL;
	}
}

/**
 * Strip the rock the town was carved out of.
 *
 * Angband 4.2's town is a starburst clearing blasted out of solid rock, with
 * whatever rock the clearing did not reach left standing around it.  As a level
 * of its own that is invisible -- the rock is the level's edge, and you never
 * see it as anything else.  Dropped whole onto grassland it stops being an edge
 * and becomes a wall: a ring of granite round the town, which is precisely the
 * walled town this project is not trying to build.
 *
 * So the rock goes and the countryside takes its place.  What is left is 4.2's
 * town exactly as 4.2 draws it -- the clearing, the streets, the shops, the
 * ruins -- standing in open country instead of in a quarry.  It also settles
 * the way out without cutting anything: the clearing meets the fields on every
 * side, so you leave town by walking off the end of a street.
 *
 * Found by flooding inwards from the town's border over rock and lava.  Ruins
 * are built of granite too, but they stand inside the clearing with floor all
 * round them, so the flood never reaches them and they survive.
 */
static void wild_town_strip_rock(struct chunk *town)
{
	int w = town->width, h = town->height;
	struct loc *queue = mem_zalloc((size_t) w * h * sizeof(struct loc));
	bool *seen = mem_zalloc((size_t) w * h * sizeof(bool));
	int head = 0, tail = 0;
	struct loc grid;

	/*
	 * Rock, and the lava streamers run through it.  Both were laid down before
	 * the clearing was cut, so both are part of the ground the town was made
	 * from rather than part of the town.
	 */
	#define TOWN_IS_SURROUND(g) \
		(square_isrock(town, (g)) || square_isfiery(town, (g)))

	/* Start from every grid just inside the town's own boundary. */
	for (grid.x = 1; grid.x < w - 1; grid.x++) {
		struct loc top = loc(grid.x, 1), bottom = loc(grid.x, h - 2);

		if (TOWN_IS_SURROUND(top) && !seen[1 * w + grid.x]) {
			seen[1 * w + grid.x] = true;
			queue[tail++] = top;
		}
		if (TOWN_IS_SURROUND(bottom) && !seen[(h - 2) * w + grid.x]) {
			seen[(h - 2) * w + grid.x] = true;
			queue[tail++] = bottom;
		}
	}
	for (grid.y = 1; grid.y < h - 1; grid.y++) {
		struct loc left = loc(1, grid.y), right = loc(w - 2, grid.y);

		if (TOWN_IS_SURROUND(left) && !seen[grid.y * w + 1]) {
			seen[grid.y * w + 1] = true;
			queue[tail++] = left;
		}
		if (TOWN_IS_SURROUND(right) && !seen[grid.y * w + w - 2]) {
			seen[grid.y * w + w - 2] = true;
			queue[tail++] = right;
		}
	}

	while (head < tail) {
		struct loc here = queue[head++];
		int dir;

		for (dir = 0; dir < 8; dir++) {
			struct loc next = loc(here.x + ddx_ddd[dir], here.y + ddy_ddd[dir]);

			if (next.x < 1 || next.x > w - 2) continue;
			if (next.y < 1 || next.y > h - 2) continue;
			if (seen[next.y * w + next.x]) continue;
			if (!TOWN_IS_SURROUND(next)) continue;

			seen[next.y * w + next.x] = true;
			queue[tail++] = next;
		}
	}

	/*
	 * FEAT_NONE marks a grid the town does not occupy.  The surface skips
	 * those when it draws the town in, leaving whatever the world put there.
	 */
	for (grid.y = 1; grid.y < h - 1; grid.y++)
		for (grid.x = 1; grid.x < w - 1; grid.x++)
			if (seen[grid.y * w + grid.x])
				square_set_feat(town, grid, FEAT_NONE);

	#undef TOWN_IS_SURROUND

	mem_free(seen);
	mem_free(queue);
}

/**
 * The town's layout, built on first use.
 *
 * Seeded from the world seed and the town's position, so it belongs to this
 * world and no other, and comes out identically however many times the surface
 * is rebuilt around it.
 */
static struct chunk *wild_town_chunk(struct wilderness *w, struct player *p)
{
	if (!wild_town) {
		wild_town = town_gen_wild(p, wild_block_seed(w, w->town_block.x,
													w->town_block.y));
		wild_town_strip_rock(wild_town);
	}

	return wild_town;
}

/**
 * Draw the town into the live surface, where the window covers it.
 *
 * Two kinds of grid are skipped.  The town's outermost ring is a permanent wall
 * that exists only because Angband levels need a boundary, and on a surface
 * that runs past the town in every direction it would be a wall around nothing.
 * The rest are the grids wild_town_strip_rock() cleared, where the world's own
 * terrain shows through.
 */
static void wild_draw_town(struct wilderness *w, struct player *p,
						   struct chunk *c, struct loc offset)
{
	struct chunk *town = wild_town_chunk(w, p);
	struct loc org = wild_town_origin(w);
	struct loc grid;

	if (!town)
		return;

	for (grid.y = 1; grid.y < town->height - 1; grid.y++)
		for (grid.x = 1; grid.x < town->width - 1; grid.x++) {
			struct loc dest = loc(org.x + grid.x - offset.x,
								  org.y + grid.y - offset.y);

			if (!square_in_bounds_fully(c, dest))
				continue;
			if (square_feat(town, grid)->fidx == FEAT_NONE)
				continue;

			square_set_feat(c, dest, square_feat(town, grid)->fidx);
		}
}

/**
 * Where a new character starts: the town's down staircase.
 *
 * Returned in world coordinates, since that is what the player carries.  The
 * staircase is found by looking for it rather than being remembered from
 * generation, which is what Angband's own town code does on reload.
 */
struct loc wild_town_start(struct wilderness *w, struct player *p)
{
	struct chunk *town = wild_town_chunk(w, p);
	struct loc org = wild_town_origin(w);
	struct loc grid;

	for (grid.y = 0; grid.y < town->height; grid.y++)
		for (grid.x = 0; grid.x < town->width; grid.x++)
			if (square_feat(town, grid)->fidx == FEAT_MORE)
				return loc(org.x + grid.x, org.y + grid.y);

	/* No staircase: start in the middle of the town and look for it. */
	return loc(org.x + town->width / 2, org.y + town->height / 2);
}

/**
 * Mark the town as known, and only the town (WLD-25).
 *
 * Angband knows the whole of depth zero from the first turn, which is right for
 * a level that is nothing but a town: you live there.  The surface is not a
 * town, and revealing all of it would give away the shape of the coast, where
 * the forests are and where the mountains stand -- everything the overworld is
 * meant to be walked to find out.
 *
 * So the town is known, as it was, and the country is not.  Beyond the town's
 * edge the ordinary rules apply: you learn the world by looking at it.
 */
void wild_town_known(struct wilderness *w, struct player *p, struct chunk *c,
					 struct loc offset)
{
	struct chunk *town = wild_town_chunk(w, p);
	struct loc org = wild_town_origin(w);
	struct loc grid;

	for (grid.y = 0; grid.y < z_info->town_hgt; grid.y++)
		for (grid.x = 0; grid.x < z_info->town_wid; grid.x++) {
			struct loc dest = loc(org.x + grid.x - offset.x,
								  org.y + grid.y - offset.y);

			if (!square_in_bounds_fully(c, dest))
				continue;

			/* Only the town itself, not the fields it was dropped into. */
			if (square_feat(town, grid)->fidx == FEAT_NONE)
				continue;

			square_memorize(c, dest);
		}
}

/**
 * Is this chunk the wilderness surface rather than a level?
 */
bool wild_is_surface(const struct chunk *c)
{
	return c && c->name && streq(c->name, "wilderness");
}

/**
 * Build the live wilderness surface around a world position (WLD-24).
 *
 * The overworld is one continuous surface, not a set of levels: a town and the
 * forest beyond it are the same map, and walking between them is walking, not a
 * level change.  Blocks are the unit of *generation* — each is laid out from
 * its own seed — but the level the player stands on tiles a window of them
 * together.
 *
 * \param p is the player, needed to lay the town out on first use.
 * \param centre is the world grid the window is built around.
 * \param offset returns the world grid of the surface's top-left corner, so the
 * caller can convert between world and level coordinates.
 */
struct chunk *wild_surface(struct wilderness *w, struct player *p,
						   struct loc centre, struct loc *offset)
{
	int size = z_info->wild_block_size;
	int view = wild_view_blocks();
	int span = view * size;
	int half = span / 2;
	struct chunk *c;
	struct loc grid;
	int world_max = w->blocks * size;
	int ox, oy;

	/* Keep the window inside the world rather than off its edge. */
	ox = MIN(MAX(centre.x - half, 0), MAX(0, world_max - span));
	oy = MIN(MAX(centre.y - half, 0), MAX(0, world_max - span));

	/* Align to block boundaries: blocks generate whole, not in pieces. */
	ox -= ox % size;
	oy -= oy % size;

	c = cave_new(span, span);
	c->depth = 0;
	c->name = string_make("wilderness");

	for (grid.y = 0; grid.y < span; grid.y++) {
		for (grid.x = 0; grid.x < span; grid.x++) {
			int wx = ox + grid.x, wy = oy + grid.y;
			int bx = wx / size, by = wy / size;
			struct wild_block *block = wild_block_at(w, bx, by);
			int feat;

			if (!block) {
				/* Beyond the world's edge: impassable rock. */
				square_set_feat(c, grid, FEAT_ROCK);
				continue;
			}

			/*
			 * Draw from a stream fixed by the block and the grid's position
			 * within it, so a grid's terrain does not depend on which window
			 * it happened to be drawn in.
			 */
			{
				uint32_t h = wild_block_seed(w, bx, by);

				h ^= (uint32_t) (wx % size) * 0x27220A95u;
				h ^= (uint32_t) (wy % size) * 0x165667B1u;
				h ^= h >> 15;
				h *= 0x2545F491u;
				h ^= h >> 13;

				feat = wild_terrain_feat(block->terrain, h % 100);
			}

			square_set_feat(c, grid, feat);
		}
	}

	/* Roads run east-west through the middle of the blocks that carry them. */
	for (int by = 0; by < view; by++) {
		for (int bx = 0; bx < view; bx++) {
			struct wild_block *block =
				wild_block_at(w, ox / size + bx, oy / size + by);

			if (!block || !(block->info & (WILD_INFO_ROAD | WILD_INFO_TRACK)))
				continue;

			grid.y = by * size + size / 2;
			for (grid.x = bx * size; grid.x < (bx + 1) * size; grid.x++)
				square_set_feat(c, grid, FEAT_ROAD);
		}
	}

	/*
	 * Draw the town in.  It goes down after the terrain and the roads, so the
	 * countryside runs up to its edge rather than through it.
	 */
	{
		struct loc org = wild_town_origin(w);

		if (org.x + z_info->town_wid > ox && org.x < ox + span &&
			org.y + z_info->town_hgt > oy && org.y < oy + span)
			wild_draw_town(w, p, c, loc(ox, oy));
	}

	/*
	 * The world has an edge, and Angband levels expect a permanent boundary --
	 * a great deal of code steps one grid outwards without checking.  Where the
	 * window sits against the edge of the world, give it one.  Everywhere else
	 * the window scrolls before the player can reach its border, so the border
	 * is never stood next to and needs no wall.
	 */
	for (grid.x = 0; grid.x < span; grid.x++) {
		if (oy == 0)
			square_set_feat(c, loc(grid.x, 0), FEAT_PERM);
		if (oy + span >= world_max)
			square_set_feat(c, loc(grid.x, span - 1), FEAT_PERM);
	}
	for (grid.y = 0; grid.y < span; grid.y++) {
		if (ox == 0)
			square_set_feat(c, loc(0, grid.y), FEAT_PERM);
		if (ox + span >= world_max)
			square_set_feat(c, loc(span - 1, grid.y), FEAT_PERM);
	}

	if (offset) {
		offset->x = ox;
		offset->y = oy;
	}

	/* Mark the blocks under the window as seen. */
	for (int by = 0; by < view; by++)
		for (int bx = 0; bx < view; bx++) {
			struct wild_block *block =
				wild_block_at(w, ox / size + bx, oy / size + by);
			if (block)
				block->info |= WILD_INFO_SEEN;
		}

	return c;
}

/**
 * Should the live surface be rebuilt around the player?
 *
 * The window is nine blocks across, so the player can walk a long way before
 * running out of it.  Rebuilding is not free — it regenerates every block in
 * the window — so it happens when the player comes within a block's width of an
 * edge, not on every step across a block boundary.
 *
 * Returns false when the window is already against the world's edge on that
 * side: there is nothing further to scroll to, and rebuilding would produce an
 * identical surface.
 */
bool wild_needs_recentre(struct player *p)
{
	int size = z_info->wild_block_size;
	int span = wild_view_blocks() * size;
	int world_max = wild ? wild->blocks * size : 0;
	int lx = p->wild_grid.x - p->wild_offset.x;
	int ly = p->wild_grid.y - p->wild_offset.y;

	if (!wild)
		return false;

	/* Near the west or east edge, with world left in that direction. */
	if (lx < size && p->wild_offset.x > 0)
		return true;
	if (lx >= span - size && p->wild_offset.x + span < world_max)
		return true;

	/* Near the north or south edge. */
	if (ly < size && p->wild_offset.y > 0)
		return true;
	if (ly >= span - size && p->wild_offset.y + span < world_max)
		return true;

	return false;
}

/**
 * Move the player one step across the world, keeping level and world position
 * in step (WLD-23).
 *
 * The player's position is held in world coordinates; their position on the
 * live surface is derived from it.  Keeping the world position as the truth
 * means the surface can be rebuilt beneath them without their location being
 * disturbed.
 */
void wild_track_move(struct player *p, struct loc grid)
{
	p->wild_grid.x = p->wild_offset.x + grid.x;
	p->wild_grid.y = p->wild_offset.y + grid.y;
}
