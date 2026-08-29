/* monster/restele
 *
 * Monsters that will not be moved (ZangbandTK, CNT-04).
 *
 * RES_TELE reads like an immunity and is not one. A unique that has it is
 * unaffected outright; anything else gets a saving throw on its hit points, so
 * the same monster may be shifted one turn and stand its ground the next. These
 * check both branches are reachable and that the saving throw is really a roll
 * -- a resist that always succeeds and a resist that never does look identical
 * from inside a single fight, and both would be wrong.
 */

#include "unit-test.h"

#include "init.h"
#include "mon-lore.h"
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

/** A monster of the given race, with nothing else set. */
static struct monster stub_of(struct monster_race *race) {
	struct monster mon;

	memset(&mon, 0, sizeof(mon));
	mon.race = race;

	return mon;
}

/** The first race carrying the flag, unique or not as asked. */
static struct monster_race *a_resister(bool unique) {
	int i;

	for (i = 0; i < z_info->r_max; i++) {
		struct monster_race *race = &r_info[i];

		if (!race->name) continue;
		if (!rf_has(race->flags, RF_RES_TELE)) continue;
		if (rf_has(race->flags, RF_UNIQUE) != unique) continue;

		return race;
	}

	return NULL;
}

/**
 * The flag reached the bestiary, and both branches have somebody in them.
 */
static int test_the_resisters_are_there(void *state) {
	int i, resisters = 0, uniques = 0, ordinary = 0;

	for (i = 0; i < z_info->r_max; i++) {
		struct monster_race *race = &r_info[i];

		if (!race->name) continue;
		if (!rf_has(race->flags, RF_RES_TELE)) continue;

		resisters++;
		if (rf_has(race->flags, RF_UNIQUE)) uniques++;
		else ordinary++;
	}

	eq(resisters, 102);

	/* Both halves of the rule need somebody to apply to. */
	require(uniques > 0);
	require(ordinary > 0);

	ok;
}

/**
 * A unique that has the flag is unaffected, every time.
 */
static int test_a_unique_is_never_moved(void *state) {
	struct monster_race *race = a_resister(true);
	int i;

	notnull(race);

	for (i = 0; i < 200; i++) {
		struct monster mon = stub_of(race);
		bool unaffected = false;

		require(monster_resists_teleport(&mon, false, &unaffected));
		require(unaffected);
	}

	ok;
}

/**
 * Anything else gets a roll, and the roll goes both ways.
 *
 * The point of the test. Two hundred tries on a monster whose odds are neither
 * of the extremes should produce some of each; if it does not, the saving throw
 * has become an immunity or a no-op and nothing in play would show it.
 */
static int test_an_ordinary_monster_gets_a_roll(void *state) {
	struct monster_race *race = NULL;
	int i, resisted = 0, moved = 0;

	/*
	 * Something in the middle of the curve: the threshold is 5000 against twice
	 * the average hit points, so a monster of a few hundred is well away from
	 * always and never.
	 */
	for (i = 0; i < z_info->r_max; i++) {
		struct monster_race *r = &r_info[i];

		if (!r->name) continue;
		if (!rf_has(r->flags, RF_RES_TELE)) continue;
		if (rf_has(r->flags, RF_UNIQUE)) continue;
		if (r->avg_hp < 200 || r->avg_hp > 2000) continue;

		race = r;
		break;
	}

	notnull(race);

	for (i = 0; i < 200; i++) {
		struct monster mon = stub_of(race);
		bool unaffected = true;

		if (monster_resists_teleport(&mon, false, &unaffected)) {
			resisted++;
			/* Never the outright shrug: that is the uniques' branch. */
			require(!unaffected);
		} else {
			moved++;
		}
	}

	require(resisted > 0);
	require(moved > 0);

	ok;
}

/**
 * Without the flag, nothing resists.
 */
static int test_without_the_flag_nothing_resists(void *state) {
	struct monster_race *race = NULL;
	int i;

	for (i = 0; i < z_info->r_max; i++) {
		struct monster_race *r = &r_info[i];

		if (!r->name) continue;
		if (rf_has(r->flags, RF_RES_TELE)) continue;

		race = r;
		break;
	}

	notnull(race);

	for (i = 0; i < 200; i++) {
		struct monster mon = stub_of(race);
		bool unaffected = true;

		require(!monster_resists_teleport(&mon, false, &unaffected));
		require(!unaffected);
	}

	ok;
}

/**
 * Watching it happen is how the flag is learned, and not watching is not.
 */
static int test_seeing_it_teaches_the_flag(void *state) {
	struct monster_race *race = a_resister(true);
	struct monster mon;
	struct monster_lore *lore;

	notnull(race);
	lore = get_lore(race);
	notnull(lore);

	/* Out of sight, nothing is learned however many times it happens. */
	rf_off(lore->flags, RF_RES_TELE);
	mon = stub_of(race);
	require(monster_resists_teleport(&mon, false, NULL));
	require(!rf_has(lore->flags, RF_RES_TELE));

	/* In sight, once is enough. */
	mon = stub_of(race);
	require(monster_resists_teleport(&mon, true, NULL));
	require(rf_has(lore->flags, RF_RES_TELE));

	ok;
}

const char *suite_name = "monster/restele";
struct test tests[] = {
	{ "the-resisters-are-there", test_the_resisters_are_there },
	{ "a-unique-is-never-moved", test_a_unique_is_never_moved },
	{ "an-ordinary-monster-gets-a-roll", test_an_ordinary_monster_gets_a_roll },
	{ "without-the-flag-nothing-resists", test_without_the_flag_nothing_resists },
	{ "seeing-it-teaches-the-flag", test_seeing_it_teaches_the_flag },
	{ NULL, NULL }
};
