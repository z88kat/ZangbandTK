/* object/randpower
 *
 * What a `RAND_POWER` ego is allowed to roll (ZangbandTK, DEC-109).
 *
 * `ego_apply_magic()` builds a mask from `OFT_PROT` and `OFT_MISC` and picks
 * one flag out of it. `OFT_MISC` is documented as "a good property, suitable
 * for ego items", so membership of that subtype *is* membership of this loot
 * table -- which means a flag added to the game for some other reason joins
 * the table by doing nothing at all. `OF_PASS_WALL` did exactly that.
 *
 * Measured rather than asserted once: the pool has twenty-one members, so one
 * draw proves nothing and a thousand draws make the absence of one of them
 * meaningful. The test also requires the pool to still be *working* -- a build
 * that rolled nothing at all would satisfy "never pass wall" and be far worse.
 */
#include "unit-test.h"
#include "test-utils.h"

#include "init.h"
#include "object.h"
#include "obj-make.h"
#include "obj-properties.h"
#include "obj-tval.h"
#include "obj-pile.h"
#include "obj-util.h"

int setup_tests(void **state) {
	set_file_paths();
	if (!init_angband()) return 1;
	*state = NULL;
	return 0;
}

int teardown_tests(void *state) {
	cleanup_angband();
	return 0;
}

/** The ego of that name, or NULL. */
static struct ego_item *ego_named(const char *name)
{
	int i;

	for (i = 0; i < z_info->e_max; i++) {
		if (e_info[i].name && streq(e_info[i].name, name)) return &e_info[i];
	}

	return NULL;
}

/**
 * Roll `tries` of a `RAND_POWER` ego and count what came out.
 *
 * `want` is counted separately from the total so the caller can say both "this
 * never appeared" and "something always did", which are the two halves that
 * make the first meaningful.
 */
static bool roll(int tries, int want, int *saw_want, int *saw_anything)
{
	struct ego_item *ego = ego_named("(Blessed)");
	struct object_kind *kind = lookup_kind(TV_SWORD,
			lookup_sval(TV_SWORD, "Main Gauche"));
	bitflag pool[OF_SIZE];
	int i;

	*saw_want = 0;
	*saw_anything = 0;

	/* Returned rather than asserted: `require()` returns from its caller. */
	if (!ego || !kind) return false;

	/* The pool as `ego_apply_magic()` builds it, to recognise a hit. */
	create_obj_flag_mask(pool, false, OFT_PROT, OFT_MISC, OFT_MAX);

	for (i = 0; i < tries; i++) {
		struct object *obj = object_new();
		int f;

		object_prep(obj, kind, 40, RANDOMISE);
		obj->ego = ego;
		ego_apply_magic(obj, 40);

		if (of_has(obj->flags, want)) (*saw_want)++;

		for (f = 1; f < OF_MAX; f++) {
			if (!of_has(pool, f)) continue;
			if (!of_has(obj->flags, f)) continue;
			(*saw_anything)++;
			break;
		}

		object_delete(NULL, NULL, &obj);
	}

	return true;
}

/**
 * A random ego never grants permanent wall-walking.
 *
 * Thirty points of power against a pool whose next largest is telepathy, and
 * it removes the dungeon as an obstacle. Nothing was built on it: `PASS_WALL`
 * appears on no artifact, ego or object in the data files, so this pool was
 * the only way a player could ever have got it on an item.
 */
static int test_a_random_ego_cannot_grant_pass_wall(void *state) {
	int saw_pass_wall, saw_anything;

	require(roll(1000, OF_PASS_WALL, &saw_pass_wall, &saw_anything));

	/* The pool still works -- otherwise "never pass wall" means nothing. */
	if (saw_anything < 900) {
		printf("  only %d of 1000 rolls granted anything at all; the pool is "
			   "broken rather than filtered\n", saw_anything);
		require(false);
	}

	if (saw_pass_wall) {
		printf("  %d of 1000 random egos could walk through walls\n",
			   saw_pass_wall);
		require(false);
	}

	ok;
}

/**
 * And the pool is otherwise untouched.
 *
 * The narrow fix removes one flag at the point of the draw rather than
 * reclassifying it, precisely so that nothing else moves. Telepathy is the
 * check: it is in the same subtype, it is the next most valuable thing in the
 * pool, and if the exclusion were written as a subtype change it would be at
 * risk along with everything else in `OFT_MISC`.
 */
static int test_the_rest_of_the_pool_still_rolls(void *state) {
	int saw_telepathy, saw_anything;

	require(roll(1000, OF_TELEPATHY, &saw_telepathy, &saw_anything));

	if (!saw_telepathy) {
		printf("  telepathy never came up in 1000 rolls; the exclusion is "
			   "wider than one flag\n");
		require(false);
	}

	ok;
}

const char *suite_name = "object/randpower";
struct test tests[] = {
	{ "a-random-ego-cannot-grant-pass-wall",
	  test_a_random_ego_cannot_grant_pass_wall },
	{ "the-rest-of-the-pool-still-rolls",
	  test_the_rest_of_the_pool_still_rolls },
	{ NULL, NULL }
};
