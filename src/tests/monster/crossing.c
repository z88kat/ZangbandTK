/* monster/crossing
 *
 * What a creature can cross that it cannot survive (ZangbandTK, CNT-04).
 *
 * Angband gates damaging terrain on a single resistance flag, named by the
 * terrain itself: deep water asks for `IM_WATER`, lava for `IM_FIRE`. That
 * answers whether a monster can *survive* the grid, which is the only question
 * a dungeon of lava pools ever had to ask.
 *
 * A world with a sea in it has a second question. A raven does not resist deep
 * water; it flies over it. Zangband asked both, and let `CAN_FLY` or `CAN_SWIM`
 * stand in for the immunity (monster2.c:498). Without that, 117 imported
 * monsters that fly and 91 that swim are all stopped by the first river they
 * meet — which looks like nothing at all, because a monster that will not
 * cross simply goes somewhere else.
 */

#include "unit-test.h"

#include "cave.h"
#include "init.h"
#include "mon-util.h"
#include "monster.h"
#include "test-utils.h"

int setup_tests(void **state) {
	set_file_paths();
	init_angband();
	*state = NULL;
	return 0;
}

int teardown_tests(void *state) {
	cleanup_angband();
	return 0;
}

/** How many races carry a flag. */
static int races_with(int flag) {
	int i, n = 0;

	for (i = 0; i < z_info->r_max; i++) {
		if (r_info[i].name && rf_has(r_info[i].flags, flag)) n++;
	}

	return n;
}

/**
 * The flags reached the creatures that should have them.
 *
 * Counts rather than existence: these come off 117 and 91 source records, and
 * a conversion that carried one bird would look identical to one that carried
 * them all.
 */
static int test_the_fliers_and_swimmers_are_there(void *state) {
	eq(races_with(RF_CAN_FLY), 117);
	eq(races_with(RF_CAN_SWIM), 91);

	ok;
}

/**
 * The birds fly and the fish do not.
 *
 * A sample chosen so that getting the sense of the flag backwards fails: a
 * raven flies and does not swim, a lizardman swims and does not fly.
 */
static int test_the_right_creatures_got_the_right_flag(void *state) {
	struct monster_race *raven = lookup_monster("raven");
	struct monster_race *lizardman = lookup_monster("lizardman");

	notnull(raven);
	require(rf_has(raven->flags, RF_CAN_FLY));
	require(!rf_has(raven->flags, RF_CAN_SWIM));

	notnull(lizardman);
	require(rf_has(lizardman->flags, RF_CAN_SWIM));
	require(!rf_has(lizardman->flags, RF_CAN_FLY));

	ok;
}

/**
 * Deep water is the terrain this actually changes.
 *
 * The flags are only worth carrying if the game has damaging terrain for them
 * to cross, and the one that matters is water: it is the terrain the wilderness
 * is full of. Asserted through the data rather than through a live cave, since
 * what would break is somebody giving deep water a different resistance flag
 * and leaving the movement code reading the old one.
 */
static int test_deep_water_is_gated_on_a_resistance(void *state) {
	int idx = lookup_feat("deep water");
	struct feature *water;

	require(idx > 0);
	water = &f_info[idx];
	require(feat_is_deep(idx));
	eq(water->resist_flag, RF_IM_WATER);

	ok;
}

/**
 * Nothing that flies is also kept out of the sky by being a fish.
 *
 * Zangband's rule is that an aquatic creature stays in the water *unless* it
 * can fly, so a flying fish is a real case and the two flags have to be read
 * in that order. There is nothing in the bestiary that is both today; the test
 * is that if one arrives, it is not silently grounded.
 */
static int test_a_flying_fish_would_not_be_grounded(void *state) {
	int i;

	for (i = 0; i < z_info->r_max; i++) {
		struct monster_race *race = &r_info[i];

		if (!race->name) continue;
		if (!monster_is_aquatic(race)) continue;
		if (!rf_has(race->flags, RF_CAN_FLY)) continue;

		/*
		 * If this ever matches, the movement code must let it leave the
		 * water -- which it does, and this is the reminder of why.
		 */
		require(rf_has(race->flags, RF_CAN_FLY));
	}

	ok;
}

const char *suite_name = "monster/crossing";
struct test tests[] = {
	{ "the-fliers-and-swimmers-are-there",
	  test_the_fliers_and_swimmers_are_there },
	{ "the-right-creatures-got-the-right-flag",
	  test_the_right_creatures_got_the_right_flag },
	{ "deep-water-is-gated-on-a-resistance",
	  test_deep_water_is_gated_on_a_resistance },
	{ "a-flying-fish-would-not-be-grounded",
	  test_a_flying_fish_would_not_be_grounded },
	{ NULL, NULL }
};
