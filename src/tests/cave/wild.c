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

/** How many trades a set of store bits names. */
static int trades_in(uint16_t stores) {
	int n, count = 0;

	for (n = 0; n < 16; n++)
		if (stores & (1u << n)) count++;

	return count;
}

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
	z_info->wild_rivers = 4;
	z_info->wild_lakes = 4;
	z_info->wild_towns = 12;
	z_info->store_max = 8;

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

/* The world has rivers in it, and they run rather than sit in puddles. */
static int test_rivers_are_paths(void *state) {
	struct wilderness *w = wild_new(65, 99991);
	int size = 65, x, y;
	int wet = 0, joined = 0;

	wild_generate(w);

	for (y = 0; y < size; y++)
		for (x = 0; x < size; x++) {
			int i, j, neighbours = 0;

			if (!(w->map[y * size + x].info & WILD_INFO_WATER)) continue;
			wet++;

			for (j = -1; j <= 1; j++)
				for (i = -1; i <= 1; i++) {
					struct wild_block *n;

					if (!i && !j) continue;
					n = wild_block_at(w, x + i, y + j);
					if (n && (n->info & WILD_INFO_WATER)) neighbours++;
				}

			if (neighbours) joined++;
		}

	/* There is water. */
	require(wet > 0);

	/*
	 * And nearly all of it is joined to more of itself. A river drawn as a
	 * line of blocks has every block touching another; scattered singletons
	 * would mean the linking had failed and left ponds.
	 */
	require(joined * 10 >= wet * 9);

	wild_free(w);
	ok;
}

/*
 * Water is drawn only where the map says there is water.
 *
 * This is a regression test with a specific fault behind it. The first attempt
 * read the block flags as a field and interpolated them between block centres,
 * which put a broad band of near-threshold values across the countryside: the
 * result was a fourteen-grid-wide channel with open water speckled through the
 * fields on either side of it. Drawing the flagged blocks as a path instead
 * confines the water to the blocks that carry it.
 */
static int test_water_stays_in_its_blocks(void *state) {
	struct wilderness *w = wild_new(65, 4242);
	int size = z_info->wild_block_size;
	int x, y, channel = 0;

	wild_generate(w);

	for (y = 0; y < 65 * size; y += 3)
		for (x = 0; x < 65 * size; x += 3) {
			struct wild_block *block = wild_block_at(w, x / size, y / size);
			int wet = wild_water_at(w, x, y);

			if (block && (block->info & WILD_INFO_WATER)) {
				if (wet > 0) channel++;
			} else {
				eq(wet, 0);
			}
		}

	/* And the channel is actually drawn, not merely permitted. */
	require(channel > 0);

	wild_free(w);
	ok;
}

/* A town is not built in a river. */
static int test_towns_keep_their_feet_dry(void *state) {
	int seed;

	for (seed = 1; seed <= 12; seed++) {
		struct wilderness *w = wild_new(65, seed * 5077);
		int x, y;

		wild_generate(w);

		for (y = 0; y < 65; y++)
			for (x = 0; x < 65; x++) {
				struct wild_block *b = &w->map[y * 65 + x];

				if (!b->place) continue;
				require(!(b->info & WILD_INFO_WATER));
			}

		wild_free(w);
	}

	ok;
}


/**
 * Larger places keep more trades, and the starting village keeps fewest.
 *
 * The village the character begins in is the reference the world is built
 * against: it is the smallest band, and it holds only what nobody can start
 * without.  Everything else is a reason to travel.
 */
static int test_bigger_places_keep_more_trades(void *state) {
	int seed;

	for (seed = 1; seed <= 20; seed++) {
		struct wilderness *w = wild_new(129, seed * 7717 + 3);
		int i, start_trades;

		wild_generate(w);
		require(w->town_count > 0);

		/* The character always starts in a village. */
		require(w->towns[0].band == 0);
		start_trades = trades_in(w->towns[0].stores);

		for (i = 0; i < w->town_count; i++) {
			struct wild_town *town = &w->towns[i];
			int trades = trades_in(town->stores);

			/* A band's size and its trades rise together. */
			require(town->band >= 0 && town->band <= 3);
			if (i > 0 && town->band > 0)
				require(trades > start_trades);
			require(trades <= (int) z_info->store_max);

			/* Larger bands stand on more ground. */
			if (town->band > w->towns[0].band) {
				require(town->wid > w->towns[0].wid);
				require(town->hgt > w->towns[0].hgt);
			}

			/* Nowhere is without somewhere to buy food or leave a pack. */
			require(town->stores & (1u << WILD_STORE_GENERAL));
			require(town->stores & (1u << WILD_STORE_HOME));
		}

		/*
		 * No class begins with a spellbook, so the starting village must sell
		 * one -- and a potion of cure light wounds.
		 */
		require(w->towns[0].stores & (1u << WILD_STORE_BOOK));
		require(w->towns[0].stores & (1u << WILD_STORE_ALCHEMY));

		wild_free(w);
	}

	ok;
}

/**
 * Two places of the same size need not keep the same trades (WLD-11a).
 */
static int test_trades_vary_between_places(void *state) {
	uint16_t seen[64];
	int seed, n = 0, distinct = 0;

	for (seed = 1; seed <= 20 && n < 64; seed++) {
		struct wilderness *w = wild_new(129, seed * 4409 + 11);
		int i;

		wild_generate(w);

		/* Gather the trades of every place that is not the start. */
		for (i = 1; i < w->town_count && n < 64; i++)
			seen[n++] = w->towns[i].stores;

		wild_free(w);
	}

	require(n > 8);

	for (seed = 0; seed < n; seed++) {
		int j;
		bool first = true;

		for (j = 0; j < seed; j++)
			if (seen[j] == seen[seed]) first = false;
		if (first) distinct++;
	}

	/* Not one set of shops repeated across the whole world. */
	require(distinct > 2);

	ok;
}



/**
 * Every town can be walked to from the village, along roads (WLD-08).
 *
 * This is the property the player actually needs.  A world with roads in it is
 * no use if the road out of the village goes nowhere: what matters is that
 * following roads from home reaches every other town in the world.
 */
static int test_roads_join_every_town(void *state) {
	int seed;

	int wet = 0, dry = 0;

	for (seed = 1; seed <= 12; seed++) {
		struct wilderness *w = wild_new(129, seed * 7717);
		bool *seen;
		int *queue;
		int head = 0, tail = 0, i;

		wild_generate(w);
		require(w->town_count > 1);

		seen = mem_zalloc(129 * 129 * sizeof(*seen));
		queue = mem_zalloc(129 * 129 * sizeof(*queue));

		/* Flood out from the village, across road blocks only. */
		{
			int start = w->towns[0].block.y * 129 + w->towns[0].block.x;

			require(wild_road_at(w, w->towns[0].block.x, w->towns[0].block.y));
			seen[start] = true;
			queue[tail++] = start;
		}

		while (head < tail) {
			int node = queue[head++];
			int bx = node % 129, by = node / 129;
			static const int dx[4] = { -1, 1, 0, 0 };
			static const int dy[4] = { 0, 0, -1, 1 };

			for (i = 0; i < 4; i++) {
				int nx = bx + dx[i], ny = by + dy[i], next;

				if (!wild_in_bounds(w, nx, ny)) continue;
				if (!wild_road_at(w, nx, ny)) continue;

				next = ny * 129 + nx;
				if (seen[next]) continue;

				seen[next] = true;
				queue[tail++] = next;
			}
		}

		/* Every town is on the network the village is on. */
		for (i = 0; i < w->town_count; i++) {
			int b = w->towns[i].block.y * 129 + w->towns[i].block.x;

			require(wild_road_at(w, w->towns[i].block.x, w->towns[i].block.y));
			require(seen[b]);
		}

		for (i = 0; i < 129 * 129; i++)
			if (w->map[i].info & WILD_INFO_ROAD) {
				if (w->map[i].terrain == WILD_TERRAIN_OCEAN) wet++;
				else dry++;
			}

		mem_free(queue);
		mem_free(seen);
		wild_free(w);
	}

	/*
	 * Roads keep to the land.  A causeway is allowed, because two towns can end
	 * up on either side of an inland sea and joining them matters more than
	 * keeping their feet dry -- but it is the exception the sea is opened for,
	 * not a short cut across every bay.  Measured over these twelve worlds:
	 * one of them needs a causeway, of twelve blocks, out of some three
	 * thousand road blocks in all.
	 */
	require(dry > 1000);
	require(wet * 50 < dry);

	ok;
}

/**
 * Roads take the trouble to go round things (WLD-08).
 *
 * A road is worth following because it goes somewhere sensible.  Routed rather
 * than drawn straight, it should cross far less mountain and swamp than the
 * country it runs through holds.
 */
static int test_roads_avoid_bad_ground(void *state) {
	int road_rough = 0, road_all = 0, world_rough = 0, world_all = 0;
	int seed, i;

	for (seed = 1; seed <= 12; seed++) {
		struct wilderness *w = wild_new(129, seed * 7717);

		wild_generate(w);

		for (i = 0; i < 129 * 129; i++) {
			bool rough = w->map[i].terrain == WILD_TERRAIN_MOUNTAIN ||
						 w->map[i].terrain == WILD_TERRAIN_SWAMP;

			/* The sea is not ground a road would ever want, so leave it out. */
			if (w->map[i].terrain == WILD_TERRAIN_OCEAN) continue;

			world_all++;
			if (rough) world_rough++;

			if (w->map[i].info & WILD_INFO_ROAD) {
				road_all++;
				if (rough) road_rough++;
			}
		}

		wild_free(w);
	}

	require(road_all > 100);

	/* Measured: roads run about a fifth as much rough ground as the land holds. */
	require(road_rough * world_all * 2 < world_rough * road_all);

	ok;
}

const char *suite_name = "cave/wild";
struct test tests[] = {
	{ "generation is deterministic", test_generation_is_deterministic },
	{ "block seeds are stable and distinct", test_block_seeds },
	{ "block seeds are scattered", test_block_seeds_are_scattered },
	{ "bounds are checked", test_bounds },
	{ "terrain classification is total", test_classification },
	{ "generated worlds are plausible", test_world_is_plausible },
	{ "terrain features are valid", test_terrain_features },
	{ "rivers are paths", test_rivers_are_paths },
	{ "water stays in its blocks", test_water_stays_in_its_blocks },
	{ "towns keep their feet dry", test_towns_keep_their_feet_dry },
	{ "bigger places keep more trades", test_bigger_places_keep_more_trades },
	{ "trades vary between places", test_trades_vary_between_places },
	{ "roads join every town", test_roads_join_every_town },
	{ "roads avoid bad ground", test_roads_avoid_bad_ground },
	{ NULL, NULL }
};
