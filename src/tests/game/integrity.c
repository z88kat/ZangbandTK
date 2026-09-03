/* game/integrity
 *
 * The chunk's own bookkeeping is consistent (ZangbandTK).
 *
 * A chunk holds two lists that index into each other and into the map: the
 * object list, where `obj->oidx` must be the slot the chunk filed it under and
 * a held object must name the monster holding it; and the monster list, where
 * `mon->midx` must be its own slot and the map grid must agree about who is
 * standing on it. Nothing checked any of that.
 *
 * 4.2 ships `object_lists_check_integrity()`, which is built on `assert()`.
 * That is right for a debug build and useless as an instrument: it aborts
 * rather than reporting, so a failure arrives as a dead process with no name
 * on it, and it says nothing about monsters at all.
 *
 * This suite exists ahead of PLR-26's pet-carrying, which moves live monsters
 * and their held objects between chunks. The failure mode of getting that
 * wrong is not a crash where the mistake is: it is an object listed in two
 * chunks, or a monster index pointing at a slot that has since been reused,
 * and it surfaces as a corrupt savefile or a duplicated artifact several
 * levels later. An instrument that fails *at* the corruption is worth more
 * than any amount of care taken while writing the thing that might cause it.
 *
 * It earns its place without that, though. These invariants have never been
 * checked, so a defect anywhere in generation, monster placement or object
 * handling could leave the lists inconsistent and nothing would say so.
 */

#include "unit-test.h"
#include "test-utils.h"

#include "cave.h"
#include "game-world.h"
#include "generate.h"
#include "init.h"
#include "mon-make.h"
#include "mon-util.h"
#include "monster.h"
#include "obj-make.h"
#include "obj-util.h"
#include "obj-pile.h"
#include "obj-tval.h"
#include "player-birth.h"
#include "player-util.h"
#include "savefile.h"
#include "z-file.h"

static void println(const char *str) {
	printf("%s\n", str);
}

int setup_tests(void **state) {
	plog_aux = println;
	set_file_paths();
	if (!init_angband()) return 1;
	(void) test_seed_rng_reported(suite_name);
	if (!player_make_simple(NULL, "Warrior", "Tester")) return 1;
	prepare_next_level(player);
	on_new_level();
	*state = NULL;
	return 0;
}

int teardown_tests(void *state) {
	if (cave) wipe_mon_list(cave, player);
	cleanup_angband();
	return 0;
}

/** Check the current level, and say what is wrong if anything is. */
static int check_now(char *why, size_t len) {
	return cave_check_integrity(cave, player->cave, why, len);
}

/**
 * A freshly generated level is consistent.
 *
 * The floor of everything else. If generation itself leaves the lists
 * disagreeing, every later assertion in this suite is measuring that instead
 * of what it means to measure.
 */
static int test_a_new_level_is_consistent(void *state) {
	char why[120];

	eq(check_now(why, sizeof(why)), 0);
	require(!why[0]);

	ok;
}

/**
 * And so is the next one, and the one after.
 *
 * Level transition is where the pet carry will live, so the baseline has to
 * cover several transitions rather than one -- a leak that only shows on the
 * second teardown is exactly the shape of thing this is for.
 */
static int test_level_changes_stay_consistent(void *state) {
	char why[120];
	int i;

	for (i = 0; i < 5; i++) {
		player->depth++;
		prepare_next_level(player);
		on_new_level();

		if (check_now(why, sizeof(why)) != 0) {
			printf("  after transition %d: %s\n", i + 1, why);
		}
		eq(check_now(why, sizeof(why)), 0);
	}

	ok;
}

/**
 * A monster carrying something is consistent both ways.
 *
 * The held-object relationship is the one the carry will have to move: the
 * object names the monster, the monster's pile contains the object, and the
 * chunk has the object listed under the index the object thinks it has. Each
 * of the three can be broken on its own.
 */
static int test_a_monster_holding_something_is_consistent(void *state) {
	struct monster_group_info info = { 0, 0 };
	struct monster_race *race;
	struct monster *mon = NULL;
	struct object *obj;
	char why[120];
	int i;

	race = lookup_monster("cutpurse");
	notnull(race);

	for (i = 0; i < 20 && !mon; i++) {
		struct loc grid;

		if (scatter_ext(cave, &grid, 1, player->grid, 6, true,
						square_isempty) == 0) continue;
		if (!place_new_monster(cave, grid, race, false, false, info,
							   ORIGIN_DROP)) continue;
		mon = square_monster(cave, grid);
	}
	require(mon);

	/* Give it something to hold */
	obj = object_new();
	object_prep(obj, lookup_kind(tval_find_idx("food"),
								 lookup_sval(tval_find_idx("food"), "Apple")),
				0, MINIMISE);
	obj->number = 1;
	obj->held_m_idx = mon->midx;
	pile_insert(&mon->held_obj, obj);
	list_object(cave, obj);

	eq(check_now(why, sizeof(why)), 0);
	require(!why[0]);

	/*
	 * And each of the three is really being checked. Breaking them one at a
	 * time is the only way to know: a checker that skipped the held-object
	 * loop entirely passed this suite until these three were added, because
	 * the object test below breaks a *floor* object and the floor branch
	 * catches that one.
	 */
	{
		int16_t kept_holder = obj->held_m_idx;
		int16_t kept_oidx = obj->oidx;

		/* The object names the wrong monster */
		obj->held_m_idx = mon->midx + 500;
		require(check_now(why, sizeof(why)) > 0);
		obj->held_m_idx = kept_holder;

		/* The chunk has not listed it */
		cave->objects[kept_oidx] = NULL;
		require(check_now(why, sizeof(why)) > 0);
		cave->objects[kept_oidx] = obj;

		/* It is listed under a slot that is not its own */
		obj->oidx = kept_oidx + 1;
		require(check_now(why, sizeof(why)) > 0);
		obj->oidx = kept_oidx;

		eq(check_now(why, sizeof(why)), 0);
	}

	ok;
}

/**
 * The checker notices a monster whose index has been broken.
 *
 * A checker that returns zero for everything passes every test above. Each of
 * these three breaks one invariant by hand, confirms it is reported, and puts
 * it back -- which is the only way to know the instrument works before
 * relying on it.
 */
static int test_the_checker_notices_a_bad_index(void *state) {
	struct monster *mon = NULL;
	char why[120];
	int i, kept;

	for (i = 1; i < cave_monster_max(cave) && !mon; i++) {
		if (cave_monster(cave, i)->race) mon = cave_monster(cave, i);
	}
	require(mon);

	kept = mon->midx;
	mon->midx = kept + 1000;
	require(check_now(why, sizeof(why)) > 0);
	require(why[0]);
	mon->midx = kept;

	eq(check_now(why, sizeof(why)), 0);

	ok;
}

/**
 * And a monster the map disagrees about.
 */
static int test_the_checker_notices_a_bad_grid(void *state) {
	struct monster *mon = NULL;
	char why[120];
	int i, kept;

	for (i = 1; i < cave_monster_max(cave) && !mon; i++) {
		if (cave_monster(cave, i)->race) mon = cave_monster(cave, i);
	}
	require(mon);

	kept = square(cave, mon->grid)->mon;
	square_set_mon(cave, mon->grid, 0);
	require(check_now(why, sizeof(why)) > 0);
	square_set_mon(cave, mon->grid, kept);

	eq(check_now(why, sizeof(why)), 0);

	ok;
}

/**
 * And an object listed under the wrong index.
 *
 * The one that matters most for the carry: `oidx` is a slot in *this* chunk's
 * array, and moving an object between chunks without re-listing it leaves it
 * pointing into the one it came from.
 */
static int test_the_checker_notices_a_bad_object_index(void *state) {
	char why[120];
	int i, kept = 0;
	struct object *obj = NULL;

	for (i = 1; i < cave->obj_max && !obj; i++) {
		if (cave->objects[i]) obj = cave->objects[i];
	}
	require(obj);

	kept = obj->oidx;
	obj->oidx = kept + 1;
	require(check_now(why, sizeof(why)) > 0);
	require(why[0]);
	obj->oidx = kept;

	eq(check_now(why, sizeof(why)), 0);

	ok;
}

/**
 * Saving and loading leaves the lists consistent.
 *
 * The loader rebuilds both lists from scratch and renumbers as it goes
 * (`rd_monsters_aux` closes up behind monsters whose race is gone), so it is
 * the one path that already does what the carry will do. If it drifts, the
 * carry has no model to follow.
 */
static int test_a_round_trip_stays_consistent(void *state) {
	char savename[128];
	char why[120];

	test_savefile_name(savename, sizeof(savename), "Integrity");

	require(savefile_save(savename));

	play_again = true;
	if (cave) wipe_mon_list(cave, player);
	cleanup_angband();
	chunk_list_max = 0;
	init_angband();
	play_again = false;

	require(savefile_load(savename, false));
	file_delete(savename);

	if (check_now(why, sizeof(why)) != 0) printf("  after load: %s\n", why);
	eq(check_now(why, sizeof(why)), 0);

	ok;
}

const char *suite_name = "game/integrity";
struct test tests[] = {
	{ "a-new-level-is-consistent", test_a_new_level_is_consistent },
	{ "a-monster-holding-something-is-consistent",
	  test_a_monster_holding_something_is_consistent },
	{ "the-checker-notices-a-bad-index",
	  test_the_checker_notices_a_bad_index },
	{ "the-checker-notices-a-bad-grid", test_the_checker_notices_a_bad_grid },
	{ "the-checker-notices-a-bad-object-index",
	  test_the_checker_notices_a_bad_object_index },
	{ "level-changes-stay-consistent", test_level_changes_stay_consistent },
	{ "a-round-trip-stays-consistent", test_a_round_trip_stays_consistent },
	{ NULL, NULL }
};
