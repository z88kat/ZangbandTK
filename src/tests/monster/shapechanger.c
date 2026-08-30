/* monster/shapechanger
 *
 * Monsters that will not hold still to be looked at (ZangbandTK, CNT-04).
 *
 * SHAPECHANGER is the one flag in the M2/M3 group that changes nothing about
 * the monster -- not its stats, not its behaviour, only the glyph you see. What
 * makes it worth a test is the dependency nobody would guess: Zangband nested
 * the check inside the multi-hued draw, so a shapechanger that is not also
 * ATTR_MULTI is drawn perfectly normally and the flag does nothing whatever.
 * All five carry ATTR_MULTI, which is exactly why the dependency never showed
 * up as a bug and exactly why it would not show up if a sixth were added
 * without it.
 */

#include "unit-test.h"

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

/**
 * The five, and they are the five the original had.
 */
static int test_the_shapechangers_are_there(void *state) {
	static const char *changers[] = {
		"chaos shapechanger", "lord of chaos", "Dworkin Barimen",
		"unmaker", "Nyarlathotep, the Crawling Chaos"
	};
	int i, found = 0;

	for (i = 0; i < z_info->r_max; i++) {
		struct monster_race *race = &r_info[i];

		if (!race->name) continue;
		if (rf_has(race->flags, RF_SHAPECHANGER)) found++;
	}

	eq(found, (int) N_ELEMENTS(changers));

	for (i = 0; i < (int) N_ELEMENTS(changers); i++) {
		struct monster_race *race = lookup_monster(changers[i]);

		notnull(race);
		require(rf_has(race->flags, RF_SHAPECHANGER));
	}

	ok;
}

/**
 * Every one of them is also multi-hued, or the flag does nothing.
 *
 * The whole point of the test. The draw path only consults SHAPECHANGER inside
 * the branch for monsters that are already changing colour, so a shapechanger
 * without ATTR_MULTI is a flag with no effect -- and it would look exactly like
 * a flag that had been implemented wrongly.
 */
static int test_a_shapechanger_is_also_multi_hued(void *state) {
	int i, checked = 0;

	for (i = 0; i < z_info->r_max; i++) {
		struct monster_race *race = &r_info[i];

		if (!race->name) continue;
		if (!rf_has(race->flags, RF_SHAPECHANGER)) continue;

		require(rf_has(race->flags, RF_ATTR_MULTI));
		checked++;
	}

	require(checked > 0);

	ok;
}

/**
 * It is a display flag and nothing else.
 *
 * Guards against the obvious wrong turn: giving a shapechanger some actual
 * shape-changing behaviour. Zangband's version touches only the glyph, and the
 * flag's type says so -- RFT_DISP, with the other flags that alter how a
 * monster is drawn rather than what it does.
 */
static int test_it_is_only_a_display_flag(void *state) {
	int i;
	bool found = false;

	for (i = 0; i < RF_MAX; i++) {
		if (i != RF_SHAPECHANGER) continue;

		/* Same group as ATTR_MULTI, ATTR_CLEAR and the rest. */
		eq(describe_race_flag(i)[0], '\0');
		found = true;
	}

	require(found);

	ok;
}

/**
 * The monsters that have it are the ones it suits.
 *
 * Not a mechanical requirement, but the reason the flag is worth carrying: the
 * set is chaos and the two people in the books who made or unmade a world.
 * A shapechanging kobold would mean the import had gone wrong somewhere.
 */
static int test_the_set_is_chaos_and_amber(void *state) {
	struct monster_race *dworkin = lookup_monster("Dworkin Barimen");
	struct monster_race *chaos = lookup_monster("chaos shapechanger");

	notnull(dworkin);
	notnull(chaos);

	require(rf_has(dworkin->flags, RF_SHAPECHANGER));
	require(rf_has(chaos->flags, RF_SHAPECHANGER));

	/* Dworkin is of Amber's blood, and the flag does not conflict with it. */
	require(rf_has(dworkin->flags, RF_AMBERITE));

	ok;
}

const char *suite_name = "monster/shapechanger";
struct test tests[] = {
	{ "the-shapechangers-are-there", test_the_shapechangers_are_there },
	{ "a-shapechanger-is-also-multi-hued",
	  test_a_shapechanger_is_also_multi_hued },
	{ "it-is-only-a-display-flag", test_it_is_only_a_display_flag },
	{ "the-set-is-chaos-and-amber", test_the_set_is_chaos_and_amber },
	{ NULL, NULL }
};
