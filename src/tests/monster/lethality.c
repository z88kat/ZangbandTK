/* monster/lethality
 *
 * Tests for the ZangbandTK lethality scalar (BAL-13, BAL-14) — the project's
 * primary balance dial, applied to every monster's hit points and armour class
 * at load time.
 */

#include "unit-test.h"

#include "mon-init.h"

int setup_tests(void **state) {
	*state = NULL;
	return 0;
}

int teardown_tests(void *state) {
	return 0;
}

/* 100% must be an exact no-op, so vanilla lethality stays reachable. */
static int test_identity(void *state) {
	require(mon_scale_lethality(55, 100, 1) == 55);
	require(mon_scale_lethality(48, 100, 0) == 48);
	require(mon_scale_lethality(1, 100, 1) == 1);
	require(mon_scale_lethality(30000, 100, 1) == 30000);
	ok;
}

/* The shipped defaults, checked against the cave orc's 4.2 values. */
static int test_defaults(void *state) {
	/* hit points 55 * 73% = 40 (40.15 truncated) */
	require(mon_scale_lethality(55, 73, 1) == 40);
	/* armour class 48 * 50% = 24 */
	require(mon_scale_lethality(48, 50, 0) == 24);
	ok;
}

/* Hit points floor at 1: a monster with none could not be fought. */
static int test_hp_floor(void *state) {
	require(mon_scale_lethality(1, 73, 1) == 1);
	require(mon_scale_lethality(1, 1, 1) == 1);
	require(mon_scale_lethality(2, 10, 1) == 1);
	ok;
}

/* Armour class floors at 0, which is meaningful where negative is not. */
static int test_ac_floor(void *state) {
	require(mon_scale_lethality(1, 50, 0) == 0);
	require(mon_scale_lethality(3, 10, 0) == 0);
	ok;
}

/* Values that carry meaning the scalar has no business reinterpreting. */
static int test_non_positive_untouched(void *state) {
	require(mon_scale_lethality(0, 73, 1) == 0);
	require(mon_scale_lethality(0, 50, 0) == 0);
	require(mon_scale_lethality(-5, 73, 1) == -5);
	ok;
}

/* Scaling must stay monotonic: a tougher monster never becomes the weaker one. */
static int test_monotonic(void *state) {
	int previous = -1;
	int base;

	for (base = 1; base <= 2000; base++) {
		int scaled = mon_scale_lethality(base, 73, 1);
		require(scaled >= previous);
		previous = scaled;
	}
	ok;
}

/* Guard against overflow at the top of the range 4.2 actually uses. */
static int test_large_values(void *state) {
	/* Morgoth carries the largest hit point total in the game. */
	require(mon_scale_lethality(30000, 73, 1) == 21900);
	require(mon_scale_lethality(30000, 50, 0) == 15000);
	ok;
}

const char *suite_name = "monster/lethality";
struct test tests[] = {
	{ "identity at 100 percent", test_identity },
	{ "shipped defaults", test_defaults },
	{ "hit points floor at 1", test_hp_floor },
	{ "armour class floors at 0", test_ac_floor },
	{ "non-positive values untouched", test_non_positive_untouched },
	{ "scaling is monotonic", test_monotonic },
	{ "large values do not overflow", test_large_values },
	{ NULL, NULL }
};
