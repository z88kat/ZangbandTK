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
#include "init.h"
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

	Rand_quick = false;

	mem_free(hgt);
	mem_free(pop);
	mem_free(law);
}
