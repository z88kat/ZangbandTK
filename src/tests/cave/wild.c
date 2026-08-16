/* cave/wild
 *
 * Tests for the ZangbandTK wilderness world map (WLD-01 to WLD-03, WLD-07).
 *
 * The property that matters most is determinism: a block must generate
 * identically from the world seed and its coordinates, however the player
 * reached it. Everything about blocks-on-demand depends on that.
 */

#include "unit-test.h"

#include "cave.h"
#include "init.h"
#include "wild.h"

int setup_tests(void **state) {
	/*
	 * Unit tests do not load game data, so supply the constants the
	 * wilderness reads. cave_new() needs a monster capacity to size its
	 * arrays from, and block generation needs the block dimensions.
	 */
	z_info = mem_zalloc(sizeof(struct angband_constants));
	z_info->level_monster_max = 1024;
	z_info->wild_blocks = 33;
	z_info->wild_block_size = 16;
	z_info->wild_cache_blocks = 81;

	*state = NULL;
	return 0;
}

int teardown_tests(void *state) {
	mem_free(z_info);
	return 0;
}

/* A world is reproducible from its seed alone. */
static int test_generation_is_deterministic(void *state) {
	struct wilderness *a = wild_new(33, 12345);
	struct wilderness *b = wild_new(33, 12345);
	struct wilderness *c = wild_new(33, 54321);
	int i, differences = 0;

	wild_generate(a);
	wild_generate(b);
	wild_generate(c);

	for (i = 0; i < 33 * 33; i++) {
		require(a->map[i].hgt == b->map[i].hgt);
		require(a->map[i].pop == b->map[i].pop);
		require(a->map[i].law == b->map[i].law);
		require(a->map[i].terrain == b->map[i].terrain);
		if (a->map[i].hgt != c->map[i].hgt)
			differences++;
	}

	/* A different seed must give a genuinely different world. */
	require(differences > 33 * 33 / 4);

	wild_free(a);
	wild_free(b);
	wild_free(c);
	ok;
}

/* Block seeds depend on the world seed and the coordinates, and nothing else. */
static int test_block_seeds(void *state) {
	struct wilderness *a = wild_new(33, 999);
	struct wilderness *b = wild_new(33, 1000);

	/* Stable for the same inputs */
	require(wild_block_seed(a, 4, 7) == wild_block_seed(a, 4, 7));

	/* Distinct per coordinate, including the transpose */
	require(wild_block_seed(a, 4, 7) != wild_block_seed(a, 7, 4));
	require(wild_block_seed(a, 4, 7) != wild_block_seed(a, 5, 7));
	require(wild_block_seed(a, 4, 7) != wild_block_seed(a, 4, 8));

	/* Distinct per world */
	require(wild_block_seed(a, 4, 7) != wild_block_seed(b, 4, 7));

	wild_free(a);
	wild_free(b);
	ok;
}

/* Neighbouring blocks must not produce visibly related seeds. */
static int test_block_seeds_are_scattered(void *state) {
	struct wilderness *w = wild_new(33, 42);
	uint32_t seeds[64];
	int n = 0, x, y, i, j;

	for (y = 0; y < 8; y++)
		for (x = 0; x < 8; x++)
			seeds[n++] = wild_block_seed(w, x, y);

	for (i = 0; i < n; i++)
		for (j = i + 1; j < n; j++)
			require(seeds[i] != seeds[j]);

	wild_free(w);
	ok;
}

/* Bounds checking is honest at every edge. */
static int test_bounds(void *state) {
	struct wilderness *w = wild_new(33, 1);

	require(wild_in_bounds(w, 0, 0));
	require(wild_in_bounds(w, 32, 32));
	require(!wild_in_bounds(w, -1, 0));
	require(!wild_in_bounds(w, 0, -1));
	require(!wild_in_bounds(w, 33, 0));
	require(!wild_in_bounds(w, 0, 33));
	require(!wild_in_bounds(NULL, 0, 0));

	require(wild_block_at(w, 0, 0) != NULL);
	require(wild_block_at(w, 33, 0) == NULL);
	require(wild_block_at(w, -1, -1) == NULL);

	wild_free(w);
	ok;
}

/* Terrain classification is total and matches its documented intent. */
static int test_classification(void *state) {
	int hgt, pop, law;

	/* Low ground is sea, high ground is mountain. */
	require(wild_classify(0, 128, 128) == WILD_TERRAIN_OCEAN);
	require(wild_classify(255, 128, 128) == WILD_TERRAIN_MOUNTAIN);

	/* Settled land is cleared land. */
	require(wild_classify(150, 200, 128) == WILD_TERRAIN_GRASS);

	/* Lawless emptiness goes to waste; ordered emptiness stays wooded. */
	require(wild_classify(150, 10, 10) == WILD_TERRAIN_WASTE);
	require(wild_classify(150, 10, 200) == WILD_TERRAIN_FOREST);

	/* Low and lawless is swamp. */
	require(wild_classify(80, 10, 10) == WILD_TERRAIN_SWAMP);

	/* Every point in the space classifies to something valid. */
	for (hgt = 0; hgt < 256; hgt += 5)
		for (pop = 0; pop < 256; pop += 15)
			for (law = 0; law < 256; law += 15)
				require(wild_classify(hgt, pop, law) < WILD_TERRAIN_MAX);

	ok;
}

/* The world should be varied, about a quarter water, and ringed by sea. */
static int test_world_is_plausible(void *state) {
	int counts[WILD_TERRAIN_MAX];
	int size = 129, margin = 3;
	int seed, kinds_seen = 0, i, inland = 0;

	memset(counts, 0, sizeof(counts));

	/* Aggregate over several worlds so one unlucky seed cannot decide it. */
	for (seed = 1; seed <= 20; seed++) {
		struct wilderness *w = wild_new(size, seed * 7919);
		int x, y;

		wild_generate(w);

		for (y = 0; y < size; y++)
			for (x = 0; x < size; x++) {
				int rim = MIN(MIN(x, y), MIN(size - 1 - x, size - 1 - y));
				struct wild_block *b = &w->map[y * size + x];

				/*
				 * The rim is forced to sea so the world ends in ocean rather
				 * than stopping dead, so it is checked separately and left out
				 * of the composition figures -- which are about what
				 * wild_classify()'s thresholds ask for, not about the margin.
				 */
				if (rim < margin) {
					require(b->terrain == WILD_TERRAIN_OCEAN);
					continue;
				}

				counts[b->terrain]++;
				inland++;
			}

		wild_free(w);
	}

	for (i = 0; i < WILD_TERRAIN_MAX; i++)
		if (counts[i] > 0)
			kinds_seen++;

	/* Not a monoculture: most terrain kinds should appear somewhere. */
	require(kinds_seen >= WILD_TERRAIN_MAX - 1);

	/*
	 * Composition must match what wild_classify()'s thresholds ask for. This
	 * is the property rank equalisation exists to provide: before it, the
	 * fractal's bell-shaped output made a world 49% ocean against a threshold
	 * asking for 25%.
	 */
	{
		int ocean_pct = 100 * counts[WILD_TERRAIN_OCEAN] / inland;

		require(ocean_pct >= 20 && ocean_pct <= 32);
	}

	ok;
}

/* Every terrain kind lays down a valid feature for every roll. */
static int test_terrain_features(void *state) {
	int terrain, roll;

	for (terrain = 0; terrain < WILD_TERRAIN_MAX; terrain++) {
		for (roll = 0; roll < 100; roll++) {
			int feat = wild_terrain_feat(terrain, roll);

			require(feat > FEAT_NONE);
			require(feat < FEAT_MAX);
		}
	}

	/* Each kind's commonest feature is the one that names it. */
	require(wild_terrain_feat(WILD_TERRAIN_OCEAN, 0) == FEAT_DEEP_WATER);
	require(wild_terrain_feat(WILD_TERRAIN_MOUNTAIN, 0) == FEAT_ROCK);
	require(wild_terrain_feat(WILD_TERRAIN_FOREST, 0) == FEAT_TREE);

	ok;
}

/*
 * Note: anything that calls square_set_feat() is not unit-tested here, since it
 * reads f_info and the unit-test harness does not load game data. Those parts
 * are covered against the real data by game/wild instead — testing them here
 * would mean faking terrain, which would prove only that the fake works.
 */

const char *suite_name = "cave/wild";
struct test tests[] = {
	{ "generation is deterministic", test_generation_is_deterministic },
	{ "block seeds are stable and distinct", test_block_seeds },
	{ "block seeds are scattered", test_block_seeds_are_scattered },
	{ "bounds are checked", test_bounds },
	{ "terrain classification is total", test_classification },
	{ "generated worlds are plausible", test_world_is_plausible },
	{ "terrain features are valid", test_terrain_features },
	{ NULL, NULL }
};
