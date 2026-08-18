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
#include "player.h"
#include "wild.h"

struct wilderness *wild = NULL;

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
/** How far beyond the town's own land its surroundings are judged, in blocks. */
#define WILD_TOWN_REACH		3

/** How far a unique may have wandered from where the player left it. */
#define WILD_UNIQUE_WANDER	5

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
	bool found = false;
	int best = 0;
	int bw, bh, reach, x, y;

	wild_town_extent(&bw, &bh);

	/* The footprint, plus a short walk beyond its edge. */
	reach = MAX(bw, bh) / 2 + WILD_TOWN_REACH;

	w->town_block = loc(centre, centre);

	for (y = bh; y < size - bh; y++) {
		for (x = bw; x < size - bw; x++) {
			struct wild_block *block = &w->map[y * size + x];
			int score, dist, fx, fy;
			int sum = 0, count = 0;
			bool ok = true;

			switch (block->terrain) {
				case WILD_TERRAIN_GRASS: score = 120; break;
				case WILD_TERRAIN_FOREST: score = 90; break;
				case WILD_TERRAIN_WASTE: score = 40; break;
				default: continue;	/* sea, shore, swamp, mountain */
			}

			for (fy = y - reach; fy <= y + reach && ok; fy++)
				for (fx = x - reach; fx <= x + reach && ok; fx++) {
					struct wild_block *f = wild_block_at(w, fx, fy);
					int d;

					if (!f) continue;

					/* The town's own ground has to be buildable and dry. */
					if (ABS(fx - x) <= bw / 2 && ABS(fy - y) <= bh / 2 &&
						(f->terrain == WILD_TERRAIN_OCEAN ||
						 f->terrain == WILD_TERRAIN_MOUNTAIN ||
						 (f->info & WILD_INFO_WATER))) {
						ok = false;
						continue;
					}

					d = wild_danger(w, fx, fy);
					sum += d;
					count++;
				}
			if (!ok) continue;

			/*
			 * The dominant term, by a wide margin.  Everything else here is a
			 * tie-break between sites that are already safe to walk out of.
			 *
			 * The mean and not the worst: over a window this size the worst
			 * block is near the maximum almost everywhere, so it carries no
			 * signal.  What distinguishes a good site from a bad one is
			 * whether the whole neighbourhood is orderly, which is what the
			 * mean measures.
			 */
			if (count)
				score -= (sum / count) * 8;

			score += block->pop / 4;

			/* All else equal, nearer the middle of the world. */
			dist = MAX(ABS(x - centre), ABS(y - centre));
			score -= dist * 2;

			/*
			 * Scores are routinely negative -- the danger term is large and
			 * subtracted -- so this tracks "have we seen any site at all"
			 * separately rather than using a sentinel value that a real score
			 * could fail to beat.
			 */
			if (!found || score > best) {
				found = true;
				best = score;
				w->town_block = loc(x, y);
			}
		}
	}

	/*
	 * Nothing scored: either the world offers no buildable ground, or it is
	 * too small for a town to fit in the first place.  Rather than leave the
	 * town on the middle block whatever that turns out to be -- open sea, in
	 * the case that found this -- take the driest, flattest block there is.
	 */
	if (!found) {
		int best_hgt = -1;

		for (y = 0; y < size; y++)
			for (x = 0; x < size; x++) {
				struct wild_block *b = &w->map[y * size + x];
				int score;

				if (b->terrain == WILD_TERRAIN_OCEAN) continue;
				if (b->info & WILD_INFO_WATER) continue;

				score = 255 - ABS((int) b->hgt - 128);
				if (score > best_hgt) {
					best_hgt = score;
					w->town_block = loc(x, y);
				}
			}
	}

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
		int bx = i % size, by = i / size;
		int rim = MIN(MIN(bx, by), MIN(size - 1 - bx, size - 1 - by));

		block->hgt = (uint8_t) hgt[i];
		block->pop = (uint8_t) pop[i];
		block->law = (uint8_t) law[i];
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
static struct chunk *wild_town = NULL;

void wild_town_free(void)
{
	if (wild_town) {
		cave_free(wild_town);
		wild_town = NULL;
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
} wild_gates[WILD_TOWN_GATES];

static int wild_gate_count = 0;

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
static bool wild_town_cut_gate(struct chunk *town, struct loc start,
							   struct loc step, struct loc across, int limit)
{
	struct loc lane[2];
	int reach[2];
	int i, k, depth;

	if (wild_gate_count + 2 > WILD_TOWN_GATES)
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
			square_set_feat(town, grid, FEAT_ROAD);
			grid = loc_sum(grid, step);
		}
	}

	/* And hang the doors in the boundary itself. */
	for (i = 0; i < 2; i++) {
		square_set_feat(town, lane[i], FEAT_CLOSED);
		wild_gates[wild_gate_count].grid = lane[i];
		wild_gates[wild_gate_count].opened = 0;
		wild_gate_count++;
	}

	return true;
}

/**
 * Give the town one gate on each of its four sides.
 *
 * Tried from the middle of each side outwards, so a gate stands where a gate
 * would: at the end of a street rather than in a corner.
 */
static void wild_town_open(struct chunk *town)
{
	int w = town->width, h = town->height;
	int i;

	wild_gate_count = 0;
	wild_town_wall(town);

	for (i = 0; i < w - 3; i++) {
		int x = w / 2 + ((i % 2) ? -((i + 1) / 2) : (i / 2));

		if (x < 1 || x > w - 3) continue;
		if (wild_town_cut_gate(town, loc(x, 1), loc(0, 1), loc(1, 0), h / 2))
			break;
	}

	for (i = 0; i < w - 3; i++) {
		int x = w / 2 + ((i % 2) ? -((i + 1) / 2) : (i / 2));

		if (x < 1 || x > w - 3) continue;
		if (wild_town_cut_gate(town, loc(x, h - 2), loc(0, -1), loc(1, 0),
							   h / 2))
			break;
	}

	for (i = 0; i < h - 3; i++) {
		int y = h / 2 + ((i % 2) ? -((i + 1) / 2) : (i / 2));

		if (y < 1 || y > h - 3) continue;
		if (wild_town_cut_gate(town, loc(1, y), loc(1, 0), loc(0, 1), w / 2))
			break;
	}

	for (i = 0; i < h - 3; i++) {
		int y = h / 2 + ((i % 2) ? -((i + 1) / 2) : (i / 2));

		if (y < 1 || y > h - 3) continue;
		if (wild_town_cut_gate(town, loc(w - 2, y), loc(-1, 0), loc(0, 1),
							   w / 2))
			break;
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
	struct loc org = wild_town_origin(w);
	int i;

	for (i = 0; i < wild_gate_count; i++) {
		struct loc grid = loc(org.x + wild_gates[i].grid.x - offset.x,
							  org.y + wild_gates[i].grid.y - offset.y);

		if (!square_in_bounds_fully(c, grid))
			continue;

		if (!square_isopendoor(c, grid)) {
			/* Shut, or broken beyond shutting.  Either way, not our business. */
			wild_gates[i].opened = 0;
			continue;
		}

		if (!wild_gates[i].opened) {
			wild_gates[i].opened = turn;
			continue;
		}

		if (turn - wild_gates[i].opened < (int32_t) z_info->wild_gate_turns)
			continue;

		/* Not onto somebody standing in the gateway. */
		if (square_monster(c, grid) || loc_eq(grid, player->grid))
			continue;

		square_close_door(c, grid);
		square_memorize(c, grid);
		square_light_spot(c, grid);
		wild_gates[i].opened = 0;

		if (square_isseen(c, grid))
			msg("The town gate swings shut.");
	}
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
		wild_town_open(wild_town);
	}

	return wild_town;
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
	struct loc org = wild_town_origin(w);
	struct loc grid;

	for (grid.y = 0; grid.y < z_info->town_hgt; grid.y++)
		for (grid.x = 0; grid.x < z_info->town_wid; grid.x++) {
			struct loc dest = loc(org.x + grid.x - offset.x,
								  org.y + grid.y - offset.y);

			if (!square_in_bounds_fully(c, dest))
				continue;

			square_memorize(c, dest);
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
	struct loc org = loc(wild_town_origin(w).x - offset.x,
						 wild_town_origin(w).y - offset.y);
	int placed = 0, tries = 0;

	/* Only when the town is actually on the live surface. */
	if (org.x + z_info->town_wid <= 0 || org.x >= c->width ||
		org.y + z_info->town_hgt <= 0 || org.y >= c->height)
		return;

	while (placed < residents && tries < residents * 50) {
		struct loc grid = loc(org.x + randint0(z_info->town_wid),
							  org.y + randint0(z_info->town_hgt));

		tries++;

		if (!square_in_bounds_fully(c, grid)) continue;
		if (!square_isempty(c, grid)) continue;

		/* Not on the doorstep: leave the player room to arrive. */
		if (distance(grid, p->grid) < 3) continue;

		if (pick_and_place_monster(c, grid, 0, true, true, ORIGIN_DROP))
			placed++;
	}
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
			object_free(relic->obj);
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
 */
int wild_danger(struct wilderness *w, int x, int y)
{
	struct wild_block *block = wild_block_at(w, x, y);
	int danger;

	if (!block)
		return 0;

	/* A town is a town: nothing hunts in the market square. */
	if (wild_in_town(w, x, y))
		return 0;

	danger = (256 - block->law) / 4;

	return MAX(1, danger - 5);
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
 * Is the town actually on this block?
 *
 * Distinct from the block's `place` mark, which covers the land reserved for
 * the town when it was sited -- a margin of a block on every side, so that a
 * town does not butt against whatever the next block turns out to be and so
 * there is somewhere for a road to leave from.  That reserved area is 35 blocks
 * against the town's own 15, and treating it as "town" made the overhead map
 * paint a slab of masonry more than twice the size of the place, with the
 * player standing in it while plainly out in the fields.
 *
 * `place` answers "is this land spoken for".  This answers "is the town here",
 * which is the question the map and the monster placement are really asking.
 */
bool wild_in_town(struct wilderness *w, int bx, int by)
{
	int size = z_info->wild_block_size;
	struct loc org;

	if (!w || !wild_in_bounds(w, bx, by))
		return false;

	org = wild_town_origin(w);

	return bx >= org.x / size &&
		   bx <= (org.x + z_info->town_wid - 1) / size &&
		   by >= org.y / size &&
		   by <= (org.y + z_info->town_hgt - 1) / size;
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

	/* Roads run east-west through the middle of the blocks that carry them. */
	for (int by = 0; by < view; by++) {
		for (int bx = 0; bx < view; bx++) {
			struct wild_block *block =
				wild_block_at(w, ox / size + bx, oy / size + by);

			if (!block || !(block->info & WILD_INFO_ROAD))
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
}
