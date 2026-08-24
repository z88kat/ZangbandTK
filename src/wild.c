/**
 * \file wild.c
 * \brief The wilderness overworld (ZangbandTK)
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

#include "angband.h"
#include "cave.h"
#include "game-world.h"
#include "generate.h"
#include "init.h"
#include "mon-make.h"
#include "obj-pile.h"
#include "obj-util.h"
#include "player.h"
#include "player-history.h"
#include "dun-type.h"
#include "player-calcs.h"
#include "wild.h"

struct wilderness *wild = NULL;

/**
 * The window the surface was last built at.
 *
 * A rebuild is triggered by either axis nearing its edge, but it used to
 * re-anchor both -- so walking a long way due west re-centred the window
 * vertically as well, in whole blocks, and the character jumped a dozen rows up
 * or down the screen without having moved north or south at all.
 */
static struct loc wild_window = { 0, 0 };
static bool wild_window_set = false;

/** How far the window moved when it was last rebuilt, in grids. */
static struct loc wild_scroll = { 0, 0 };

/** What the player knows of the surface, held while they are off it (WLD-25). */
static struct chunk *wild_known = NULL;
static struct loc wild_known_offset = { 0, 0 };

/**
 * Which town the status line was last told about; -2 for "not yet told".
 *
 * At file scope with the rest of the surface's state, and cleared by
 * wild_cleanup() with the rest of it, because it belongs to a world rather than
 * to the process.  Left inside wild_track_move() as a function static it
 * outlived the character it described: dying in the starting village and
 * beginning another one there left this reading 0 for the new character too, so
 * their first step matched and never asked for the redraw, and the status line
 * kept the dead character's location until something else happened to redraw
 * it.  Loading a second savefile in one session did the same.
 */
static int wild_shown_town = -2;

/**
 * Sea covers roughly this fraction of the world, as in Zangband.
 */
#define WILD_SEA_FRACTION 4

/**
 * How many blocks of open sea the world ends in, on every side.
 */
#define WILD_SEA_MARGIN 3

/** A lake is shaped by a plasma fractal this many blocks across; 2^n + 1. */
#define WILD_LAKE_SIZE 9

/** Everything in a lake's fractal below this becomes water. */
#define WILD_LAKE_CUT 96

/** Grids wetter than this are water; the rest are the land they run through. */
#define WILD_WATER_CUT 1

/** Half the width of a river, in grids, before its banks are roughened. */
#define WILD_RIVER_WIDTH 3

/** Wetter than this and the water is over your head. */
#define WILD_WATER_DEEP 140

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
	w->town_at = mem_zalloc((size_t) blocks * blocks * sizeof(*w->town_at));

	return w;
}

void wild_free(struct wilderness *w)
{
	if (!w)
		return;

	while (w->relics) {
		struct wild_relic *relic = w->relics;

		w->relics = relic->next;
		object_free(relic->obj);
		mem_free(relic);
	}

	while (w->uniques) {
		struct wild_unique *seen = w->uniques;

		w->uniques = seen->next;
		mem_free(seen);
	}

	mem_free(w->town_at);
	mem_free(w->map);
	mem_free(w);
}

/**
 * ------------------------------------------------------------------------
 * Rivers and lakes (WLD-08)
 * ------------------------------------------------------------------------ */

/**
 * Mark a run of blocks between two points as carrying water.
 *
 * Ported from Zangband's link_river()
 * ([wild1.c:2133](../../archive/zangband/src/wild1.c#L2133)).  A long span is
 * halved at a point pushed sideways from the midpoint and each half drawn the
 * same way, so the line arrives crooked; only short spans are drawn straight.
 * That recursion is the whole trick, and it is why the rivers wander instead of
 * ruling themselves across the map.
 */
static void wild_link_river(struct wilderness *w, int x1, int y1,
							int x2, int y2)
{
	int dx = x2 - x1, dy = y2 - y1;
	int length = MAX(ABS(dx), ABS(dy));
	int l;

	if (length > 6) {
		int xn, yn, changex = 0, changey = 0;

		dx /= 2;
		dy /= 2;

		/* Push the midpoint sideways, by up to half the span it turns on. */
		if (dy) changex = randint1(ABS(dy)) - ABS(dy) / 2;
		if (dx) changey = randint1(ABS(dx)) - ABS(dx) / 2;

		xn = MIN(MAX(x1 + dx + changex, 0), w->blocks - 1);
		yn = MIN(MAX(y1 + dy + changey, 0), w->blocks - 1);

		wild_link_river(w, x1, y1, xn, yn);
		wild_link_river(w, xn, yn, x2, y2);
		return;
	}

	for (l = 0; l <= length; l++) {
		struct wild_block *block;
		int x = length ? x1 + l * (x2 - x1) / length : x1;
		int y = length ? y1 + l * (y2 - y1) / length : y1;

		block = wild_block_at(w, x, y);
		if (block)
			block->info |= WILD_INFO_WATER;
	}
}

/**
 * Run rivers down the map (WLD-08).
 *
 * Zangband's method
 * ([wild1.c:2205](../../archive/zangband/src/wild1.c#L2205)): scatter source
 * points evenly, sort them by height, and repeatedly join the highest unused
 * one to whichever remaining point is nearest.  Water therefore runs downhill
 * and towards the sea without anything having to model flow, and a point that
 * turns out to be below sea level is struck off so that rivers end at the coast
 * rather than fanning into deltas.
 */
static void wild_place_rivers(struct wilderness *w)
{
	int n = z_info->wild_rivers;
	int count = n * n;
	int *px = mem_zalloc(count * sizeof(int));
	int *py = mem_zalloc(count * sizeof(int));
	int sea = 256 / WILD_SEA_FRACTION;
	int i, j, cur;

	/* One source per cell of an n by n grid, placed at random within it. */
	for (i = 0; i < count; i++) {
		int lo = ((i % n) * w->blocks) / n;
		int hi = lo + w->blocks / n - 1;

		px[i] = rand_range(lo, MAX(lo, hi));

		lo = ((i / n) * w->blocks) / n;
		hi = lo + w->blocks / n - 1;
		py[i] = rand_range(lo, MAX(lo, hi));
	}

	/* Highest first.  Insertion sort: there are sixteen of them. */
	for (i = 1; i < count; i++) {
		int hx = px[i], hy = py[i];
		int h = w->map[hy * w->blocks + hx].hgt;

		for (j = i; j > 0; j--) {
			int ph = w->map[py[j - 1] * w->blocks + px[j - 1]].hgt;

			if (ph >= h) break;
			px[j] = px[j - 1];
			py[j] = py[j - 1];
		}
		px[j] = hx;
		py[j] = hy;
	}

	for (cur = 0; cur < count - 1; cur++) {
		int best = -1, best_dist = 0;

		/* Struck off: this source was below sea level and has been used. */
		if (px[cur] < 0) continue;

		/* Everything from here down is sea; there is no river left to run. */
		if (w->map[py[cur] * w->blocks + px[cur]].hgt <= sea) break;

		for (i = cur + 1; i < count; i++) {
			int dx, dy, dist;

			if (px[i] < 0) continue;

			dx = px[i] - px[cur];
			dy = py[i] - py[cur];
			dist = MAX(ABS(dx), ABS(dy));

			if (best < 0 || dist < best_dist) {
				best = i;
				best_dist = dist;
			}
		}

		if (best < 0) break;

		wild_link_river(w, px[cur], py[cur], px[best], py[best]);

		/* A river that has reached the sea has finished. */
		if (w->map[py[best] * w->blocks + px[best]].hgt <= sea)
			px[best] = py[best] = -1;
	}

	mem_free(py);
	mem_free(px);
}

/**
 * Put lakes in the hollows (WLD-08).
 *
 * Zangband's method
 * ([wild1.c:2344](../../archive/zangband/src/wild1.c#L2344)): a small plasma
 * fractal dropped at a random spot, with everything under a cutoff becoming
 * water, and the attempt abandoned rather than moved if any of it would land in
 * the sea.  Attempts, not results -- a world with a lot of coast gets fewer
 * lakes, which is the right answer.
 */
static void wild_place_lakes(struct wilderness *w)
{
	int size = WILD_LAKE_SIZE;
	int *shape = mem_zalloc(size * size * sizeof(int));
	int sea = 256 / WILD_SEA_FRACTION;
	int attempt;

	for (attempt = 0; attempt < z_info->wild_lakes; attempt++) {
		int ox, oy, i, j;
		bool clear = true;

		if (w->blocks <= size) break;

		wild_plasma(shape, size, 20);

		ox = randint0(w->blocks - size);
		oy = randint0(w->blocks - size);

		/* Not if any of it would be at sea. */
		for (j = 0; j < size && clear; j++)
			for (i = 0; i < size && clear; i++) {
				struct wild_block *block;

				if (shape[j * size + i] > WILD_LAKE_CUT) continue;

				block = wild_block_at(w, ox + i, oy + j);
				if (!block || block->hgt <= sea)
					clear = false;
			}

		if (!clear) continue;

		for (j = 0; j < size; j++)
			for (i = 0; i < size; i++) {
				struct wild_block *block;

				if (shape[j * size + i] > WILD_LAKE_CUT) continue;

				block = wild_block_at(w, ox + i, oy + j);
				if (block)
					block->info |= WILD_INFO_WATER;
			}
	}

	mem_free(shape);
}

/**
 * Squared distance from a point to a line segment, in grid units.
 *
 * Integer throughout: the inputs are grid coordinates and the result is only
 * ever compared against a squared width, so nothing is gained by leaving them.
 */
static int wild_dist2_to_segment(int px, int py, int ax, int ay, int bx, int by)
{
	int vx = bx - ax, vy = by - ay;
	int wx = px - ax, wy = py - ay;
	int len2 = vx * vx + vy * vy;
	int dot, cx, cy;

	if (len2 == 0)
		return wx * wx + wy * wy;

	dot = wx * vx + wy * vy;
	if (dot <= 0)
		return wx * wx + wy * wy;
	if (dot >= len2) {
		int ex = px - bx, ey = py - by;

		return ex * ex + ey * ey;
	}

	/* Foot of the perpendicular, rounded to the nearest grid. */
	cx = ax + (vx * dot) / len2;
	cy = ay + (vy * dot) / len2;

	return (px - cx) * (px - cx) + (py - cy) * (py - cy);
}

/**
 * How wet a world grid is, from 0 (dry) to 255 (mid-channel).
 *
 * Zangband drew each water block by running a plasma fractal whose corners were
 * weighted by which neighbouring blocks carried water, so that a river joined up
 * across block boundaries and tapered into the land
 * ([wild3.c:1588](../../archive/zangband/src/wild3.c#L1588)).  The idea is kept
 * -- water is a field over the land rather than a terrain kind, and it is
 * continuous across blocks -- but the mechanism is ours, because our grids are
 * drawn from a hash of their own position and there is no per-block scratch
 * buffer to run a fractal in (W-1).
 *
 * The flagged blocks are read as a *path*: each water block is joined to its
 * water neighbours by a segment between their centres, and a grid is wet by how
 * close it lies to the nearest such segment.  That is what makes a river a
 * river.  An earlier attempt interpolated the flags as a field between block
 * centres, which sounds equivalent and is not: a linear ramp from 255 to 0 over
 * sixteen grids puts a broad band of near-threshold values across the
 * countryside, so the result was a fourteen-grid-wide channel with speckles of
 * open water scattered through the fields on either side.
 *
 * A block with water on most sides is not a river but the middle of a lake, and
 * fills.
 */
int wild_water_at(struct wilderness *w, int x, int y)
{
	int size = z_info->wild_block_size;
	int half = size / 2;
	int bx = x / size, by = y / size;
	struct wild_block *block = wild_block_at(w, bx, by);
	int neighbours = 0, best = -1;
	int width, jitter, i, j;
	uint32_t h;

	if (!block || !(block->info & WILD_INFO_WATER))
		return 0;

	/* The centre of this block, and of each neighbour, in world grids. */
	for (j = -1; j <= 1; j++)
		for (i = -1; i <= 1; i++) {
			struct wild_block *n;
			int d2;

			if (!i && !j) continue;

			n = wild_block_at(w, bx + i, by + j);
			if (!n || !(n->info & WILD_INFO_WATER)) continue;

			neighbours++;

			d2 = wild_dist2_to_segment(x, y,
									   bx * size + half, by * size + half,
									   (bx + i) * size + half,
									   (by + j) * size + half);

			if (best < 0 || d2 < best)
				best = d2;
		}

	/*
	 * Ragged banks.  The width wobbles per grid rather than the wetness, so
	 * the edge of the water is uneven without stray pools appearing in the
	 * fields a bowshot away.
	 */
	h = (uint32_t) x * 0x27220A95u ^ (uint32_t) y * 0x165667B1u ^
		(w ? w->seed : 0);
	h ^= h >> 15;
	h *= 0x2545F491u;
	h ^= h >> 13;
	jitter = (int) (h % 3) - 1;

	/* Surrounded by water: this is the middle of a lake, not a bank. */
	if (neighbours >= 5)
		return 255;

	/* Flagged but joined to nothing: a pool the size of the block's heart. */
	if (best < 0) {
		int dx = x - (bx * size + half), dy = y - (by * size + half);

		best = dx * dx + dy * dy;
	}

	width = WILD_RIVER_WIDTH + jitter;
	if (width < 1) width = 1;

	if (best > width * width)
		return 0;

	/* Deepest mid-channel, shallowing towards the bank. */
	return 255 - (int) ((int64_t) best * 255 / (width * width));
}

/**
 * The size of each band of town, in grids (WLD-11).
 *
 * The first is Angband's own town, and is the smallest there is: the player
 * starts in a village, and everything else they find is bigger.  Nothing here
 * exceeds the live surface, which is 144 grids square.
 */
static const struct { uint16_t wid, hgt; } wild_town_bands[] = {
	{  66, 22 },	/* village -- Angband's town, and where the player starts */
	{  88, 26 },	/* town */
	{ 110, 30 },	/* city */
	{ 132, 34 },	/* great city */
};

#define WILD_TOWN_BANDS ((int) N_ELEMENTS(wild_town_bands))


/**
 * What the starting village keeps.
 *
 * The village is the smallest place in the world -- it is the reference the
 * other bands are larger than -- so it holds only what a character cannot begin
 * without: somewhere to buy food and light, somewhere to buy a potion of cure
 * light wounds, somewhere to store what will not fit in a pack, and a
 * bookseller.
 *
 * The bookseller is here for a reason worth recording: no class begins with a
 * spellbook.  A mage starts with a rapier and a torch, so a village without one
 * would leave every caster unable to learn a spell until they had walked to
 * another town.
 *
 * What it does *not* hold -- the armoury, the weaponsmith, the magic shop, the
 * black market -- is what makes a larger town worth the walk.
 */
#define WILD_VILLAGE_STORES \
	((1u << WILD_STORE_GENERAL) | (1u << WILD_STORE_BOOK) | \
	 (1u << WILD_STORE_ALCHEMY) | (1u << WILD_STORE_HOME))

/**
 * How many blocks a town of this size covers, in each direction.
 *
 * A block of margin is added on every side: a town that runs right up to the
 * edge of its blocks has nowhere for a road to leave from, and looks wrong
 * butted against whatever the next block turns out to be.
 */
static void wild_town_extent_of(const struct wild_town *town, int *bw, int *bh)
{
	int size = z_info->wild_block_size;

	*bw = (town->wid + size - 1) / size + 2;
	*bh = (town->hgt + size - 1) / size + 2;
}

/**
 * The largest footprint any town can have, for keeping clear of the map edges.
 */
static void wild_town_extent(int *bw, int *bh)
{
	struct wild_town biggest = { { 0, 0 },
		wild_town_bands[WILD_TOWN_BANDS - 1].wid,
		wild_town_bands[WILD_TOWN_BANDS - 1].hgt, 0, 0, 0, NULL, 0, 0 };

	wild_town_extent_of(&biggest, bw, bh);
}

static int wild_town_popcount(uint16_t v)
{
	int n = 0;

	while (v) { n += v & 1; v >>= 1; }

	return n;
}

/**
 * Choose which stores a town holds (WLD-11a).
 *
 * Not every town has a black market, and a frontier village should not carry
 * the same shops as a city.  How many follows from the size band; which ones
 * follows from the land, which is WLD-15's scoring applied to 4.2's store list
 * rather than to a building catalogue.
 *
 * The general store and the home are in every town: one sells food and light,
 * the other is where the player's belongings live, and a town without either is
 * a town nobody would stop at.
 */
/**
 * The adjective for a quality tier, or NULL for the plain trade (WLD-16a).
 */
const char *wild_quality_name(int tier)
{
	if (tier <= 0 || tier > quality_tier_count) return NULL;

	return quality_tiers[tier - 1].name;
}

/**
 * How good a town's shop of a given trade is (WLD-15, WLD-16a).
 *
 * Zangband scored every building on population, magic and law plus a rarity,
 * and that scoring is the whole point of the quality ladder: an arcane
 * weaponsmith you have to travel to find is worth having, and one in every town
 * is wallpaper.  So the tier comes out of the country the town stands in.
 *
 * Which axis matters depends on the trade, because the ladders are not the same
 * ladder.  A magic shop or a bookseller climbs on magic -- that is what there is
 * more of to sell.  Arms and armour climb on people and on order, since a
 * smith needs a town that can keep him busy and a road the steel can arrive by,
 * with magic counting for something because the top of that ladder is
 * enchanted.  A general store climbs on people alone: there is no arcane bread.
 *
 * The band counts too -- a great city is a better place to trade than a village
 * whatever the country is like -- and the block's own seed breaks ties, so two
 * equally favoured towns need not come out the same.
 *
 * Returns 0 for the plain trade, up to quality_tier_count.
 *
 * \param town is the index into w->towns, since town 0 is a special case.
 * \param store is a WILD_STORE_* index.
 */
int wild_store_quality(struct wilderness *w, int town, int store)
{
	struct wild_town *t;
	struct wild_block *block;
	uint32_t seed;
	int pop, law, magic, score, tier;

	if (!w || town < 0 || town >= w->town_count) return 0;
	if (!quality_tier_count) return 0;

	/*
	 * Home has no stock to be better, and the black market is already the top
	 * of every ladder -- it sells whatever it likes at whatever depth the
	 * player has reached, so a tier on top of that would be a second opinion
	 * about the same thing.
	 */
	if (store == WILD_STORE_HOME || store == WILD_STORE_BLACK) return 0;

	/*
	 * And home town is plain, by WLD-12.  The opening should not depend on
	 * procedural luck in either direction: a character who starts next to an
	 * arcane weaponsmith has a different game from one who does not, and
	 * neither of them chose it.
	 */
	if (town == 0) return 0;

	t = &w->towns[town];
	block = wild_block_at(w, t->block.x, t->block.y);
	if (!block) return 0;

	pop = block->pop;
	law = block->law;
	magic = block->magic;
	seed = wild_block_seed(w, t->block.x, t->block.y);

	switch (store) {
		case WILD_STORE_MAGIC:
		case WILD_STORE_BOOK:
			score = magic * 3 / 4 + pop / 4;
			break;
		case WILD_STORE_ALCHEMY:
			score = magic / 2 + pop / 2;
			break;
		case WILD_STORE_WEAPON:
		case WILD_STORE_ARMOR:
			score = pop / 2 + law / 4 + magic / 4;
			break;
		default:
			score = pop * 3 / 4 + law / 4;
			break;
	}

	/* Size tells, and so does the place itself. */
	score += t->band * 10;
	score += (int) ((seed >> (store * 3 + 5)) & 31) - 16;

	/*
	 * Thresholds taken from the measured distribution rather than guessed, at
	 * its 60th, 85th and 97th centiles, over 40 worlds and 1,872 shops.  Guessed
	 * first, and the guess was badly wrong: 190/160/130 put a quarter of every
	 * trade in the world on the top rung, and the tiers came out in the wrong
	 * order because everything above the highest threshold piles into it.
	 *
	 * The reason the guess failed is worth keeping.  Towns are *sited* on law
	 * (WLD-08a), so law at a town block is not a free variable: measured over
	 * 479 towns it runs 104 to 254 with a mean of 208, which is nearly a
	 * constant offset rather than an axis.  Population spans its whole range and
	 * magic is very close to uniform, because nothing selects for it -- so those
	 * two are what actually decide the tier, and it was magic being the only
	 * untouched axis that made it worth adding one.
	 *
	 * See the-quality-ladder-is-a-ladder in tests/game/wild.c, which fails if
	 * this flattens out at either end.
	 */
	if (score >= 261) tier = 3;
	else if (score >= 225) tier = 2;
	else if (score >= 183) tier = 1;
	else tier = 0;

	return MIN(tier, quality_tier_count);
}

static uint16_t wild_town_stores(struct wilderness *w, int bx, int by, int band)
{
	struct wild_block *block = wild_block_at(w, bx, by);
	uint16_t held = (1u << WILD_STORE_GENERAL) | (1u << WILD_STORE_HOME);
	int pop = block ? block->pop : 128;
	int law = block ? block->law : 128;
	uint32_t seed = wild_block_seed(w, bx, by);
	int guard = 0;
	int want;

	/*
	 * How many trades a place can keep.  Every band overlaps the next only at
	 * its edges, so a town always out-stocks the starting village and a great
	 * city usually out-stocks a town -- but the count is drawn from the place
	 * itself, so two towns of the same size need not keep the same trades.
	 */
	switch (band) {
		case 0:  want = 3; break;   /* hamlet */
		case 1:  want = 5; break;   /* town */
		case 2:  want = 6; break;   /* city */
		default: want = 7; break;   /* great city */
	}
	want += (int) ((seed >> 24) & 1);

	/* Weight each remaining store by what the place is like. */
	while (guard++ < 100) {
		int best = -1, best_score = -1, n;

		for (n = 0; n < (int) z_info->store_max && n < 16; n++) {
			int score;

			if (held & (1u << n)) continue;

			switch (n) {
				/* Arms and armour follow people and order. */
				case WILD_STORE_ARMOR:
				case WILD_STORE_WEAPON: score = 60 + pop / 8 + law / 16; break;
				/* Learning gathers where it is safe to gather. */
				case WILD_STORE_BOOK:   score = 40 + law / 4; break;
				case WILD_STORE_ALCHEMY: score = 55 + pop / 8; break;
				case WILD_STORE_MAGIC:  score = 20 + pop / 6 + law / 8; break;
				/* And the black market keeps out of the light. */
				case WILD_STORE_BLACK:  score = 10 + (255 - law) / 4 + pop / 8; break;
				default:           score = 30; break;
			}

			/*
			 * Trades that score alike -- the armoury and the weaponsmith --
			 * would otherwise always fall the same way round, and every hamlet
			 * in the world would keep an armoury and no weaponsmith.  Let the
			 * place itself break the tie.  Reproducible: the same block of the
			 * same world always favours the same trades.
			 */
			score += (int) ((seed >> (n * 3)) & 7);

			if (score > best_score) { best_score = score; best = n; }
		}

		if (best < 0) break;
		held |= 1u << best;

		if (wild_town_popcount(held) >= want) break;
	}

	return held;
}

/**
 * Towns stand where the law reaches.
 *
 * Zangband required `law > 230` of a town site
 * ([wild1.c:3328](../../archive/zangband/src/wild1.c#L3328)), and it is not
 * decoration: danger in the wilderness comes from law and nothing else (see
 * wild_danger()), so where a town stands decides how survivable its doorstep
 * is.  Measured without this, a new character walking three blocks out of town
 * met monsters of dungeon depth 20 to 53.
 *
 * A fixed threshold turned out to be the wrong shape for it.  Requiring the
 * whole footprint above a cutoff fails on most worlds -- thirty-odd blocks all
 * lawful at once is a rare thing -- and relaxing the cutoff until something
 * passes lands wherever the ladder happens to stop, which is not a choice at
 * all.  Scoring the site on the worst danger within a short walk of it asks the
 * question directly, always answers, and picks the safest doorstep the world
 * has to offer rather than the first adequate one.
 */
#define WILD_TOWN_REACH		3

/** How far a unique may have wandered from where the player left it. */
#define WILD_UNIQUE_WANDER	5

/** No two towns closer than this, in blocks. */
#define WILD_TOWN_APART		12

/**
 * Score a site for a town, or return a large negative if it will not do.
 *
 * The dominant term is the danger within a short walk, because that is what
 * decides whether the doorstep is survivable.  Everything else is a tie-break
 * between sites that are already safe to walk out of.
 */
static int wild_town_score(struct wilderness *w, int x, int y,
						   const struct wild_town *shape)
{
	struct wild_block *block = wild_block_at(w, x, y);
	int centre = w->blocks / 2;
	int score, sum = 0, count = 0, dist, fx, fy, bw, bh, reach;

	if (!block) return -100000;

	switch (block->terrain) {
		case WILD_TERRAIN_GRASS: score = 120; break;
		case WILD_TERRAIN_FOREST: score = 90; break;
		case WILD_TERRAIN_WASTE: score = 40; break;
		default: return -100000;	/* sea, shore, swamp, mountain */
	}

	wild_town_extent_of(shape, &bw, &bh);
	reach = MAX(bw, bh) / 2 + WILD_TOWN_REACH;

	for (fy = y - reach; fy <= y + reach; fy++)
		for (fx = x - reach; fx <= x + reach; fx++) {
			struct wild_block *f = wild_block_at(w, fx, fy);

			if (!f) continue;

			/* The town's own ground has to be buildable and dry. */
			if (ABS(fx - x) <= bw / 2 && ABS(fy - y) <= bh / 2 &&
				(f->terrain == WILD_TERRAIN_OCEAN ||
				 f->terrain == WILD_TERRAIN_MOUNTAIN ||
				 (f->info & WILD_INFO_WATER) || f->place))
				return -100000;

			sum += wild_danger(w, fx, fy);
			count++;
		}

	/*
	 * The mean and not the worst: over a window this size the worst block is
	 * near the maximum almost everywhere, so it carries no signal.  What
	 * distinguishes a good site from a bad one is whether the whole
	 * neighbourhood is orderly, which is what the mean measures.
	 */
	if (count) score -= (sum / count) * 8;

	score += block->pop / 4;

	/* All else equal, nearer the middle of the world. */
	dist = MAX(ABS(x - centre), ABS(y - centre));
	score -= dist * 2;

	return score;
}

/**
 * Mark the ground a town stands on, and a block of margin around it, as spoken
 * for.
 *
 * It used to flag those blocks as carrying a road as well.  That was WLD-08's
 * stub, which the requirement asked to be replaced rather than extended, and
 * wild_place_roads() replaced it: roads are now routed between towns instead of
 * being a ring around each one.
 */
static void wild_town_claim(struct wilderness *w, const struct wild_town *town)
{
	int bw, bh, fx, fy;

	wild_town_extent_of(town, &bw, &bh);

	for (fy = town->block.y - bh / 2; fy <= town->block.y + bh / 2; fy++)
		for (fx = town->block.x - bw / 2; fx <= town->block.x + bw / 2; fx++) {
			struct wild_block *f = wild_block_at(w, fx, fy);

			if (!f) continue;
			f->place = 1;
		}
}

/**
 * Who lives in the town on this block (WLD-11).
 *
 * Law decides it, mostly.  A town needs settled country to stand in at all, so
 * every site has some law to it -- but the range across the towns of a world
 * runs from about 170 to 255, and the bottom of that is country nobody is
 * policing.  A town there has more often than not been taken.
 *
 * Population is the second axis, as it is for the trades: a place the land can
 * barely support is a place the animals get back.
 *
 * The starting village is always villagers.  WLD-12 says the opening should not
 * depend on procedural luck, and beginning in a town held by monsters is the
 * worst luck the world could deal.
 */
static int wild_town_folk(struct wilderness *w, int bx, int by, bool starting,
						  int band)
{
	struct wild_block *block = wild_block_at(w, bx, by);
	uint32_t seed = wild_block_seed(w, bx, by);
	int law = block ? block->law : 255;
	int pop = block ? block->pop : 128;

	if (starting || !block)
		return WILD_FOLK_VILLAGER;

	/*
	 * Lawless country: the town is held by whatever took it.  A larger place
	 * holds out longer -- a great city can defend itself where a hamlet cannot
	 * -- which is both the obvious reading and the one that costs the player
	 * less: losing a village of four trades is a nuisance, losing the only
	 * great city within reach is the black market and the magic shop gone.
	 */
	if (law < 185 - band * 15)
		return WILD_FOLK_MONSTER;

	/*
	 * Or emptied entirely.  Rare, and not tied to either axis: a town stands
	 * empty for reasons the parameter space does not carry -- a plague, a war,
	 * a shadow that went wrong -- so it is drawn from the block's own seed.
	 */
	if (((seed >> 9) & 15) == 0)
		return WILD_FOLK_ABANDONED;

	/* Thinly peopled country: emptied once, and the animals moved back in. */
	if (pop < 110)
		return WILD_FOLK_BEAST;

	return WILD_FOLK_VILLAGER;
}

/** What to call a town's inhabitants, for the manual and for tests. */
const char *wild_folk_name(int folk)
{
	switch (folk) {
		case WILD_FOLK_VILLAGER:  return "villagers";
		case WILD_FOLK_BEAST:     return "beasts";
		case WILD_FOLK_MONSTER:   return "monsters";
		case WILD_FOLK_ABANDONED: return "abandoned";
		default:                  return "?";
	}
}

/**
 * Which services a town keeps (WLD-15, WLD-16).
 *
 * The same idea the trades are chosen by: score the building against the country
 * and the size of the place.  A magetower is the work of people with the leisure
 * and the order to build one, so it wants population and law both -- and it is
 * no use in a town nobody can reach, which is why the smallest band never has
 * one.  A town that has fallen has one only in the sense that the building is
 * still standing.
 */
static uint16_t wild_town_services(struct wilderness *w, int bx, int by,
								   int band, int folk, bool starting)
{
	struct wild_block *block = wild_block_at(w, bx, by);
	uint16_t held = 0;

	/*
	 * The starting village always has a tower, whatever its size and country
	 * would say.  Every journey begins at home, and a network you cannot leave
	 * from is a network with one fewer node than it needs: without this the
	 * player has to walk to a city before they can travel anywhere at all,
	 * every time.  The same reasoning as WLD-12's fixed store set -- the
	 * opening should not turn on procedural luck.
	 */
	if (starting)
		return 1u << WILD_SERVICE_MAGETOWER;

	if (!block || band < 1)
		return 0;

	/* Nobody is running a teleport network out of a town held by monsters. */
	if (folk == WILD_FOLK_MONSTER || folk == WILD_FOLK_ABANDONED)
		return 0;

	/*
	 * By size, and stated plainly.
	 *
	 * The magetower was scored on population and law against a threshold at
	 * first, in the same style as the trades -- and the scoring was worse than
	 * useless, because it was not legible.  Measured on a real world: every one
	 * of its band-one towns scored between 112 and 126 against a threshold of
	 * 130, so a "town" never had a tower and only cities did, which nothing
	 * told the player.  Walking into two towns in a row and finding no tower in
	 * either is how it was reported.
	 *
	 * So these are laid out by band instead.  A rule the player can hold in
	 * their head is worth more than variation they cannot see the shape of:
	 *
	 *   town         a way out, a bed, and somebody to mend you
	 *   city         and a magesmith, and somebody to recharge a wand
	 *   great city   the same; there is nothing left to add yet
	 */
	held |= 1u << WILD_SERVICE_MAGETOWER;
	held |= 1u << WILD_SERVICE_HEALER;
	held |= 1u << WILD_SERVICE_INN;

	if (band >= 2) {
		held |= 1u << WILD_SERVICE_ENCHANT;
		held |= 1u << WILD_SERVICE_RECHARGE;
	}

	return held;
}

/**
 * Put the towns on the map (WLD-10, WLD-11, WLD-12).
 *
 * The village goes first and gets the best doorstep the world has, because that
 * is where the player begins and the first hour has to be survivable.  The rest
 * are then placed in turn, each on the best remaining site far enough from the
 * others, and each sized by how settled its country is -- so the great cities
 * end up where the people are, which is where you would expect to find them.
 */
static void wild_place_towns(struct wilderness *w)
{
	int size = w->blocks;
	int bw, bh, want = z_info->wild_towns;
	int x, y, n;

	wild_town_extent(&bw, &bh);
	w->town_count = 0;

	if (want > WILD_TOWNS_MAX) want = WILD_TOWNS_MAX;

	for (n = 0; n < want; n++) {
		struct wild_town town;
		int best = 0, band;
		bool found = false;

		memset(&town, 0, sizeof(town));

		for (y = bh; y < size - bh; y++)
			for (x = bw; x < size - bw; x++) {
				struct wild_block *block = &w->map[y * size + x];
				int score, i;
				bool crowded = false;

				/* Towns keep their distance from one another. */
				for (i = 0; i < w->town_count; i++)
					if (MAX(ABS(x - w->towns[i].block.x),
							ABS(y - w->towns[i].block.y)) < WILD_TOWN_APART)
						crowded = true;
				if (crowded) continue;

				/*
				 * The village is the smallest band there is; every other town
				 * is sized by how settled its country is.
				 */
				band = 0;
				if (n > 0) {
					if (block->pop > 200) band = 3;
					else if (block->pop > 150) band = 2;
					else if (block->pop > 100) band = 1;
				}

				town.wid = wild_town_bands[band].wid;
				town.hgt = wild_town_bands[band].hgt;

				score = wild_town_score(w, x, y, &town);
				if (score <= -100000) continue;

				if (!found || score > best) {
					found = true;
					best = score;
					w->towns[w->town_count].block = loc(x, y);
					w->towns[w->town_count].band = band;
					w->towns[w->town_count].wid = town.wid;
					w->towns[w->town_count].hgt = town.hgt;
				}
			}

		if (!found) break;

		{
			struct wild_town *placed = &w->towns[w->town_count];

			placed->stores = (w->town_count == 0) ? WILD_VILLAGE_STORES
				: wild_town_stores(w, placed->block.x, placed->block.y,
								   placed->band);
			placed->folk = wild_town_folk(w, placed->block.x, placed->block.y,
										  w->town_count == 0, placed->band);
			placed->services = wild_town_services(w, placed->block.x,
												  placed->block.y,
												  placed->band, placed->folk,
												  w->town_count == 0);
			wild_town_claim(w, placed);
			w->town_count++;
		}
	}

	/*
	 * A world with nowhere habitable at all is possible in principle, and a
	 * character has to start somewhere.  Put the village on the driest, flattest
	 * block there is rather than leaving it at (0, 0), off the map's edge.
	 */
	if (!w->town_count) {
		int best_hgt = -1;

		w->towns[0].block = loc(size / 2, size / 2);
		w->towns[0].wid = wild_town_bands[0].wid;
		w->towns[0].hgt = wild_town_bands[0].hgt;
		w->towns[0].band = 0;
		w->towns[0].stores = WILD_VILLAGE_STORES;
		w->towns[0].folk = WILD_FOLK_VILLAGER;

		/*
		 * Inside the same margin the placement loop above keeps.  Without it a
		 * town could be claimed at block 0,0, and wild_town_origin_of() would
		 * put its top-left corner at a negative world grid -- off the map, with
		 * the block index truncating towards zero so that block -1 tested as
		 * inside the town.
		 */
		for (y = bh; y < size - bh; y++)
			for (x = bw; x < size - bw; x++) {
				struct wild_block *b = &w->map[y * size + x];
				int score;

				if (b->terrain == WILD_TERRAIN_OCEAN) continue;
				if (b->info & WILD_INFO_WATER) continue;

				/*
				 * And nor may the ground it would stand on: this path exists
				 * for a world with nowhere good, not for a town in a lake.
				 */
				{
					int fx, fy, bw2, bh2;
					bool wet = false;

					wild_town_extent_of(&w->towns[0], &bw2, &bh2);
					for (fy = y - bh2 / 2; fy <= y + bh2 / 2 && !wet; fy++)
						for (fx = x - bw2 / 2; fx <= x + bw2 / 2 && !wet; fx++) {
							struct wild_block *f = wild_block_at(w, fx, fy);

							if (f && (f->info & WILD_INFO_WATER)) wet = true;
						}
					if (wet) continue;
				}

				score = 255 - ABS((int) b->hgt - 128);
				if (score > best_hgt) {
					best_hgt = score;
					w->towns[0].block = loc(x, y);
				}
			}

		/*
		 * The same services the ordinary path would have given it.  Asked for
		 * last, once the block is settled, because wild_town_services() reads
		 * the block it is told about -- and asked for at all because without
		 * it this village had none, magetower included, and WLD-12 promises
		 * every character a tower to leave home from.  A starting village with
		 * no tower is one the player has to walk out of on foot, across a world
		 * that had nowhere habitable in it.
		 */
		w->towns[0].services = wild_town_services(w, w->towns[0].block.x,
												  w->towns[0].block.y,
												  w->towns[0].band,
												  w->towns[0].folk, true);

		wild_town_claim(w, &w->towns[0]);
		w->town_count = 1;
	}
}

/**
 * The world grid of a town rectangle's top-left corner.
 */
struct loc wild_town_origin_of(const struct wilderness *w, int town)
{
	int size = z_info->wild_block_size;
	const struct wild_town *t;
	int cx, cy;

	if (!w || town < 0 || town >= w->town_count)
		return loc(0, 0);

	t = &w->towns[town];
	cx = t->block.x * size + size / 2;
	cy = t->block.y * size + size / 2;

	return loc(cx - t->wid / 2, cy - t->hgt / 2);
}

/** The starting village, which is where the player begins. */
struct loc wild_town_origin(const struct wilderness *w)
{
	return wild_town_origin_of(w, 0);
}

int wild_town_count(const struct wilderness *w)
{
	return w ? w->town_count : 0;
}

/**
 * Bring the block index up to date with the town list.
 *
 * Towns are only ever appended -- wild_place_towns() sites one, claims its
 * ground and then counts it -- so the usual case is filling in the blocks of
 * the towns added since the last call, which costs one town footprint apiece.
 * The count going backwards means the world is being placed again from the
 * start, and the index is thrown away rather than unpicked.
 *
 * A block is recorded for the first town that covers it, which is what the
 * list walk this replaces returned.  In practice no two towns can overlap:
 * every site is rejected if its footprint touches ground already claimed, and
 * a claim reaches a block further than the town itself does.
 */
static void wild_town_index(struct wilderness *w)
{
	int size = z_info->wild_block_size;

	if (w->towns_indexed > w->town_count) {
		memset(w->town_at, 0,
			   (size_t) w->blocks * w->blocks * sizeof(*w->town_at));
		w->towns_indexed = 0;
	}

	while (w->towns_indexed < w->town_count) {
		int i = w->towns_indexed;
		struct loc org = wild_town_origin_of(w, i);
		int x0 = org.x / size, x1 = (org.x + w->towns[i].wid - 1) / size;
		int y0 = org.y / size, y1 = (org.y + w->towns[i].hgt - 1) / size;
		int bx, by;

		for (by = y0; by <= y1; by++)
			for (bx = x0; bx <= x1; bx++) {
				int at;

				if (!wild_in_bounds(w, bx, by)) continue;

				at = by * w->blocks + bx;
				if (!w->town_at[at]) w->town_at[at] = (int16_t) (i + 1);
			}

		w->towns_indexed++;
	}
}

/**
 * Which town, if any, stands on this block.
 *
 * Distinct from the block's `place` mark, which covers the land reserved when
 * the town was sited -- a margin of a block on every side, so that a town does
 * not butt against whatever the next block turns out to be and so there is
 * somewhere for a road to leave from.  `place` answers "is this land spoken
 * for".  This answers "is a town here", which is the question the map and the
 * monster placement are really asking.
 */
int wild_town_at(struct wilderness *w, int bx, int by)
{
	if (!w || !wild_in_bounds(w, bx, by))
		return -1;

	wild_town_index(w);

	return w->town_at[by * w->blocks + bx] - 1;
}


/**
 * Give the towns their names (WLD-11).
 *
 * A settled town takes a name from the settled list, one that has fallen or
 * stands empty from the lawless list -- so what a place is called says something
 * about it before you arrive.
 *
 * No two towns in a world share a name: a road or a quest that names a town has
 * to mean one place.  Where a list runs short the other is borrowed from rather
 * than a name being repeated, and if both run out the town keeps NULL and is
 * described by its size alone, which is what the status line did before names
 * existed.
 *
 * Drawn from the world seed and nothing else, so a character always comes home
 * to the same names.  Which also means reordering town.txt renames every town of
 * every existing world -- said so in the file.
 */
static void wild_name_towns(struct wilderness *w)
{
	bool *used_settled = NULL, *used_lawless = NULL;
	int i;

	if (town_names.settled_count > 0)
		used_settled = mem_zalloc(town_names.settled_count *
								  sizeof(*used_settled));
	if (town_names.lawless_count > 0)
		used_lawless = mem_zalloc(town_names.lawless_count *
								  sizeof(*used_lawless));

	for (i = 0; i < w->town_count; i++) {
		struct wild_town *town = &w->towns[i];
		bool fallen = (town->folk == WILD_FOLK_MONSTER ||
					   town->folk == WILD_FOLK_ABANDONED);
		uint32_t seed = wild_block_seed(w, town->block.x, town->block.y);
		int pass;

		town->name = NULL;

		/*
		 * The list its character calls for first; then the other one, rather
		 * than leaving a town nameless while names go spare.
		 */
		for (pass = 0; pass < 2 && !town->name; pass++) {
			bool lawless = fallen ? (pass == 0) : (pass == 1);
			char **list = lawless ? town_names.lawless : town_names.settled;
			bool *used = lawless ? used_lawless : used_settled;
			int count = lawless ? town_names.lawless_count
							    : town_names.settled_count;
			int k;

			if (!count || !used) continue;

			/* From the town's own seed, then the next free one along. */
			for (k = 0; k < count; k++) {
				int at = (int) ((seed >> 3) % (uint32_t) count);

				at = (at + k) % count;

				if (used[at]) continue;

				used[at] = true;
				town->name = list[at];
				break;
			}
		}
	}

	mem_free(used_lawless);
	mem_free(used_settled);
}

/**
 * ------------------------------------------------------------------------
 * Where the dungeons open (WLD-14)
 * ------------------------------------------------------------------------ */

/**
 * How far apart dungeon mouths are kept, in blocks.
 *
 * Closer than the towns are, since a dungeon is a door rather than a place and
 * two of them within sight of each other is not absurd -- but far enough that
 * the world does not end up with the whole ladder of them in one valley, which
 * is exactly what happened when this was not enforced.
 */
#define WILD_DUNGEON_APART 8

/**
 * How far a road reaches either side of its centre line, in grids.
 *
 * One means three grids wide.  A road one grid wide can be walked straight past
 * -- and was: a road that turned a right angle in the block the player stood in
 * read as a road that ended at the beach, because a one-grid corner is a single
 * square of floor at right angles to the way you are going.
 */
#define WILD_ROAD_HALF 1

/** How far out of a gate the approach reaches before looking for the road. */
#define WILD_APPROACH_REACH 3

/**
 * How well this block suits a dungeon of this kind.
 *
 * The same idea the towns are sited by: each dungeon names the population and
 * height of country its mouth is found in, and the world is searched for the
 * block that matches best.  So the Caverns of Kolvir open high in settled
 * country and the Abyss opens in empty lowland, without either being placed by
 * hand.
 */
static int wild_dungeon_score(struct wilderness *w, int bx, int by,
							  const struct dun_type *type)
{
	struct wild_block *block = wild_block_at(w, bx, by);
	int score = 1000;

	if (!block)
		return -100000;

	/* Not in the sea, and not on ground already spoken for. */
	if (block->terrain == WILD_TERRAIN_OCEAN) return -100000;
	if (block->info & WILD_INFO_WATER) return -100000;
	if (block->place) return -100000;

	/* Nor where another dungeon has already opened, or too near one. */
	for (int i = 0; i < w->dungeon_count; i++)
		if (MAX(ABS(bx - w->dungeons[i].block.x),
				ABS(by - w->dungeons[i].block.y)) < WILD_DUNGEON_APART)
			return -100000;

	/* How near the country is to what this dungeon wants. */
	score -= ABS((int) block->pop - (int) type->pop);
	score -= ABS((int) block->hgt - (int) type->height);

	return score;
}

/**
 * Open the dungeons in the world (WLD-10, WLD-14).
 *
 * Every dungeon defined in dungeon.txt gets a mouth, so that the depth ladder
 * has no gap in it -- a world missing the dungeon that covers depths 40 to 75
 * would be a world a character could not get past depth 40 in.  Rarity decides
 * the order they are placed in, not whether they are placed at all: the best
 * sites go to the ones that are particular about where they are.
 */
static void wild_place_dungeons(struct wilderness *w)
{
	int size = w->blocks;
	int bsize = z_info->wild_block_size;
	int placed = 0, rarity;

	w->dungeon_count = 0;

	/* Commonest first, so the choosiest dungeons are not left the scraps. */
	for (rarity = 1; rarity <= 255 && placed < WILD_DUNGEONS_MAX; rarity++) {
		struct dun_type *type;

		for (type = dun_types; type; type = type->next) {
			int best = 0, bx = -1, by = -1;
			int x, y;

			if (type->rarity != rarity) continue;
			if (placed >= WILD_DUNGEONS_MAX) break;

			for (y = 1; y < size - 1; y++)
				for (x = 1; x < size - 1; x++) {
					int score = wild_dungeon_score(w, x, y, type);

					if (score <= -100000) continue;
					if (bx < 0 || score > best) {
						best = score;
						bx = x;
						by = y;
					}
				}

			if (bx < 0) continue;

			w->dungeons[placed].block = loc(bx, by);
			w->dungeons[placed].type = type->index;
			w->dungeons[placed].max_depth = 0;

			/*
			 * The mouth sits at the middle of its block, which is where a road
			 * through the block runs, so a road leads to the door rather than
			 * past it.
			 */
			w->dungeons[placed].grid = loc(bx * bsize + bsize / 2,
										   by * bsize + bsize / 2);

			w->map[by * size + bx].place = 1;
			placed++;

			/*
			 * Kept in step as we go, not set at the end: the site score reads
			 * it to keep the mouths apart, and with it left at zero until the
			 * last one every dungeon in the world piled into the same valley.
			 */
			w->dungeon_count = placed;
		}
	}
}

int wild_dungeon_count(const struct wilderness *w)
{
	return w ? w->dungeon_count : 0;
}

struct wild_dungeon *wild_dungeon_by_index(struct wilderness *w, int idx)
{
	if (!w || idx < 0 || idx >= w->dungeon_count)
		return NULL;

	return &w->dungeons[idx];
}

/**
 * Does a dungeon open anywhere in this block?
 *
 * The map draws whole blocks, so it asks by block; the surface and the stairs
 * ask by grid, since standing next to a dungeon's mouth is not standing on it.
 */
bool wild_dungeon_in_block(struct wilderness *w, int bx, int by)
{
	int i;

	if (!w) return false;

	for (i = 0; i < w->dungeon_count; i++)
		if (w->dungeons[i].block.x == bx && w->dungeons[i].block.y == by)
			return true;

	return false;
}

/**
 * What to call a place of this size (WLD-11).
 *
 * One table, read by the status line and by the world map's legend, so that the
 * word the player sees standing in a town is the word the map used to describe
 * it from a distance.
 */
const char *wild_band_name(int band)
{
	switch (band) {
		case 0:  return "village";
		case 1:  return "town";
		case 2:  return "city";
		case 3:  return "great city";
		default: return "place";
	}
}

/**
 * What a service is called, for the manual and for messages.
 */
const char *wild_service_name(int service)
{
	switch (service) {
		case WILD_SERVICE_MAGETOWER: return "magetower";
		case WILD_SERVICE_HEALER:    return "healer";
		case WILD_SERVICE_INN:       return "inn";
		case WILD_SERVICE_ENCHANT:   return "magesmith";
		case WILD_SERVICE_RECHARGE:  return "recharger";
		default:                     return "building";
	}
}

/**
 * Which service the player is standing on, or -1 for none (WLD-18).
 *
 * A service is a door with behaviour behind it, so the terrain is the record of
 * which one it is -- there is nothing else to look it up in.
 */
int wild_service_at(struct chunk *c, struct loc grid)
{
	if (!c || !square_in_bounds_fully(c, grid))
		return -1;

	switch (square(c, grid)->feat) {
		case FEAT_MAGETOWER: return WILD_SERVICE_MAGETOWER;
		case FEAT_HEALER:    return WILD_SERVICE_HEALER;
		case FEAT_INN:       return WILD_SERVICE_INN;
		case FEAT_MAGESMITH: return WILD_SERVICE_ENCHANT;
		case FEAT_RECHARGER: return WILD_SERVICE_RECHARGE;
		default:             return -1;
	}
}

/**
 * Which town this world grid is inside, or -1 for none.
 *
 * Grid-precise, unlike wild_town_at(), which answers by block and so counts a
 * grid in the same block as the town but outside its wall.  The status line
 * needs the exact question: standing at the gate is not standing in the market.
 */
int wild_town_here(struct wilderness *w, struct loc grid)
{
	int i;

	if (!w) return -1;

	for (i = 0; i < w->town_count; i++) {
		struct loc org = wild_town_origin_of(w, i);

		if (grid.x >= org.x && grid.x < org.x + w->towns[i].wid &&
			grid.y >= org.y && grid.y < org.y + w->towns[i].hgt)
			return i;
	}

	return -1;
}

/**
 * Note that the player has been inside this town (WLD-16c).
 *
 * The magetower carries people between places they already know, so it needs to
 * know which those are.  For a town that means having stood in it -- seeing it
 * across a field is not the same as having been there, and the first crossing
 * of the world should stay worth making.
 */
void wild_note_visit(struct wilderness *w, struct loc grid)
{
	int idx = wild_town_here(w, grid);

	if (idx >= 0)
		w->towns[idx].visited = 1;
}

/**
 * Has the player found this dungeon's mouth (WLD-16c)?
 *
 * Seeing it is enough, which is a lower bar than a town asks -- a mouth is a
 * staircase in a field and there is nothing to be inside of.  Answered from the
 * block map the world map already keeps, so it needs no state of its own and
 * comes back from a savefile with everything else the player has seen.
 */
bool wild_dungeon_found(struct wilderness *w, int idx)
{
	struct wild_dungeon *mouth = wild_dungeon_by_index(w, idx);

	if (!mouth) return false;

	return wild_seen(w, mouth->block.x, mouth->block.y);
}

/**
 * Which dungeon opens at this world grid, or -1 for none.
 */
int wild_dungeon_at(struct wilderness *w, struct loc grid)
{
	int i;

	if (!w) return -1;

	for (i = 0; i < w->dungeon_count; i++)
		if (loc_eq(w->dungeons[i].grid, grid))
			return i;

	return -1;
}

/**
 * ------------------------------------------------------------------------
 * Roads (WLD-08)
 * ------------------------------------------------------------------------ */

/**
 * What it costs to carry a road across a block.
 *
 * Roads are not drawn straight.  They are routed, at the cost of a shortest
 * path across the block map, so they run down valleys, keep out of the swamp
 * and go round a mountain rather than over it -- which is what makes a road
 * worth following rather than merely worth looking at.
 */
static int wild_road_cost(const struct wild_block *block)
{
	int cost;

	if (!block)
		return 10000;

	switch (block->terrain) {
		case WILD_TERRAIN_GRASS:    cost = 2; break;
		case WILD_TERRAIN_WASTE:    cost = 3; break;
		case WILD_TERRAIN_FOREST:   cost = 5; break;
		case WILD_TERRAIN_SHORE:    cost = 6; break;
		case WILD_TERRAIN_SWAMP:    cost = 14; break;
		case WILD_TERRAIN_MOUNTAIN: cost = 25; break;

		/* Roads do not go to sea, except where there is no other way. */
		default:                    cost = 500; break;
	}

	/* A river or a lake wants a ford or a bridge, which is work. */
	if (block->info & WILD_INFO_WATER)
		cost += 18;

	/* Where a road already runs, another costs nothing: roads share. */
	if (block->info & WILD_INFO_ROAD)
		cost = 1;

	return cost;
}

/**
 * Lay a road between two blocks, by the cheapest way across the country.
 *
 * Dijkstra over the block map with a binary heap.  The world is generated once,
 * and 129 x 129 blocks is small, so the cost of doing this properly is not
 * worth avoiding -- and a road that visibly avoids the mountains reads as a
 * road, where a straight line drawn over them does not.
 */
/**
 * Scratch for wild_route_road(), owned by wild_place_roads().
 *
 * Half a megabyte of it on a 129-block world, and a world is laid with up to
 * seventy roads: allocating and freeing it per road cost more than the routing
 * did.  Allocated once around the whole job instead, and cleared per road.
 */
struct wild_road_scratch {
	int32_t *dist;
	int *prev;
	bool *done;
	int *hnode;
	int32_t *hcost;
	int count;
};

static bool wild_route_road(struct wilderness *w, struct wild_road_scratch *s,
							struct loc from, struct loc to, bool by_sea)
{
	int size = w->blocks, count = size * size;
	int32_t *dist = s->dist;
	int *prev = s->prev;
	bool *done = s->done;
	int *hnode = s->hnode;
	int32_t *hcost = s->hcost;
	int hn = 0;

	int start = from.y * size + from.x, goal = to.y * size + to.x;
	int i, at;

	memset(done, 0, count * sizeof(*done));

	for (i = 0; i < count; i++) {
		dist[i] = INT32_MAX;
		prev[i] = -1;
	}

	#define ROAD_PUSH(_node, _cost) \
		do { \
			int _i = hn++; \
			hnode[_i] = (_node); \
			hcost[_i] = (_cost); \
			while (_i > 0) { \
				int _p = (_i - 1) / 2; \
				int _tn; \
				int32_t _tc; \
				if (hcost[_p] <= hcost[_i]) break; \
				_tn = hnode[_p]; hnode[_p] = hnode[_i]; hnode[_i] = _tn; \
				_tc = hcost[_p]; hcost[_p] = hcost[_i]; hcost[_i] = _tc; \
				_i = _p; \
			} \
		} while (0)

	dist[start] = 0;
	ROAD_PUSH(start, 0);

	while (hn > 0) {
		int node = hnode[0];
		int j = 0;

		/* Pop the cheapest. */
		hn--;
		hnode[0] = hnode[hn];
		hcost[0] = hcost[hn];
		for (;;) {
			int l = 2 * j + 1, r = l + 1, m = j, tn;
			int32_t tc;

			if (l < hn && hcost[l] < hcost[m]) m = l;
			if (r < hn && hcost[r] < hcost[m]) m = r;
			if (m == j) break;
			tn = hnode[m]; hnode[m] = hnode[j]; hnode[j] = tn;
			tc = hcost[m]; hcost[m] = hcost[j]; hcost[j] = tc;
			j = m;
		}

		if (done[node]) continue;
		done[node] = true;
		if (node == goal) break;

		{
			int bx = node % size, by = node / size;
			static const int dx[4] = { -1, 1, 0, 0 };
			static const int dy[4] = { 0, 0, -1, 1 };

			for (i = 0; i < 4; i++) {
				int nx = bx + dx[i], ny = by + dy[i], next;
				int32_t step;

				if (nx < 0 || ny < 0 || nx >= size || ny >= size) continue;

				next = ny * size + nx;
				if (done[next]) continue;

				/* Keep to the land unless this is the causeway pass. */
				if (!by_sea && w->map[next].terrain == WILD_TERRAIN_OCEAN)
					continue;

				step = dist[node] + wild_road_cost(&w->map[next]);
				if (step >= dist[next]) continue;

				dist[next] = step;
				prev[next] = node;
				ROAD_PUSH(next, step);
			}
		}
	}

	#undef ROAD_PUSH

	/* Walk the path back and mark it, if one was found at all. */
	{
		bool reached = (goal == start) || done[goal] || prev[goal] >= 0;

		if (reached)
			for (at = goal; at >= 0; at = prev[at])
				w->map[at].info |= WILD_INFO_ROAD;

		return reached;
	}
}

/**
 * Lay a road, keeping to the land if the land allows it.
 *
 * Two towns can end up on either side of an inland sea, and then there is no
 * road between them that does not get its feet wet.  Rather than let every
 * road take a short cut across a bay because the water happened to be cheaper
 * than the hills, the sea is closed off entirely and only opened when the first
 * attempt finds no way round at all.
 */
static void wild_lay_road(struct wilderness *w, struct wild_road_scratch *s,
						  struct loc from, struct loc to)
{
	if (!wild_route_road(w, s, from, to, false))
		(void) wild_route_road(w, s, from, to, true);
}

/**
 * Join the towns up with roads (WLD-08).
 *
 * Two passes.  The first builds a minimum spanning tree over the towns, which
 * is what guarantees the thing the player actually needs: that there is a road
 * out of the village, and that following roads reaches every other town in the
 * world.  The second adds a road between any two towns closer than
 * `wild:road-dist` blocks -- Zangband's ROAD_DIST, kept as the reference value
 * -- so that settled country ends up with a network rather than a single thread
 * through it.
 */
static void wild_place_roads(struct wilderness *w)
{
	int n = w->town_count;
	int count = w->blocks * w->blocks;
	struct wild_road_scratch scratch;
	bool *joined;
	int i, j;

	if (n < 2)
		return;

	scratch.count = count;
	scratch.dist = mem_zalloc(count * sizeof(*scratch.dist));
	scratch.prev = mem_zalloc(count * sizeof(*scratch.prev));
	scratch.done = mem_zalloc(count * sizeof(*scratch.done));
	scratch.hnode = mem_zalloc((4 * count + 8) * sizeof(*scratch.hnode));
	scratch.hcost = mem_zalloc((4 * count + 8) * sizeof(*scratch.hcost));

	joined = mem_zalloc(n * sizeof(*joined));
	joined[0] = true;

	/* Prim's: grow the tree from the village, nearest town first. */
	for (i = 1; i < n; i++) {
		int best_from = -1, best_to = -1, best = 0;
		int a, b;

		for (a = 0; a < n; a++) {
			if (!joined[a]) continue;

			for (b = 0; b < n; b++) {
				int d;

				if (joined[b]) continue;

				d = MAX(ABS(w->towns[a].block.x - w->towns[b].block.x),
						ABS(w->towns[a].block.y - w->towns[b].block.y));

				if (best_from < 0 || d < best) {
					best = d;
					best_from = a;
					best_to = b;
				}
			}
		}

		if (best_to < 0) break;

		wild_lay_road(w, &scratch, w->towns[best_from].block,
					  w->towns[best_to].block);
		joined[best_to] = true;
	}

	mem_free(joined);

	/* And the short hops between neighbours, for a network rather than a tree. */
	for (i = 0; i < n; i++)
		for (j = i + 1; j < n; j++) {
			int d = MAX(ABS(w->towns[i].block.x - w->towns[j].block.x),
						ABS(w->towns[i].block.y - w->towns[j].block.y));

			if (d <= (int) z_info->wild_road_dist)
				wild_lay_road(w, &scratch, w->towns[i].block,
							  w->towns[j].block);
		}

	/*
	 * And a spur to every dungeon mouth.
	 *
	 * Measured before this existed: six of the thirteen mouths happened to sit
	 * on a road and the rest were between eleven and sixty-two blocks from one
	 * -- up to a thousand grids of open country to search with nothing to
	 * follow.  Since a dungeon is sited by the kind of country it belongs in,
	 * and the deep ones belong in empty country a long way from any town, that
	 * is not something better siting can fix.  A road to the door can.
	 *
	 * Routed to the nearest town rather than to the nearest road: blocks that
	 * already carry a road cost almost nothing to cross, so the spur runs to
	 * the network by the shortest way and then follows it, which is the same
	 * answer without having to search for the road first.
	 */
	for (i = 0; i < w->dungeon_count; i++) {
		struct loc mouth = w->dungeons[i].block;
		int best = -1, nearest = 0;

		for (j = 0; j < n; j++) {
			int d = MAX(ABS(w->towns[j].block.x - mouth.x),
						ABS(w->towns[j].block.y - mouth.y));

			if (best < 0 || d < nearest) {
				nearest = d;
				best = j;
			}
		}

		if (best >= 0)
			wild_lay_road(w, &scratch, w->towns[best].block, mouth);
	}

	mem_free(scratch.hcost);
	mem_free(scratch.hnode);
	mem_free(scratch.done);
	mem_free(scratch.prev);
	mem_free(scratch.dist);
}

/**
 * Adopt a window the game did not build in this process (WLD-24).
 *
 * wild_window is what lets a rebuild leave alone the axis that did not need
 * moving, and wild_scroll is what lets the display follow the axis that did.
 * Both are worked out when a window is built -- so after loading a character
 * who was standing on the surface, neither had been set: the game had restored
 * p->wild_offset without ever calling wild_surface().  The first scroll after
 * every load therefore re-anchored both axes and told the display the window
 * had not moved, which is precisely the jump the two of them exist to prevent.
 */
void wild_adopt_window(struct loc offset)
{
	wild_window = offset;
	wild_window_set = true;
	wild_scroll = loc(0, 0);
}

/**
 * How far the window moved, in grids, when the surface was last rebuilt.
 *
 * The display addresses the panel in coordinates within the chunk, and a
 * rebuild replaces the chunk under it.  Shifting the panel by this keeps the
 * same country in the same place on screen, so that scrolling the world is
 * invisible rather than a jump.
 */
struct loc wild_scroll_delta(void)
{
	return wild_scroll;
}

/**
 * Does a road run through this block?
 */
bool wild_road_at(struct wilderness *w, int bx, int by)
{
	struct wild_block *block = wild_block_at(w, bx, by);

	return block && (block->info & WILD_INFO_ROAD);
}

/**
 * Lay out the world map (WLD-07, WLD-08).
 *
 * Four independent fractals give each block a position in parameter space, and
 * its terrain follows from the first three.  Height is the roughest, since
 * coastlines and mountain ranges should be ragged; population, law and magic
 * vary more smoothly, because settlement, order and whatever magic is all
 * spread by contiguity.
 *
 * Magic takes no part in choosing terrain -- it is what WLD-15 scores buildings
 * on, so it decides what stands in a town rather than what the country looks
 * like.  A place can be steeped in it and look like ordinary farmland.
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
	int *magic = mem_zalloc(count * sizeof(int));
	int i;

	/* World generation draws from a stream fixed by the world seed alone. */
	Rand_quick = true;
	Rand_value = w->seed;

	wild_plasma(hgt, size, 24);
	wild_plasma(pop, size, 12);
	wild_plasma(law, size, 12);
	/*
	 * Magic runs on a stream of its own, and that is not a detail.
	 *
	 * Every draw here comes from one stream seeded by the world seed, so a
	 * fractal added to the middle of this sequence shifts every draw after it
	 * and the whole world comes out different -- different rivers, different
	 * towns, in different places, with different names.  Which is what happened
	 * when this line was first added between law and the rest: every existing
	 * character's world was quietly rearranged under them, and a savefile that
	 * recorded a visit to a town called Helgram was loaded into a world with no
	 * such place.  The visit was dropped on the floor, because town knowledge is
	 * stored by name.
	 *
	 * Taking magic from its own stream and putting the shared one back leaves
	 * every other draw exactly where it was, so worlds made before magic existed
	 * still come out the way they did.
	 */
	{
		uint32_t keep = Rand_value;

		Rand_value = w->seed ^ 0x9E3779B9;
		wild_plasma(magic, size, 12);
		Rand_value = keep;
	}

	for (i = 0; i < count; i++) {
		struct wild_block *block = &w->map[i];
		int bx = i % size, by = i / size;
		int rim = MIN(MIN(bx, by), MIN(size - 1 - bx, size - 1 - by));

		block->hgt = (uint8_t) hgt[i];
		block->pop = (uint8_t) pop[i];
		block->law = (uint8_t) law[i];
		block->magic = (uint8_t) magic[i];
		block->terrain = (uint8_t) wild_classify(hgt[i], pop[i], law[i]);
		block->place = 0;
		block->info = 0;

		/*
		 * The world ends in open sea (WLD-08).  Sail far enough west and you
		 * run out of land, then out of world -- which is the honest shape for
		 * a map that has to stop somewhere, and a good deal better than a wall
		 * across the countryside.  The margin is wide enough that the boundary
		 * is always reached across water and never abruptly out of a forest.
		 */
		if (rim < WILD_SEA_MARGIN) {
			block->terrain = WILD_TERRAIN_OCEAN;
			block->hgt = 0;
		} else if (rim < WILD_SEA_MARGIN + 1 &&
				   block->terrain != WILD_TERRAIN_OCEAN) {
			block->terrain = WILD_TERRAIN_SHORE;
		}
	}

	/*
	 * Water before towns, so that a town is not planted in a lake and the
	 * placement scoring can steer clear of the wet ground.
	 */
	wild_place_rivers(w);
	wild_place_lakes(w);

	/*
	 * Towns, then the dungeons that open between them, then the roads that
	 * join all of it up.  The roads go last because they are laid to reach the
	 * places -- a dungeon mouth nobody can find is a dungeon nobody uses.
	 */
	wild_place_towns(w);
	wild_name_towns(w);
	wild_place_dungeons(w);
	wild_place_roads(w);

	Rand_quick = false;

	mem_free(hgt);
	mem_free(pop);
	mem_free(law);
	mem_free(magic);
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
}

/**
 * Let go of the world.  Called when the game shuts down, and whenever a
 * different world is about to be generated.
 */
void wild_cleanup(void)
{
	wild_window_set = false;
	wild_scroll = loc(0, 0);
	wild_shown_town = -2;

	if (wild_known) {
		cave_free(wild_known);
		wild_known = NULL;
	}

	wild_town_free();
	wild_free(wild);
	wild = NULL;
}

/**
 * How many blocks across the live surface is.
 *
 * Zangband kept 9x9 blocks live around the player, and wild:cache-blocks is
 * that figure: the surface is the largest odd square that fits in it.
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
static struct chunk *wild_town[WILD_TOWNS_MAX];

void wild_town_free(void)
{
	int i;

	for (i = 0; i < WILD_TOWNS_MAX; i++)
		if (wild_town[i]) {
			cave_free(wild_town[i]);
			wild_town[i] = NULL;
		}
}

/**
 * The town's gates, in the town chunk's own coordinates.
 *
 * Two tiles per side, so at most eight.  Held here rather than on the surface
 * because the surface is rebuilt as the player walks and the town is not: the
 * gates belong to the town, and where they are does not change.
 */
#define WILD_TOWN_GATES 8

static struct {
	struct loc grid;		/**< Where in the town chunk */
	int32_t opened;			/**< The turn it was found standing open */
} wild_gates[WILD_TOWNS_MAX][WILD_TOWN_GATES];

static int wild_gate_count[WILD_TOWNS_MAX];

/**
 * Wall the town in.
 *
 * Angband's town is a starburst clearing blasted out of rock, and where the
 * clearing happens to reach the edge of its rectangle it leaves a hole.  As a
 * level of its own that never mattered.  On a surface it produces a town with
 * ragged gaps in it several tiles wide -- which looks like nothing in
 * particular, and lets anything at all walk in.
 *
 * So every way through the boundary is closed, and the gates below are cut
 * deliberately.  Shop doorways and the staircase are left alone: a shop that
 * happens to face the edge is still a shop, and bricking up the way down would
 * be a poor joke.
 */
static void wild_town_wall(struct chunk *town)
{
	int w = town->width, h = town->height;
	struct loc stairs_lost = loc(-1, -1);
	int i;

	#define TOWN_SEAL(g) \
		do { \
			struct loc _g = (g); \
			if (square_ispassable(town, _g) && !square_isperm(town, _g) && \
				!square_isshop(town, _g)) { \
				if (square_isdownstairs(town, _g)) stairs_lost = _g; \
				square_set_feat(town, _g, FEAT_GRANITE); \
			} \
		} while (0)

	for (i = 1; i < w - 1; i++) {
		TOWN_SEAL(loc(i, 1));
		TOWN_SEAL(loc(i, h - 2));
	}
	for (i = 1; i < h - 1; i++) {
		TOWN_SEAL(loc(1, i));
		TOWN_SEAL(loc(w - 2, i));
	}

	#undef TOWN_SEAL

	/*
	 * Angband puts the staircase against the north wall -- town_gen_layout()
	 * starts looking at row 1 and takes the first floor it finds -- so it can
	 * land on the boundary itself and be walled over.  Rather than make it an
	 * exception to the sealing, which would leave a third way out of town
	 * beside the gates, it is moved to the nearest floor inside.
	 */
	if (stairs_lost.x >= 0) {
		int radius;

		for (radius = 1; radius < MAX(w, h); radius++) {
			bool placed = false;
			int dx, dy;

			for (dy = -radius; dy <= radius && !placed; dy++)
				for (dx = -radius; dx <= radius && !placed; dx++) {
					struct loc grid = loc(stairs_lost.x + dx,
										  stairs_lost.y + dy);

					if (!square_in_bounds_fully(town, grid)) continue;
					if (grid.x < 2 || grid.x > w - 3) continue;
					if (grid.y < 2 || grid.y > h - 3) continue;
					if (!square_isfloor(town, grid)) continue;
					if (square_isshop(town, grid)) continue;

					square_set_feat(town, grid, FEAT_MORE);
					placed = true;
				}

			if (placed) break;
		}
	}
}

/**
 * Cut a gate two tiles wide through the town's boundary.
 *
 * Both tiles are driven inward until they meet ground that can be walked on,
 * and the pair of grids in the boundary itself become closed doors.  Gives up
 * on meeting a permanent wall -- that is a shop, and a gate through the side of
 * somebody's shop is not a gate.
 *
 * \param start is the grid in the boundary to cut from.
 * \param step is the direction inwards.
 * \param across is the direction along the boundary, for the second tile.
 * \return true if a gate now stands here.
 */
static bool wild_town_cut_gate(struct chunk *town, int idx, struct loc start,
							   struct loc step, struct loc across, int limit)
{
	struct loc lane[2];
	int reach[2];
	int i, k, depth;

	if (wild_gate_count[idx] + 2 > WILD_TOWN_GATES)
		return false;

	lane[0] = start;
	lane[1] = loc_sum(start, across);

	for (i = 0; i < 2; i++) {
		struct loc grid = lane[i];

		reach[i] = -1;

		for (k = 0; k < limit; k++) {
			if (!square_in_bounds_fully(town, grid))
				return false;
			if (square_isperm(town, grid) || square_isshop(town, grid))
				return false;
			if (square_ispassable(town, grid)) {
				reach[i] = k;
				break;
			}
			grid = loc_sum(grid, step);
		}

		if (reach[i] < 0)
			return false;
	}

	/* Drive both tiles as far as the deeper of the two needs. */
	depth = MAX(reach[0], reach[1]);

	for (i = 0; i < 2; i++) {
		struct loc grid = lane[i];

		for (k = 0; k < depth; k++) {
			/*
			 * Carve rock only.  The two tiles of a gate are driven to the
			 * deeper of the two depths, so the shallower one runs on past
			 * ground that was already open -- and paving that over destroyed
			 * whatever stood on it.  The town's down staircase is the casualty
			 * that matters: wild_town_wall() moves it inside when the north
			 * wall lands on it, which puts it exactly in the north gate's path,
			 * and about one town in fifty lost its staircase this way.
			 */
			if (!square_ispassable(town, grid))
				square_set_feat(town, grid, FEAT_ROAD);
			grid = loc_sum(grid, step);
		}
	}

	/* And hang the doors in the boundary itself. */
	for (i = 0; i < 2; i++) {
		square_set_feat(town, lane[i], FEAT_CLOSED);
		wild_gates[idx][wild_gate_count[idx]].grid = lane[i];
		wild_gates[idx][wild_gate_count[idx]].opened = 0;
		wild_gate_count[idx]++;
	}

	return true;
}

/**
 * Where a road arrives at one side of a town, in the town's own coordinates.
 *
 * Roads are drawn along the middle of the blocks that carry them, so a road
 * coming in from the north runs down the centre column of some block.  The gates
 * were cut at the middle of each side regardless, and a town is up to eight
 * blocks across -- so a road arriving along a block column two over from the
 * middle left the traveller at a blank wall with the gate twenty-six grids
 * along it.  Measured before this existed: half the towns of a world had their
 * road arrive within two grids of a gate and the rest between sixteen and
 * twenty-six, which is the block size and multiples of it.
 *
 * So the gate goes where the road comes in.  That is also the right way round:
 * a town does not put its gate somewhere and hope a road turns up.
 *
 * \param side is 0 north, 1 south, 2 west, 3 east.
 * \return the coordinate along that side, or -1 if no road arrives there.
 */
static int wild_town_road_gate(struct wilderness *w, int idx, int side)
{
	int size = z_info->wild_block_size;
	struct loc org = wild_town_origin_of(w, idx);
	int wid = w->towns[idx].wid, hgt = w->towns[idx].hgt;
	int bx0 = org.x / size, bx1 = (org.x + wid - 1) / size;
	int by0 = org.y / size, by1 = (org.y + hgt - 1) / size;
	int b;

	if (side < 2) {
		/* North or south: look for a road in the block row beyond. */
		int outside = (side == 0) ? by0 - 1 : by1 + 1;
		int inside = (side == 0) ? by0 : by1;

		for (b = bx0; b <= bx1; b++) {
			int local;

			if (!wild_road_at(w, b, outside)) continue;
			if (!wild_road_at(w, b, inside)) continue;

			/*
			 * Clamped rather than rejected.  A town's rectangle is centred on
			 * its block and so does not line up with block boundaries, which
			 * means the centre of an edge block can fall outside the town
			 * altogether.  Refusing those sent the gate back to the middle of
			 * the side, which is the worst answer; the corner nearest the road
			 * is much the better one.
			 */
			local = b * size + size / 2 - org.x;
			return MAX(1, MIN(local, wid - 3));
		}
	} else {
		int outside = (side == 2) ? bx0 - 1 : bx1 + 1;
		int inside = (side == 2) ? bx0 : bx1;

		for (b = by0; b <= by1; b++) {
			int local;

			if (!wild_road_at(w, outside, b)) continue;
			if (!wild_road_at(w, inside, b)) continue;

			local = b * size + size / 2 - org.y;
			return MAX(1, MIN(local, hgt - 3));
		}
	}

	return -1;
}

/**
 * Give the town one gate on each of its four sides.
 *
 * Tried from the middle of each side outwards, so a gate stands where a gate
 * would: at the end of a street rather than in a corner.
 */
static void wild_town_open(struct chunk *town, int idx)
{
	int w = town->width, h = town->height;
	int i, side;

	wild_gate_count[idx] = 0;
	wild_town_wall(town);

	/*
	 * Each side in turn, working outwards from where its road arrives -- or
	 * from the middle, if nothing was paved to that side.  A town wants a way
	 * out on every side whether or not anybody built a road to it.
	 *
	 * Outwards from the road rather than merely trying the road first: the
	 * cutter refuses a spot with a shop behind it, and falling back to the
	 * middle of the side then put the gate as far from the traveller as it
	 * could.  Measured: trying the road alone left one town in twelve with its
	 * gate twelve to seventeen grids from where the road stopped.
	 */
	for (side = 0; side < 4; side++) {
		int road = wild ? wild_town_road_gate(wild, idx, side) : -1;
		int span = (side < 2) ? w : h;
		int from = (road >= 0) ? road : span / 2;

		for (i = 0; i < span - 3; i++) {
			int at = from + ((i % 2) ? -((i + 1) / 2) : (i / 2));
			bool cut;

			if (at < 1 || at > span - 3) continue;

			switch (side) {
				case 0:
					cut = wild_town_cut_gate(town, idx, loc(at, 1),
											 loc(0, 1), loc(1, 0), h / 2);
					break;
				case 1:
					cut = wild_town_cut_gate(town, idx, loc(at, h - 2),
											 loc(0, -1), loc(1, 0), h / 2);
					break;
				case 2:
					cut = wild_town_cut_gate(town, idx, loc(1, at),
											 loc(1, 0), loc(0, 1), w / 2);
					break;
				default:
					cut = wild_town_cut_gate(town, idx, loc(w - 2, at),
											 loc(-1, 0), loc(0, 1), w / 2);
					break;
			}

			if (cut) break;
		}
	}
}

/**
 * Swing the town gates shut behind whoever went through them (WLD-10).
 *
 * A gate that stays open once opened is a hole with a door beside it.  This is
 * called on the world tick: a gate found standing open is noted, and closed
 * again once it has stood open long enough and nobody is in the doorway.
 *
 * Nothing hooks the act of opening.  The gate is simply looked at from time to
 * time, which costs a handful of grid tests and works whoever opened it --
 * player, monster, or a spell that blew it off its hinges.
 */
void wild_town_gates_tick(struct wilderness *w, struct chunk *c,
						  struct loc offset)
{
	int town;

	for (town = 0; town < wild_town_count(w); town++) {
		struct loc org = wild_town_origin_of(w, town);
		int i;

		/* Only a town whose gates have been built has gates to shut. */
		if (!wild_town[town]) continue;

		for (i = 0; i < wild_gate_count[town]; i++) {
			struct loc grid = loc(org.x + wild_gates[town][i].grid.x - offset.x,
								  org.y + wild_gates[town][i].grid.y - offset.y);

			if (!square_in_bounds_fully(c, grid))
				continue;

			if (!square_isopendoor(c, grid)) {
				/* Shut, or broken beyond shutting.  Not our business. */
				wild_gates[town][i].opened = 0;
				continue;
			}

			if (!wild_gates[town][i].opened) {
				wild_gates[town][i].opened = turn;
				continue;
			}

			if (turn - wild_gates[town][i].opened <
				(int32_t) z_info->wild_gate_turns)
				continue;

			/* Not onto somebody standing in the gateway. */
			if (square_monster(c, grid) || loc_eq(grid, player->grid))
				continue;

			square_close_door(c, grid);
			square_memorize(c, grid);
			square_light_spot(c, grid);
			wild_gates[town][i].opened = 0;

			if (square_isseen(c, grid))
				msg("The town gate swings shut.");
		}
	}
}

/**
 * The town's layout, built on first use.
 *
 * Seeded from the world seed and the town's position, so it belongs to this
 * world and no other, and comes out identically however many times the surface
 * is rebuilt around it.
 */
static struct chunk *wild_town_chunk(struct wilderness *w, struct player *p,
									 int idx)
{
	if (idx < 0 || idx >= w->town_count)
		return NULL;

	if (!wild_town[idx]) {
		const struct wild_town *town = &w->towns[idx];

		wild_town[idx] = town_gen_wild(p,
			wild_block_seed(w, town->block.x, town->block.y),
			town->wid, town->hgt, town->stores, town->services);
		wild_town_open(wild_town[idx], idx);
	}

	return wild_town[idx];
}

/**
 * Run the road up to the town's gates (WLD-08).
 *
 * A road is routed to a town's middle block and the gates are cut where it
 * arrives, which puts the two together most of the time -- but not always: the
 * gate cutter refuses a spot with a shop behind it, and a town's rectangle does
 * not line up with the blocks the road is drawn along.  When they miss, the road
 * stopped at a blank wall and the traveller had to walk the perimeter to find a
 * way in.  Reported from play as walking a long road to a dead end, which is
 * exactly what it was.
 *
 * So the last stretch is drawn here instead of being left to chance: from each
 * gate, out through the ring the town does not draw, and then along the outside
 * of the wall until it meets the road that came for it.  Bounded by the length
 * of that side, and drawn only if a road is actually found -- a gate on a side
 * nobody paved stays a gate onto open country.
 */
/**
 * Run the road from a town's gates out to meet the network (WLD-08).
 *
 * A road is routed between block centres, and a gate is cut where the road
 * arrives at a wall -- but not every gate has a road arriving at it.  A town has
 * four, and the network commonly reaches one or two of them.
 *
 * So nothing is paved until the approach has actually found the road.  Paving
 * first and searching afterwards is what this did, and it left a three-grid
 * stub at every gate the network did not reach: measured over six worlds, 147 of
 * 508 gates, better than one in four.  Reported from play as a road that "goes
 * out of the town but just ends", which is precisely what it was -- and worse
 * than no road at all, because a road is a promise that it goes somewhere.
 */
static void wild_draw_town_approach(struct wilderness *w, struct chunk *c,
									int idx, struct loc offset)
{
	struct loc org = wild_town_origin_of(w, idx);
	int wid = w->towns[idx].wid, hgt = w->towns[idx].hgt;
	int i;

	for (i = 0; i < wild_gate_count[idx]; i++) {
		struct loc gate = wild_gates[idx][i].grid;
		struct loc step = loc(0, 0), along;
		struct loc out[WILD_APPROACH_REACH];
		struct loc at;
		int reach, out_count = 0, dir;
		bool joined = false;

		/* Which wall it is in, and so which way is out. */
		if (gate.y <= 1) step = loc(0, -1);
		else if (gate.y >= hgt - 2) step = loc(0, 1);
		else if (gate.x <= 1) step = loc(-1, 0);
		else if (gate.x >= wid - 2) step = loc(1, 0);
		else continue;

		along = loc(step.y, step.x);

		/* Out to the edge of the town's ground, remembering the way. */
		at = loc(org.x + gate.x - offset.x, org.y + gate.y - offset.y);

		for (reach = 0; reach < WILD_APPROACH_REACH; reach++) {
			struct loc next = loc(at.x + step.x, at.y + step.y);

			if (!square_in_bounds_fully(c, next)) break;
			if (!square_ispassable(c, next)) break;

			at = next;
			out[out_count++] = at;
		}

		/* Then along the wall, each way, until the road turns up. */
		for (dir = -1; dir <= 1 && !joined; dir += 2) {
			struct loc walk = at;
			int span = (step.x ? hgt : wid);
			int k;

			for (k = 0; k < span; k++) {
				struct loc next = loc(walk.x + along.x * dir,
									  walk.y + along.y * dir);

				if (!square_in_bounds_fully(c, next)) break;
				if (!square_ispassable(c, next)) break;

				walk = next;

				if (square(c, walk)->feat == FEAT_ROAD) {
					/* Found it: pave what we walked over to get here... */
					struct loc back = at;
					int j;

					for (j = 0; j <= k; j++) {
						if (square(c, back)->feat != FEAT_ROAD)
							square_set_feat(c, back, FEAT_ROAD);
						back = loc(back.x + along.x * dir,
								   back.y + along.y * dir);
					}

					joined = true;
					break;
				}
			}
		}

		/* ...and the way out of the gate, but only now that it leads there. */
		if (joined) {
			int j;

			for (j = 0; j < out_count; j++)
				if (square(c, out[j])->feat != FEAT_ROAD)
					square_set_feat(c, out[j], FEAT_ROAD);
		}
	}
}

/**
 * Draw the town into the live surface, where the window covers it.
 *
 * The town's outermost ring is skipped.  It is a permanent wall, and it exists
 * only because Angband levels need a boundary; on a surface that runs past the
 * town in every direction it would be a wall around nothing.  What is inside it
 * -- the clearing, the streets, the shops, the ruins, and the rock the clearing
 * was cut from -- is 4.2's town, and goes down as 4.2 drew it.
 */
static void wild_draw_town(struct wilderness *w, struct player *p,
						   struct chunk *c, struct loc offset)
{
	int idx;

	for (idx = 0; idx < wild_town_count(w); idx++) {
		struct loc org = wild_town_origin_of(w, idx);
		struct chunk *town;
		struct loc grid;

		/* Only the towns this window actually covers. */
		if (org.x + w->towns[idx].wid <= offset.x ||
			org.x >= offset.x + c->width ||
			org.y + w->towns[idx].hgt <= offset.y ||
			org.y >= offset.y + c->height)
			continue;

		town = wild_town_chunk(w, p, idx);
		if (!town) continue;

		for (grid.y = 1; grid.y < town->height - 1; grid.y++)
			for (grid.x = 1; grid.x < town->width - 1; grid.x++) {
				struct loc dest = loc(org.x + grid.x - offset.x,
									  org.y + grid.y - offset.y);

				if (!square_in_bounds_fully(c, dest))
					continue;

				square_set_feat(c, dest, square_feat(town, grid)->fidx);
			}

		/* And bring the road up to its gates (WLD-08). */
		wild_draw_town_approach(w, c, idx, offset);
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
	struct chunk *town = wild_town_chunk(w, p, 0);
	struct loc org = wild_town_origin(w);
	struct loc grid;

	/*
	 * A world with no town in it has no staircase to start on.  Should not
	 * happen -- wild_place_towns() has a fallback of its own -- but the middle
	 * of the world is a better answer than a crash.
	 */
	if (!town)
		return loc(w->blocks * z_info->wild_block_size / 2,
				   w->blocks * z_info->wild_block_size / 2);

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
	struct loc org = wild_town_origin(w);
	struct loc grid;

	/*
	 * Only the village.  The player has lived there and knows every street of
	 * it; the other towns are somewhere to find, and they arrive on the map the
	 * same way the rest of the world does -- by being walked to.
	 */
	for (grid.y = 0; grid.y < w->towns[0].hgt; grid.y++)
		for (grid.x = 0; grid.x < w->towns[0].wid; grid.x++) {
			struct loc dest = loc(org.x + grid.x - offset.x,
								  org.y + grid.y - offset.y);

			if (!square_in_bounds_fully(c, dest))
				continue;

			square_memorize(c, dest);
		}
}

/**
 * Which inhabitants wild_town_people() is placing just now.
 *
 * Global because get_mon_num_prep() takes a bare function pointer, which is how
 * the wilderness monster filter works too.
 */
static int wild_folk_wanted = WILD_FOLK_VILLAGER;

/**
 * Would this monster be found living in a town of the current kind (WLD-11)?
 */
static bool wild_folk_ok(struct monster_race *race)
{
	if (!race || !race->base)
		return false;

	/* Nothing may be the only copy of itself: uniques are not street life. */
	if (rf_has(race->flags, RF_UNIQUE))
		return false;

	switch (wild_folk_wanted) {
		case WILD_FOLK_VILLAGER:
			/*
			 * People.  Angband's town list is almost all one base, so this is
			 * mostly a matter of excluding the animals that share it -- and the
			 * beggars, drunks and merchants are exactly the crowd that was
			 * being lost before WLD-24 put them back.
			 */
			return streq(race->base->name, "townsfolk") ||
				   streq(race->base->name, "person") ||
				   streq(race->base->name, "humanoid");

		case WILD_FOLK_BEAST:
			return rf_has(race->flags, RF_ANIMAL) ||
				   streq(race->base->name, "feline") ||
				   streq(race->base->name, "canine") ||
				   streq(race->base->name, "bird") ||
				   streq(race->base->name, "rodent") ||
				   streq(race->base->name, "quadruped");

		case WILD_FOLK_MONSTER:
			/* Anything that is not a person, and would not be missed. */
			return !streq(race->base->name, "townsfolk") &&
				   !streq(race->base->name, "person");

		default:
			return false;
	}
}

/**
 * Put the townspeople back on the streets (WLD-24).
 *
 * Angband's town_gen() places its residents itself, and the surface never calls
 * it: it takes the town's *terrain* and draws that in, which leaves the beggars,
 * the scruffy dogs and the pitiful looking wretches behind.  They were lost
 * without being noticed until the town was walked through and found empty.
 *
 * They cannot come across in the blit -- monsters live in a chunk's own arrays
 * and are addressed by index -- so they are placed here instead, on the surface,
 * within the town they belong to.  Which means they are re-rolled whenever the
 * window is rebuilt: a different crowd each time you come home.  That is the
 * same gap everything else on the surface has, and WLD-04 closes it for all of
 * them together.
 *
 * As in town_gen(), the count depends on the hour.
 */
void wild_town_people(struct wilderness *w, struct player *p, struct chunk *c,
					  struct loc offset)
{
	int residents = is_daytime() ?
		z_info->town_monsters_day : z_info->town_monsters_night;
	int idx;

	for (idx = 0; idx < wild_town_count(w); idx++) {
		struct loc org = loc(wild_town_origin_of(w, idx).x - offset.x,
							 wild_town_origin_of(w, idx).y - offset.y);
		int wid = w->towns[idx].wid, hgt = w->towns[idx].hgt;
		int placed = 0, tries = 0;
		int want, depth = 0;

		/* Only when the town is actually on the live surface. */
		if (org.x + wid <= 0 || org.x >= c->width ||
			org.y + hgt <= 0 || org.y >= c->height)
			continue;

		/*
		 * A town nobody lives in has nobody in its streets (WLD-11).
		 *
		 * Not what empties it -- wild_folk_ok() accepts nothing for an
		 * abandoned town, so the loop below would place nobody in any case.
		 * This saves it several hundred failed attempts at doing so.
		 */
		if (w->towns[idx].folk == WILD_FOLK_ABANDONED)
			continue;

		/* A city has more people in its streets than a village does. */
		want = residents + residents * w->towns[idx].band / 2;

		/*
		 * A town held by monsters is not held by many of them, and they are
		 * dangerous rather than numerous.  Drawn from the depth the country
		 * around the town would give, so a monster town in mild country is a
		 * milder one.
		 */
		if (w->towns[idx].folk == WILD_FOLK_MONSTER) {
			want = 1 + want / 3;

			/*
			 * wild_block_danger(), not wild_danger(): the latter returns zero
			 * for any block inside a town, and a town's own block is always
			 * inside one, so this used to clamp to depth 1 every time and fill
			 * a town held by monsters -- in lawless country, no less -- with
			 * the weakest creatures in the game.
			 */
			depth = MAX(1, wild_block_danger(
				wild_block_at(w, w->towns[idx].block.x,
							  w->towns[idx].block.y)));
		}

		wild_folk_wanted = w->towns[idx].folk;
		get_mon_num_prep(wild_folk_ok);

		while (placed < want && tries < want * 50) {
			struct loc grid = loc(org.x + randint0(wid),
								  org.y + randint0(hgt));

			tries++;

			if (!square_in_bounds_fully(c, grid)) continue;
			if (!square_isempty(c, grid)) continue;

			/* Not on the doorstep: leave the player room to arrive. */
			if (distance(grid, p->grid) < 3) continue;

			if (pick_and_place_monster(c, grid, depth, true, true, ORIGIN_DROP))
				placed++;
		}

		/*
		 * Put the allocation table back before the next town, and before
		 * anything else asks it for a monster.
		 */
		get_mon_num_prep(NULL);
	}
}

/**
 * ------------------------------------------------------------------------
 * The magetower's network (WLD-16c)
 * ------------------------------------------------------------------------ */

/**
 * Gather the places the magetower will carry the player to.
 *
 * Towns they have stood in, and dungeon mouths they have seen -- the two
 * different bars are deliberate.  A town is somewhere you have been; a mouth is
 * a staircase in a field, with nothing to be inside of, so seeing it is enough.
 *
 * The town the player is standing in is left out: there is nothing to buy.
 *
 * \param w is the world.
 * \param from is the player's world position.
 * \param dest receives the destinations, in world grids.
 * \param max is how many it has room for.
 * \return how many were found.
 */
int wild_travel_places(struct wilderness *w, struct loc from,
					   struct wild_place *dest, int max)
{
	int here = wild_town_here(w, from);
	int found = 0;
	int i;

	if (!w) return 0;

	for (i = 0; i < w->town_count && found < max; i++) {
		struct loc org = wild_town_origin_of(w, i);

		if (!w->towns[i].visited) continue;
		if (i == here) continue;

		dest[found].grid = loc(org.x + w->towns[i].wid / 2,
							   org.y + w->towns[i].hgt / 2);
		dest[found].name = w->towns[i].name;
		dest[found].what = wild_band_name(w->towns[i].band);
		dest[found].cost = wild_travel_cost(w, from, dest[found].grid);
		found++;
	}

	for (i = 0; i < w->dungeon_count && found < max; i++) {
		struct wild_dungeon *mouth = wild_dungeon_by_index(w, i);
		struct dun_type *type = dun_type_by_index(mouth->type);

		if (!wild_dungeon_found(w, i)) continue;

		dest[found].grid = mouth->grid;
		dest[found].name = type ? type->name : "a dungeon";
		dest[found].what = "dungeon";
		dest[found].cost = wild_travel_cost(w, from, dest[found].grid);
		found++;
	}

	return found;
}

/**
 * What the mages charge to carry somebody this far (WLD-16c).
 *
 * By the block, not the grid, so the number is legible and a step across a
 * town does not cost anything worth counting.
 */
int32_t wild_travel_cost(struct wilderness *w, struct loc from, struct loc to)
{
	int size = z_info->wild_block_size;
	int blocks = MAX(ABS(to.x - from.x), ABS(to.y - from.y)) / size;

	(void) w;

	return (int32_t) MAX(1, blocks) * (int32_t) z_info->wild_travel_cost;
}

/**
 * ------------------------------------------------------------------------
 * What the player knows of the surface (WLD-25)
 * ------------------------------------------------------------------------ */

/**
 * Hold on to what the player knows of the surface while they are off it.
 *
 * The surface is not a level: it is rebuilt from the world seed whenever it is
 * returned to, and the parallel chunk holding what the player has seen is
 * rebuilt empty with it.  Within a single rebuild -- scrolling the window --
 * generate_level() carries that knowledge across in a local.  A trip to the
 * dungeon is two rebuilds with a level in between, so the local could not
 * survive it, and a character who walked into town, went down the stairs and
 * came back up found the town unexplored.
 *
 * So the knowledge is kept here instead, where it outlives the call.  Takes
 * ownership of the chunk.
 */
void wild_keep_knowledge(struct chunk *known, struct loc offset)
{
	if (wild_known && wild_known != known)
		cave_free(wild_known);

	wild_known = known;
	wild_known_offset = offset;
}

/**
 * Look at what is being held, without taking it.
 *
 * For the savefile: a character who saves while down in the dungeon has their
 * knowledge of the surface here and nowhere else, so this is the only place it
 * can be written from.  Ownership stays with wild.c.
 */
struct chunk *wild_held_knowledge(struct loc *offset)
{
	if (wild_known && offset)
		*offset = wild_known_offset;

	return wild_known;
}

/**
 * Take back what was kept, or NULL if nothing was.  Ownership passes to the
 * caller, which must free it.
 */
struct chunk *wild_take_knowledge(struct loc *offset)
{
	struct chunk *known = wild_known;

	if (known && offset)
		*offset = wild_known_offset;

	wild_known = NULL;

	return known;
}

/**
 * ------------------------------------------------------------------------
 * What the player leaves behind (WLD-04)
 * ------------------------------------------------------------------------ */

/**
 * How long a thing lies where it was dropped, before the odds are even that
 * somebody has walked off with it.
 *
 * Scaled by how many people the land supports, which is the same number terrain
 * and monster density come from: a sword left outside a city gate is gone by
 * morning, and one dropped in a waste may lie there a long while.
 */
static int32_t wild_relic_half_life(struct wilderness *w, struct loc grid)
{
	int size = z_info->wild_block_size;
	struct wild_block *block = wild_block_at(w, grid.x / size, grid.y / size);
	int32_t base = (int32_t) z_info->relic_half_life * 10L * z_info->day_length;
	int pop = block ? block->pop : 0;

	/* Empty country keeps its full patience; a city has almost none. */
	return MAX(1, base / (1 + pop / 24));
}

/**
 * Is a thing left at this spot still there, after this long?
 *
 * A half-life rather than a deadline: each half-life that passes halves the
 * odds, so most things go early and the occasional one survives a remarkably
 * long time, which is how lost property actually behaves.
 */
static bool wild_relic_survives(struct wilderness *w, struct loc grid,
								int32_t elapsed)
{
	int32_t half = wild_relic_half_life(w, grid);
	int chance = 1000;

	if (elapsed <= 0)
		return true;

	while (elapsed >= half && chance > 0) {
		chance /= 2;
		elapsed -= half;
	}

	/* Taper across the part-completed half-life rather than stepping. */
	chance -= (int) ((int64_t) (chance / 2) * elapsed / half);

	return randint0(1000) < chance;
}

/**
 * Take what is lying on the surface into the world's memory (WLD-04).
 *
 * Called before the surface is torn down, which happens whenever the window
 * scrolls or the player goes below.  Objects are copied out rather than moved:
 * a chunk's objects are addressed by index within it and carry a companion in
 * the player's knowledge chunk, and both of those are about to be freed.
 */
void wild_harvest(struct wilderness *w, struct player *p, struct chunk *c,
				  struct loc offset)
{
	struct loc grid;
	int i;

	if (!w || !c)
		return;

	for (grid.y = 0; grid.y < c->height; grid.y++) {
		struct object *obj;

		for (grid.x = 0; grid.x < c->width; grid.x++) {
			obj = square_object(c, grid);
			while (obj) {
				struct object *next = obj->next;
				struct wild_relic *relic;
				struct object *keep = object_new();

				object_copy(keep, obj);

				/*
				 * The copy belongs to nothing: its index was the old chunk's,
				 * and what the player knew of it lived in a chunk that is
				 * about to go with it.
				 */
				keep->oidx = 0;
				keep->known = NULL;
				keep->grid = loc(0, 0);
				keep->held_m_idx = 0;
				keep->mimicking_m_idx = 0;

				relic = mem_zalloc(sizeof *relic);
				relic->grid = loc(offset.x + grid.x, offset.y + grid.y);
				relic->turn = turn;
				relic->obj = keep;
				relic->next = w->relics;
				w->relics = relic;

				/*
				 * Take the original off the surface as well as copying it.
				 * In ordinary use the surface is freed a moment later and this
				 * makes no difference; leaving it would mean the thing existed
				 * twice for that moment, which is the sort of detail that
				 * turns into a duplication bug the first time something else
				 * runs in between.
				 */
				square_delete_object(c, grid, obj, false, false);

				obj = next;
			}
		}
	}

	/*
	 * And the uniques (WLD-04b).  Recorded rather than copied: a monster is a
	 * good deal more entangled than an object -- an index in the chunk, a slot
	 * in a group, a pile of held objects -- and none of that needs preserving.
	 * What has to survive is which one it was, where it was, and how badly the
	 * player hurt it.  Everything else can be built again.
	 *
	 * The chunk's own teardown decrements each race's cur_num as it goes, so
	 * the unique is properly un-placed and can be placed again on return.
	 */
	for (i = 1; i < cave_monster_max(c); i++) {
		struct monster *mon = cave_monster(c, i);
		struct wild_unique *seen;

		if (!mon->race) continue;
		if (!rf_has(mon->race->flags, RF_UNIQUE)) continue;

		/*
		 * Only ones the player actually hurt.  An untouched unique is
		 * indistinguishable from a freshly rolled one, so remembering it buys
		 * nothing -- and it costs a good deal, because a remembered unique is
		 * put back near where it was left, and where it was left is next to
		 * the player if it happened to be following them.  Farmer Maggot
		 * therefore reappeared at the player's elbow every time the window
		 * scrolled, which is not what WLD-04b was for.
		 */
		if (mon->hp >= mon->maxhp) continue;

		seen = mem_zalloc(sizeof *seen);
		seen->race = mon->race;
		seen->grid = loc(offset.x + mon->grid.x, offset.y + mon->grid.y);
		seen->hp = mon->hp;
		seen->turn = turn;
		seen->next = w->uniques;
		w->uniques = seen;
	}
}

static void wild_restore_uniques(struct wilderness *w, struct player *p,
								 struct chunk *c, struct loc offset);

/**
 * Give up an object the world could not hand back.
 *
 * An artifact has to be released as well as freed, or it is gone from the game
 * for good: aup_info[].created stays set, and nothing will ever roll it again.
 * The dungeon has a rule for this already -- an artifact the player never
 * identified goes back in the pool, one they knew they had is spent -- and a
 * relic that rotted in a field is the same case as a level left behind, so it
 * is judged the same way.
 *
 * wild_harvest() takes the surface's floor objects into the world's memory
 * *before* prepare_next_level() sweeps the level for artifacts, which is what
 * makes this necessary: nothing left outdoors is ever seen by that sweep.
 */
static void wild_relic_lost(struct player *p, struct object *obj)
{
	if (obj->artifact) {
		bool found = obj_is_known_artifact(obj);

		if ((p && OPT(p, birth_lose_arts)) || found) {
			if (p) history_lose_artifact(p, obj->artifact);
			mark_artifact_created(obj->artifact, true);
		} else {
			mark_artifact_created(obj->artifact, false);
		}
	}

	object_free(obj);
}

/**
 * Put back what is still there, and forget what is not (WLD-04a).
 *
 * Called after the surface is built.  Anything outside the window stays in the
 * world's memory untouched -- it is only judged when the player is in a
 * position to look at it, which keeps the reckoning honest and costs nothing
 * while they are elsewhere.
 */
void wild_restore(struct wilderness *w, struct player *p, struct chunk *c,
				  struct loc offset)
{
	struct wild_relic **link;

	if (!w || !c)
		return;

	link = &w->relics;
	while (*link) {
		struct wild_relic *relic = *link;
		struct loc grid = loc(relic->grid.x - offset.x,
							  relic->grid.y - offset.y);
		bool dummy = true;

		/* Not on the live surface: leave it in the world's memory. */
		if (!square_in_bounds_fully(c, grid)) {
			link = &relic->next;
			continue;
		}

		*link = relic->next;

		/*
		 * floor_carry() lists the object itself, and on the path where it
		 * merges the drop into a stack already on the floor it *frees* it.
		 * Nothing may touch relic->obj after it returns true.
		 */
		if (!wild_relic_survives(w, relic->grid, turn - relic->turn) ||
			!square_isobjectholding(c, grid) ||
			!floor_carry(c, grid, relic->obj, &dummy)) {
			wild_relic_lost(p, relic->obj);
		}

		mem_free(relic);
	}

	wild_restore_uniques(w, p, c, offset);
}

/**
 * Put back the uniques that are still out there (WLD-04b).
 *
 * They have had time to recover, and time to move: a monster left alone for a
 * night is neither where it was nor as hurt as it was.  It is also awake, and
 * no longer afraid of anything that happened before, which makes a unique the
 * player ran away from meaner on the second meeting than it was on the first.
 */
static void wild_restore_uniques(struct wilderness *w, struct player *p,
								 struct chunk *c, struct loc offset)
{
	struct wild_unique **link = &w->uniques;

	while (*link) {
		struct wild_unique *seen = *link;
		struct loc grid = loc(seen->grid.x - offset.x, seen->grid.y - offset.y);
		struct monster_group_info info = { 0, 0 };
		struct loc place;
		int32_t elapsed = turn - seen->turn;
		struct monster *mon;

		/* Not on the live surface: leave it where the world remembers it. */
		if (!square_in_bounds_fully(c, grid)) {
			link = &seen->next;
			continue;
		}

		*link = seen->next;

		/* It has wandered a little way from where it was left. */
		scatter(c, &place, grid, WILD_UNIQUE_WANDER, false);
		if (!square_isempty(c, place) || square_isdamaging(c, place))
			place = grid;

		/*
		 * Placement can fail, and one way it fails is worth naming: while the
		 * player was below, this unique was free to be generated in the
		 * dungeon, because nothing here was holding its cur_num.  If that has
		 * happened it is no longer in the wilderness, and the world forgets it
		 * rather than putting a second one out.
		 */
		if (square_isempty(c, place) && !square_isdamaging(c, place) &&
			place_new_monster(c, place, seen->race, false, false, info,
							  ORIGIN_DROP)) {
			mon = square_monster(c, place);
			if (mon) {
				int regen = mon->maxhp / 100;
				int healed;

				if (regen < 1) regen = 1;
				if (rf_has(mon->race->flags, RF_REGENERATE)) regen *= 2;

				/*
				 * Clamped before it is stored, not after.  mon->hp is 16 bits
				 * and the product is not: a wounded Morgoth left for a long
				 * dungeon trip otherwise wraps to a negative and comes back
				 * dying instead of healed.
				 */
				healed = seen->hp +
					regen * (int) MIN(elapsed / 100, (int32_t) 10000);
				mon->hp = (int16_t) MIN(healed, (int) mon->maxhp);
			}
		}

		mem_free(seen);
	}
}

int wild_relic_count(const struct wilderness *w)
{
	struct wild_relic *relic;
	int count = 0;

	for (relic = w ? w->relics : NULL; relic; relic = relic->next)
		count++;

	return count;
}

int wild_unique_count(const struct wilderness *w)
{
	struct wild_unique *seen;
	int count = 0;

	for (seen = w ? w->uniques : NULL; seen; seen = seen->next)
		count++;

	return count;
}

/**
 * ------------------------------------------------------------------------
 * What lives out there (CNT-05)
 * ------------------------------------------------------------------------ */

/**
 * How dangerous a block is, as an equivalent dungeon depth.
 *
 * Zangband's own figure, from set_mon_gen() in
 * [wild1.c:3418](../../archive/zangband/src/wild1.c#L3418):
 *
 *     mon_gen = (256 - law) / 4;  mon_gen = MAX(1, mon_gen - 5);
 *
 * Law is the only input, which is the point: the wilderness is dangerous where
 * nothing polices it.  Since law is laid down by a fractal it varies smoothly,
 * so danger changes by degrees as you travel rather than by block, and a town
 * placed in orderly country sits in a wide patch of orderly country.  That is
 * what keeps the doorstep survivable -- not a safe radius bolted on afterwards.
 *
 * Split in two, because the town exemption is not always what the caller wants.
 * wild_danger() answers "how dangerous is it to stand here", which in a town is
 * not at all; wild_block_danger() answers "what danger does this country carry",
 * which a town standing on it does not change.  Anything asking about a town's
 * own block has to use the second, or it gets the exemption back as its answer.
 */
int wild_block_danger(const struct wild_block *block)
{
	int danger;

	if (!block)
		return 0;

	danger = (256 - block->law) / 4;

	return MAX(1, danger - 5);
}

int wild_danger(struct wilderness *w, int x, int y)
{
	/* A town is a town: nothing hunts in the market square. */
	if (wild_in_town(w, x, y))
		return 0;

	return wild_block_danger(wild_block_at(w, x, y));
}

/**
 * How much a block holds, as the divisor on the rarity roll.
 *
 * Zangband's `mon_prob`, again from
 * [wild1.c:3422](../../archive/zangband/src/wild1.c#L3422): `pop / 16`.
 *
 * Population here is how much life the land supports rather than how many
 * people live on it, so this says what it looks like it says -- lush country
 * teems and barren country does not.
 */
int wild_density(struct wilderness *w, int x, int y)
{
	struct wild_block *block = wild_block_at(w, x, y);

	if (!block || wild_in_town(w, x, y))
		return 0;

	return block->pop / 16;
}

/**
 * Keep the townspeople in the town.
 *
 * Angband's depth-zero monsters are the town's own: beggars, drunks, urchins,
 * scruffy dogs, and Farmer Maggot.  get_mon_num() will happily place a monster
 * shallower than the level it is asked for, so without this the open country
 * fills up with people who have no business being ten miles from anywhere.
 */
static bool wild_monster_ok(struct monster_race *race)
{
	return race->level > 0;
}

/**
 * Put monsters on the wilderness surface (CNT-05).
 *
 * Rolled per grid rather than as a headcount, which is Zangband's method and
 * has the property that matters: where the monsters are follows from what the
 * land is, so a walk from farmland into the hills gets steadily worse without
 * anything having to decide that it should.
 *
 * Half of them are asleep, as in
 * [wild3.c:1450](../../archive/zangband/src/wild3.c#L1450).
 *
 * They are re-rolled whenever the window is rebuilt, so the country behind you
 * is not the country you walked through.  That is the same gap everything else
 * on the surface has, and WLD-04 closes it for all of them together.
 */
void wild_populate(struct wilderness *w, struct player *p, struct chunk *c,
				   struct loc offset)
{
	int size = z_info->wild_block_size;
	uint32_t rarity = is_daytime() ?
		z_info->wild_mon_rarity_day : z_info->wild_mon_rarity_night;
	struct monster_group_info info = { 0, 0 };
	struct loc grid;

	get_mon_num_prep(wild_monster_ok);

	for (grid.y = 0; grid.y < c->height; grid.y++)
		for (grid.x = 0; grid.x < c->width; grid.x++) {
			int bx = (offset.x + grid.x) / size;
			int by = (offset.y + grid.y) / size;
			int chance = rarity / (wild_density(w, bx, by) + 1);
			struct monster_race *race;
			int depth;

			if (chance <= 0 || randint0(chance) != 0)
				continue;

			/* Not on the player's doorstep, and not in the sea or the fire. */
			if (distance(grid, p->grid) < 8) continue;
			if (!square_isempty(c, grid)) continue;
			if (square_isdamaging(c, grid)) continue;

			depth = wild_danger(w, bx, by);
			if (!depth) continue;

			race = get_mon_num(depth, depth);
			if (!race) continue;

			place_new_monster(c, grid, race, one_in_(2), true, info,
							  ORIGIN_DROP);
		}

	/*
	 * Put the allocation table back.  It is global state, and the very next
	 * thing to run is wild_town_people(), which wants precisely the monsters
	 * this filter excludes.
	 */
	get_mon_num_prep(NULL);
}

/**
 * Carry what the player knows of the world across a scroll of the window.
 *
 * The window is rebuilt from scratch whenever it moves, and the parallel chunk
 * holding the player's knowledge is rebuilt with it.  Without this the map is
 * wiped roughly every forty steps: everything explored goes unknown again, and
 * since the surface deliberately does not memorise itself under daylight
 * (WLD-25) there is nothing to put it back.
 *
 * Only the terrain is carried.  Known objects and traps live in the knowledge
 * chunk's own arrays and are addressed by index within it, and they are not
 * worth the entanglement -- what the player wants back is the shape of the
 * country they walked through.
 *
 * \param from is the old knowledge chunk, \param from_offset its window's
 * origin; \param to and \param to_offset the same for the new one.
 */
void wild_carry_knowledge(struct chunk *from, struct loc from_offset,
						  struct chunk *to, struct loc to_offset)
{
	int dx = from_offset.x - to_offset.x;
	int dy = from_offset.y - to_offset.y;
	struct loc grid;

	if (!from || !to)
		return;

	for (grid.y = 0; grid.y < to->height; grid.y++)
		for (grid.x = 0; grid.x < to->width; grid.x++) {
			struct loc was = loc(grid.x - dx, grid.y - dy);

			if (was.x < 0 || was.x >= from->width) continue;
			if (was.y < 0 || was.y >= from->height) continue;

			to->squares[grid.y][grid.x].feat = from->squares[was.y][was.x].feat;
		}
}

/**
 * Set the player down where they already were.
 *
 * Scrolling the window is not arriving anywhere.  The player is standing where
 * they were standing a moment ago, the terrain under them is generated from the
 * world seed and so is identical, and the only thing that has changed is which
 * part of the world the live surface covers.
 *
 * This exists because sanitize_player_loc() is the wrong tool for that job and
 * was being used for it.  It asks square_isarrivable(), which wants FLOOR or
 * stairs -- and trees and water are passable but are not floor.  So a player
 * who happened to be standing among trees when the window scrolled was treated
 * as having arrived somewhere illegal and flung to a random grid anywhere in
 * the window: the map jumped, a strip of country they had never seen appeared,
 * and the town was suddenly nowhere to be found.  In a world this wooded that
 * is most of the time.
 *
 * The only case that genuinely needs handling is a grid that cannot be stood on
 * at all, and the answer to that is the nearest one that can -- not a random
 * one a hundred grids away.
 */
void wild_settle_player(struct chunk *c, struct player *p)
{
	int radius;

	if (square_in_bounds_fully(c, p->grid) && square_ispassable(c, p->grid))
		return;

	for (radius = 1; radius < MAX(c->width, c->height); radius++) {
		int dx, dy;

		for (dy = -radius; dy <= radius; dy++)
			for (dx = -radius; dx <= radius; dx++) {
				struct loc grid = loc(p->grid.x + dx, p->grid.y + dy);

				/* Only the ring at this radius; the inside has been tried. */
				if (ABS(dx) != radius && ABS(dy) != radius) continue;
				if (!square_in_bounds_fully(c, grid)) continue;
				if (!square_isempty(c, grid)) continue;
				if (square_isdamaging(c, grid)) continue;

				p->grid = grid;
				return;
			}
	}
}

/**
 * ------------------------------------------------------------------------
 * What the player has seen of the world (WLD-25)
 * ------------------------------------------------------------------------ */

/**
 * Put the nearest place the player has not found onto the world map (PLR-41).
 *
 * What a true dream at the inn shows you.  A place counts as found when its block
 * has been seen, the same test the world map and the dungeon list already use, so
 * this marks the block and the place appears where it stands.
 *
 * It marks the block *seen* and not the town *visited*, and the difference is the
 * whole honesty of the thing: seen is "you know it is there", visited is "you have
 * stood in it", and the magetower carries you only to the latter.  A dream tells
 * you where to walk.  It does not carry you.
 *
 * The nearest, rather than a random one, because a vision of somewhere on the far
 * side of the world is a curiosity and one of somewhere three days' walk away is a
 * plan.  Repeated nights therefore open the map outwards from where the player is,
 * which is the order they would have explored it in anyway.
 *
 * \param from is a world grid -- where the dreamer is sleeping.
 * \param down is set true if the place is a dungeon mouth rather than a town.
 * \return the place's name, or NULL if there is nothing left to find.
 */
const char *wild_reveal_nearest(struct wilderness *w, struct loc from, bool *down)
{
	int size = z_info->wild_block_size;
	struct loc here = loc(from.x / size, from.y / size);
	struct wild_block *block;
	const char *name = NULL;
	struct loc best = loc(0, 0);
	int best_dist = -1;
	bool best_down = false;
	int i;

	if (!w) return NULL;

	for (i = 0; i < w->town_count; i++) {
		struct wild_town *town = &w->towns[i];
		int dist;

		if (wild_seen(w, town->block.x, town->block.y)) continue;

		/* Somewhere else.  A dream about the room you are asleep in is not one. */
		if (loc_eq(town->block, here)) continue;

		dist = distance(here, town->block);
		if (best_dist < 0 || dist < best_dist) {
			best_dist = dist;
			best = town->block;
			best_down = false;
			name = town->name ? town->name : wild_band_name(town->band);
		}
	}

	for (i = 0; i < w->dungeon_count; i++) {
		struct wild_dungeon *mouth = &w->dungeons[i];
		struct dun_type *type = dun_type_by_index(mouth->type);
		int dist;

		if (wild_seen(w, mouth->block.x, mouth->block.y)) continue;
		if (loc_eq(mouth->block, here)) continue;

		dist = distance(here, mouth->block);
		if (best_dist < 0 || dist < best_dist) {
			best_dist = dist;
			best = mouth->block;
			best_down = true;
			name = (type && type->name) ? type->name : "a way down";
		}
	}

	if (!name) return NULL;

	block = wild_block_at(w, best.x, best.y);
	if (!block) return NULL;

	block->info |= WILD_INFO_SEEN;

	if (down) *down = best_down;

	return name;
}

/**
 * Forget the world map, except where home is (ZangbandTK, PLR-40).
 *
 * What the lotus takes.  Every block the player has seen goes back to unseen, so
 * the world map is blank again, every town is unvisited, and since a dungeon
 * mouth counts as found when its block has been seen, the ways down are lost with
 * everything else.
 *
 * Home is the exception, and not out of kindness: WLD-12 makes the starting
 * village always known, the magetower's list is built from what the player has
 * found, and a character who has forgotten every place including the one they
 * started in has no way to travel anywhere and nothing to walk towards.  The
 * blocks around it stay seen too, or the village would be a name on an otherwise
 * empty map with no ground around it.
 *
 * Corwin begins the first novel with no memory and one certainty -- that there is
 * a place called Amber and he is of it.  That is the shape of this.
 */
void wild_forget_knowledge(struct wilderness *w)
{
	int i, x, y;
	struct loc home;

	if (!w || !w->town_count) return;

	for (i = 0; i < w->blocks * w->blocks; i++)
		w->map[i].info &= ~WILD_INFO_SEEN;

	for (i = 0; i < w->town_count; i++)
		w->towns[i].visited = 0;

	/* And put home back. */
	home = w->towns[0].block;
	w->towns[0].visited = 1;

	for (y = home.y - 1; y <= home.y + 1; y++)
		for (x = home.x - 1; x <= home.x + 1; x++) {
			struct wild_block *block = wild_block_at(w, x, y);

			if (block) block->info |= WILD_INFO_SEEN;
		}
}

/**
 * Note that the player has seen the country around a world position.
 *
 * Knowledge for the overhead map is kept per *block*, not per grid, because the
 * map draws one character per block and that is all it can show.  A block is
 * 16 grids and sight reaches 20, so standing anywhere in a block you can see
 * into each of its neighbours: the whole three-by-three is marked.
 *
 * This is much the cheaper half of remembering the world.  The map wants 129 by
 * 129 bits -- two kilobytes, saved outright.  Remembering the *detail* of
 * country walked through is a separate and far larger problem, and is still
 * open: walk out of the live window and back, and the grids come back unknown.
 */
void wild_mark_seen(struct wilderness *w, struct loc grid)
{
	int size = z_info->wild_block_size;
	int bx = grid.x / size, by = grid.y / size;
	int i, j;

	if (!w) return;

	for (j = -1; j <= 1; j++)
		for (i = -1; i <= 1; i++) {
			struct wild_block *block = wild_block_at(w, bx + i, by + j);

			if (block) block->info |= WILD_INFO_SEEN;
		}
}

bool wild_seen(struct wilderness *w, int x, int y)
{
	struct wild_block *block = wild_block_at(w, x, y);

	return block && (block->info & WILD_INFO_SEEN);
}

/**
 * Is any town on this block?
 */
bool wild_in_town(struct wilderness *w, int bx, int by)
{
	return wild_town_at(w, bx, by) >= 0;
}

/**
 * The terrain feature that stands for a whole block on the overhead map.
 *
 * Taken from the same table the ground itself is drawn from, so the map agrees
 * with the country: what the map calls forest is what you walk into.  A town,
 * a road and water each outrank the terrain under them, in that order, since
 * those are the things worth seeing on a map.
 */
int wild_block_feat(struct wilderness *w, int x, int y)
{
	struct wild_block *block = wild_block_at(w, x, y);

	if (!block)
		return FEAT_NONE;

	if (wild_in_town(w, x, y))
		return FEAT_PERM;
	if (wild_dungeon_in_block(w, x, y))
		return FEAT_DUNGEON;
	if (block->info & WILD_INFO_ROAD)
		return FEAT_ROAD;
	if (block->info & WILD_INFO_WATER)
		return FEAT_WATER;

	/* Roll zero is each terrain's dominant feature. */
	return wild_terrain_feat(block->terrain, 0);
}

/**
 * Is this chunk the wilderness surface rather than a level?
 */
bool wild_is_surface(const struct chunk *c)
{
	return c && c->name && streq(c->name, "wilderness");
}

/**
 * How close the player may come to the window's border before it is rebuilt.
 *
 * Further than they can see, and by a margin.  The border carries the
 * impassable ring every Angband level needs, and away from the edge of the
 * world that ring is a fiction -- there is more country beyond it.  The player
 * must never be in a position to look at it, so the trigger has to exceed
 * max-sight rather than merely being "near the edge".
 */
static int wild_recentre_margin(void)
{
	return MAX(2 * z_info->wild_block_size, z_info->max_sight + 8);
}

/**
 * Is the player far enough from both edges of this axis to leave it alone?
 *
 * Mirrors wild_needs_recentre(), which is what decides a rebuild is due: an
 * axis that would not have called for one on its own does not get moved by one
 * the other axis called for.
 */
static bool wild_axis_settled(int centre, int origin, int span, int margin,
							  int world_max)
{
	int local = centre - origin;

	/* It has to be inside the old window before anything else. */
	if (local < 0 || local >= span)
		return false;

	if (local < margin && origin > 0)
		return false;
	if (local >= span - margin && origin + span < world_max)
		return false;

	return true;
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

	/*
	 * Then leave alone whichever axis did not need moving.  A rebuild is
	 * triggered by either axis nearing the window's edge and used to re-anchor
	 * both, so a long walk due west re-centred the window vertically too --
	 * and since the window aligns to whole blocks, the character's row within
	 * it jumped by up to a block without their having moved north or south.
	 * From inside, the character appeared to drop a dozen tiles down the
	 * screen mid-stride.
	 */
	if (wild_window_set) {
		int margin = wild_recentre_margin();

		if (wild_axis_settled(centre.x, wild_window.x, span, margin, world_max))
			ox = wild_window.x;
		if (wild_axis_settled(centre.y, wild_window.y, span, margin, world_max))
			oy = wild_window.y;
	}

	/*
	 * How far the window moved, so the display can follow it.  The panel is
	 * addressed in coordinates within the chunk, and the chunk has just been
	 * rebuilt underneath it: without this the view jumps by however far the
	 * window travelled.
	 */
	wild_scroll = wild_window_set ? loc(wild_window.x - ox, wild_window.y - oy)
								  : loc(0, 0);

	wild_window = loc(ox, oy);
	wild_window_set = true;

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
				/* Beyond the world's edge: open sea. */
				square_set_feat(c, grid, FEAT_WORLD_EDGE);
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

			/*
			 * Rivers and lakes are laid over the land they run through rather
			 * than replacing its terrain kind, so a river through forest has
			 * trees on its banks (WLD-08).  The sea needs none of this: it is
			 * already water by classification.
			 */
			if (block->terrain != WILD_TERRAIN_OCEAN) {
				int wet = wild_water_at(w, wx, wy);

				if (wet > WILD_WATER_DEEP)
					feat = FEAT_DEEP_WATER;
				else if (wet >= WILD_WATER_CUT)
					feat = FEAT_WATER;
			}

			square_set_feat(c, grid, feat);
		}
	}

	/*
	 * Roads run from the middle of a block to the middle of each neighbouring
	 * block that carries one, so a routed road comes out joined up wherever it
	 * turns.  Drawing every road block as an east-west line, as the stub did,
	 * would leave a road that bends north into a row of disconnected dashes.
	 *
	 * Three grids wide, not one.  A road one grid wide is a road you can walk
	 * straight past: reported from play as a road that "ends at the beach",
	 * where in fact it turned ninety degrees south in the block the player was
	 * standing in and carried on to a dungeon.  A one-grid corner is a single
	 * square of floor at right angles to the way you are going, and there is no
	 * reason for the player to look at it.  Three grids makes a road something
	 * you can see the shape of from a distance, and makes a turn read as a turn.
	 */
	for (int by = 0; by < view; by++) {
		for (int bx = 0; bx < view; bx++) {
			int wx = ox / size + bx, wy = oy / size + by;
			int cx = bx * size + size / 2, cy = by * size + size / 2;
			int lane;

			if (!wild_road_at(w, wx, wy))
				continue;

			#define ROAD_PAVE(_gx, _gy) \
				do { \
					struct loc _g = loc((_gx), (_gy)); \
					if (square_in_bounds_fully(c, _g)) \
						square_set_feat(c, _g, FEAT_ROAD); \
				} while (0)

			for (lane = -WILD_ROAD_HALF; lane <= WILD_ROAD_HALF; lane++) {
				/* The junction at the middle, so every turn is squared off. */
				int across;

				for (across = -WILD_ROAD_HALF; across <= WILD_ROAD_HALF;
					 across++)
					ROAD_PAVE(cx + lane, cy + across);

				if (wild_road_at(w, wx - 1, wy))
					for (grid.x = bx * size; grid.x <= cx; grid.x++)
						ROAD_PAVE(grid.x, cy + lane);

				if (wild_road_at(w, wx + 1, wy))
					for (grid.x = cx; grid.x < (bx + 1) * size; grid.x++)
						ROAD_PAVE(grid.x, cy + lane);

				if (wild_road_at(w, wx, wy - 1))
					for (grid.y = by * size; grid.y <= cy; grid.y++)
						ROAD_PAVE(cx + lane, grid.y);

				if (wild_road_at(w, wx, wy + 1))
					for (grid.y = cy; grid.y < (by + 1) * size; grid.y++)
						ROAD_PAVE(cx + lane, grid.y);
			}

			#undef ROAD_PAVE
		}
	}

	/*
	 * Draw the towns in.  They go down after the terrain and the roads, so the
	 * countryside runs up to their edges rather than through them.
	 *
	 * Unconditionally: wild_draw_town() tests each town against the window
	 * itself.  This call used to be wrapped in a test of its own, against the
	 * *starting village's* rectangle -- so once the player walked far enough
	 * west that the window no longer covered home, no town was drawn at all.
	 * Walking towards another town made it vanish as the window scrolled, which
	 * read from inside as being teleported into open country.
	 */
	wild_draw_town(w, p, c, loc(ox, oy));

	/*
	 * And the dungeon mouths (WLD-14), last of the furniture, so that neither a
	 * road nor a town's rock can be laid over the one thing in the block the
	 * player came to find.
	 */
	for (int i = 0; i < w->dungeon_count; i++) {
		struct loc at = loc(w->dungeons[i].grid.x - ox,
							w->dungeons[i].grid.y - oy);

		if (at.x < 1 || at.y < 1 || at.x >= span - 1 || at.y >= span - 1)
			continue;

		square_set_feat(c, at, FEAT_DUNGEON);
	}

	/*
	 * Every Angband level has an impassable boundary, and a great deal of code
	 * relies on it without saying so: monster group placement, object drops and
	 * others step outwards from a grid on the assumption that nothing can ever
	 * be standing on the outermost ring.  The surface has to honour that, or it
	 * gets walked off the edge of -- which it was, intermittently, by a monster
	 * placed on the last column bringing its friends with it.
	 *
	 * So the ring is always there.  It is drawn as open sea because at the edge
	 * of the world that is exactly what it is, and everywhere else the window
	 * scrolls while the player is still further from it than they can see, so
	 * it is never looked at.
	 */
	for (grid.x = 0; grid.x < span; grid.x++) {
		square_set_feat(c, loc(grid.x, 0), FEAT_WORLD_EDGE);
		square_set_feat(c, loc(grid.x, span - 1), FEAT_WORLD_EDGE);
	}
	for (grid.y = 0; grid.y < span; grid.y++) {
		square_set_feat(c, loc(0, grid.y), FEAT_WORLD_EDGE);
		square_set_feat(c, loc(span - 1, grid.y), FEAT_WORLD_EDGE);
	}

	if (offset) {
		offset->x = ox;
		offset->y = oy;
	}

	return c;
}


/**
 * Should the live surface be rebuilt around the player?
 *
 * The window is nine blocks across, so the player can walk a long way before
 * running out of it.  Rebuilding is not free -- it regenerates every block in
 * the window -- so it happens on approaching the border, not on every step
 * across a block boundary.
 *
 * Returns false when the window is already against the world's edge on that
 * side: there is nothing further to scroll to, rebuilding would produce an
 * identical surface, and the ring there is the real edge of the world and is
 * meant to be seen.
 */
bool wild_needs_recentre(struct player *p)
{
	int size = z_info->wild_block_size;
	int span = wild_view_blocks() * size;
	int world_max = wild ? wild->blocks * size : 0;
	int margin = wild_recentre_margin();
	int lx = p->wild_grid.x - p->wild_offset.x;
	int ly = p->wild_grid.y - p->wild_offset.y;

	if (!wild)
		return false;

	/* Near the west or east edge, with world left in that direction. */
	if (lx < margin && p->wild_offset.x > 0)
		return true;
	if (lx >= span - margin && p->wild_offset.x + span < world_max)
		return true;

	/* Near the north or south edge. */
	if (ly < margin && p->wild_offset.y > 0)
		return true;
	if (ly >= span - margin && p->wild_offset.y + span < world_max)
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

	/* And the world map fills in behind them (WLD-25). */
	wild_mark_seen(wild, p->wild_grid);

	/* And a town they walk into becomes somewhere they can be carried back to. */
	wild_note_visit(wild, p->wild_grid);

	/*
	 * The status line names the place the player is standing in, so it has to
	 * be redrawn when that changes.  It is flagged on a level change and
	 * nothing else, so walking out of a town left its name on the screen with
	 * open country all around -- which read as being in a town that was no
	 * longer there.
	 */
	{
		int now = wild_town_here(wild, p->wild_grid);

		if (now != wild_shown_town) {
			wild_shown_town = now;
			p->upkeep->redraw |= PR_DEPTH;
		}
	}
}
