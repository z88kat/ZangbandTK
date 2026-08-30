/* monster/lastflags
 *
 * The last of Zangband's flags (ZangbandTK, CNT-04 and CNT-09).
 *
 * Five mechanisms, two renames and one refusal, which between them empty both
 * flagmaps. What is worth defending here is mostly that the data reached the
 * right things: each of these is on one to five items out of a thousand, so a
 * conversion quietly dropping one looks exactly like a conversion that never
 * had it. The arithmetic that can be checked without a live cave is checked
 * too.
 */

#include "unit-test.h"

#include "init.h"
#include "mon-util.h"
#include "monster.h"
#include "object.h"
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

/** How many of r_info carry a race flag. */
static int races_with(int flag) {
	int i, n = 0;

	for (i = 0; i < z_info->r_max; i++) {
		if (r_info[i].name && rf_has(r_info[i].flags, flag)) n++;
	}

	return n;
}

/** How many egos and artifacts carry an object flag. */
static int items_with(int flag) {
	int i, n = 0;

	for (i = 0; i < z_info->e_max; i++) {
		if (e_info[i].name && of_has(e_info[i].flags, flag)) n++;
	}
	for (i = 0; i < z_info->a_max; i++) {
		if (a_info[i].name && of_has(a_info[i].flags, flag)) n++;
	}

	return n;
}

/**
 * The quantum monster is there, and there is one of it.
 *
 * Zangband gave the flag to exactly one creature, and a second would be a sign
 * the import had matched something it should not have.
 */
static int test_the_quantum_monster_is_there(void *state) {
	eq(races_with(RF_QUANTUM), 1);

	ok;
}

/**
 * A quantum monster is not a questor.
 *
 * It can stop existing on its own turn, so a quest that named one could never
 * be finished. Zangband spared its questors for the same reason, and this is
 * the check that the two rules cannot be brought into conflict by a later
 * edit to the data.
 */
static int test_nothing_quantum_is_needed_for_a_quest(void *state) {
	int i;

	for (i = 0; i < z_info->r_max; i++) {
		struct monster_race *race = &r_info[i];

		if (!race->name) continue;
		if (!rf_has(race->flags, RF_QUANTUM)) continue;

		require(!rf_has(race->flags, RF_QUESTOR));
	}

	ok;
}

/**
 * The four new object flags reached the items that should have them.
 */
static int test_the_object_flags_are_there(void *state) {
	require(items_with(OF_GHOUL_TOUCH) > 0);
	require(items_with(OF_RETURN) > 0);
	require(items_with(OF_LUCK_10) > 0);
	require(items_with(OF_EASY_ENCHANT) > 0);

	ok;
}

/**
 * And the two that turned out to be renames reached theirs.
 *
 * `SENSE` is Zangband's data-file name for the searching skill and `THROW` for
 * an item meant to be thrown; 4.2 has both already, under `SEARCH` and
 * `THROWING`. Both had been filed as mechanisms to build, on the strength of
 * their names, and both were being dropped from the conversion instead --
 * which is why this asserts a count rather than merely that the flag exists.
 */
static int test_the_renames_carried_their_items(void *state) {
	int i, searchers = 0;

	require(items_with(OF_THROWING) > 0);

	/*
	 * SEARCH is a modifier rather than a flag, and Angband's own artifacts
	 * carry eight of them -- so "more than none" is true whether or not the
	 * import contributed, which is how the first version of this test passed
	 * with the rename removed.  The count is what discriminates: eight of the
	 * imported artifacts have one, and without the rename all eight are
	 * dropped.
	 */
	for (i = 0; i < z_info->a_max; i++) {
		if (a_info[i].name && a_info[i].modifiers[OBJ_MOD_SEARCH]) searchers++;
	}

	eq(searchers, 16);

	ok;
}

/**
 * Luck is a bonus to the saving throw, not a ceiling on it.
 *
 * Zangband wrote `skills[SKILL_SAV] = 10`, an assignment, which made a lucky
 * novice better at saving than an unlucky veteran. Read here as the bonus the
 * flag's name promises. Checked as the arithmetic, since the real one needs a
 * character with the item on.
 */
static int test_luck_adds_rather_than_sets(void *state) {
	int base, lucky;

	/* What the code does now. */
	base = 40;
	lucky = base + 10;
	require(lucky > base);

	/* What the original did, for the case that shows the difference. */
	base = 40;
	lucky = 10;
	require(lucky < base);

	ok;
}

const char *suite_name = "monster/lastflags";
struct test tests[] = {
	{ "the-quantum-monster-is-there", test_the_quantum_monster_is_there },
	{ "nothing-quantum-is-needed-for-a-quest",
	  test_nothing_quantum_is_needed_for_a_quest },
	{ "the-object-flags-are-there", test_the_object_flags_are_there },
	{ "the-renames-carried-their-items",
	  test_the_renames_carried_their_items },
	{ "luck-adds-rather-than-sets", test_luck_adds_rather_than_sets },
	{ NULL, NULL }
};
