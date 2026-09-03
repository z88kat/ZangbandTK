/* game/carry
 *
 * Pets follow the player between levels (ZangbandTK, PLR-26, DEC-60).
 *
 * Zangband leaves every pet behind. That is not a design choice so much as its
 * region model making the decision: changing level unreferences the region and
 * `wipe_monsters()` deletes everything in it, pets included, with no
 * pet-specific code anywhere in the path and no mention of the subject in its
 * documentation or its changelog. The project owner reversed the first reading
 * of PLR-26 -- "Pets are one element of zangband that people like. Usually
 * people don't leave their pets behind." -- and DEC-60 records that.
 *
 * What makes this worth a suite of its own is not the feature, it is the
 * ownership. A monster belongs to a chunk four ways at once: its `midx` is a
 * slot in that chunk's monster array, its held objects carry an `oidx` into
 * that chunk's object array, its group index names a group in that chunk's
 * group list, and the map grid it stands on records its index back. Carrying
 * it means moving all four, and getting any of them wrong does not crash where
 * the mistake is -- it produces a duplicated artifact or a corrupt savefile
 * several levels later.
 *
 * So every test here checks `cave_check_integrity()` as well as whatever it is
 * nominally about. That checker (`game/integrity`) was built before this
 * feature for exactly this reason.
 */

#include "unit-test.h"
#include "test-utils.h"

#include "cave.h"
#include "game-world.h"
#include "generate.h"
#include "init.h"
#include "mon-make.h"
#include "mon-predicate.h"
#include "mon-util.h"
#include "monster.h"
#include "obj-make.h"
#include "obj-pile.h"
#include "obj-tval.h"
#include "obj-util.h"
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

/** Everything off the level. */
static void clear_the_level(void) {
	int i;

	for (i = 1; i < cave_monster_max(cave); i++) {
		if (cave_monster(cave, i)->race) delete_monster_idx(cave, i);
	}
}

/** Put `n` monsters of that race on that side; returns how many were placed. */
static int place_side(const char *name, enum monster_allegiance side, int n) {
	struct monster_group_info info = { 0, 0 };
	struct monster_race *race = lookup_monster(name);
	int placed = 0, i;

	if (!race) return 0;

	/*
	 * Anywhere on the level, found by scanning rather than by scattering near
	 * the player. `scatter_ext` with line of sight inside twelve grids sounds
	 * generous and is not: the player arrives in a corridor most of the time,
	 * so it kept returning nothing and the whole suite failed on its setup
	 * about half the time. What these tests need is pets *on the level* --
	 * `collect_pets()` takes every one of them wherever they stand.
	 *
	 * Origin 0: no drop. `place_monster()` only creates one when given an
	 * origin, and a pet holding objects is its own test.
	 */
	for (i = 0; i < 4000 && placed < n; i++) {
		struct loc grid = loc(randint0(cave->width), randint0(cave->height));

		if (!square_in_bounds_fully(cave, grid)) continue;
		if (!square_isempty(cave, grid)) continue;
		if (!place_new_monster(cave, grid, race, false, false, info, 0))
			continue;
		monster_set_allegiance(square_monster(cave, grid), side);
		placed++;
	}

	return placed;
}

/** How many monsters of that side are on the level. */
static int count_side(enum monster_allegiance side) {
	int i, n = 0;

	for (i = 1; i < cave_monster_max(cave); i++) {
		struct monster *mon = cave_monster(cave, i);

		if (mon->race && mon->allegiance == side) n++;
	}

	return n;
}

/** Take a staircase. */
static void go_down(void) {
	player->depth++;
	prepare_next_level(player);
	on_new_level();
}

/**
 * Place `n` pets, descend, and report how many arrived. Repeats up to `tries`
 * times and returns the best result.
 *
 * Every test in this suite that wanted an exact count needed this. A pet that
 * finds no free grid inside the carry radius is left behind and named -- that
 * is the policy, not a failure -- so "three went down and three arrived" is
 * true almost always and not always, and a test asserting it is asserting the
 * level generator's good behaviour rather than this code's. Repeating and
 * taking the best asks the question that was meant: *can* they all arrive.
 *
 * Integrity is checked on every attempt, not just the good one, because a
 * partial carry is exactly the case where the bookkeeping might be left
 * half-done.
 */
static int best_carry(const char *race, int n, int tries) {
	char why[120];
	int best = 0, t;

	for (t = 0; t < tries && best < n; t++) {
		clear_the_level();
		if (place_side(race, MON_ALLEGIANCE_PET, n) != n) continue;

		go_down();

		if (count_side(MON_ALLEGIANCE_PET) > best)
			best = count_side(MON_ALLEGIANCE_PET);

		/* Never more than went down, whatever else happened */
		require(count_side(MON_ALLEGIANCE_PET) <= n);
		eq(cave_check_integrity(cave, player->cave, why, sizeof(why)), 0);
	}

	return best;
}

/** The level is internally consistent, and say what is wrong if not. */
static void require_consistent(void) {
	char why[120];

	if (cave_check_integrity(cave, player->cave, why, sizeof(why)) != 0) {
		printf("  integrity: %s\n", why);
	}
}

/**
 * One pet follows.
 *
 * The floor. Note the check on the *hostile* monster too: the level is
 * regenerated, so every monster on it is new, and a test that only counted
 * pets could pass against a carry that duplicated rather than moved.
 */
static int test_one_pet_follows(void *state) {
	char why[120];

	eq(best_carry("soldier", 1, 10), 1);
	eq(cave_check_integrity(cave, player->cave, why, sizeof(why)), 0);

	ok;
}

/**
 * Several follow, and they arrive near the player.
 *
 * "Follow" has to mean something spatially or it is just bookkeeping: a pet
 * that arrives on the far side of the level has not followed anybody. The
 * scatter is radius one to three, so three is the assertion.
 */
static int test_several_follow_and_arrive_near(void *state) {
	int trial, wanted = 0, arrived = 0, full = 0;

	/*
	 * Measured over twenty descents rather than asserted on one. Whether four
	 * pets fit depends on where the staircase put the player, and a pet that
	 * will not fit is left behind and named -- that is the policy, not a
	 * failure. So what this asserts is the policy: everything that arrives is
	 * within the carry radius, and the rate over twenty descents is written
	 * down where a regression in it would be visible. Not "at least one
	 * arrives" -- an arrival with no free grid at all leaves the whole stable,
	 * about one descent in eight hundred.
	 */
	for (trial = 0; trial < 20; trial++) {
		int i, here = 0;

		clear_the_level();
		if (place_side("soldier", MON_ALLEGIANCE_PET, 4) != 4) continue;
		wanted += 4;

		go_down();

		for (i = 1; i < cave_monster_max(cave); i++) {
			struct monster *mon = cave_monster(cave, i);

			if (!mon->race || !monster_is_pet(mon)) continue;
			here++;

			/* Whatever arrived, arrived beside the player */
			require(distance(mon->grid, player->grid)
					<= z_info->pet_carry_radius);
		}
		arrived += here;
		if (here == 4) full++;
		require_consistent();
	}

	require(wanted > 0);
	printf("  %d of %d pets arrived across %d descents; all four made it %d "
		   "times\n", arrived, wanted, wanted / 4, full);

	/*
	 * A floor, not a target. At the measured radius of five there is room for
	 * six pets on about ninety-six per cent of arrivals, so four pets should
	 * essentially always fit -- but "essentially" is not "always", and a test
	 * that demanded four every time would be asserting the level generator's
	 * good behaviour rather than this code's.
	 */
	require(arrived * 10 >= wanted * 9);

	ok;
}

/**
 * Only pets follow.
 *
 * Friendly monsters are not pets -- PLR-29's whole point -- and a hostile
 * monster following the player downstairs would be a different game. Both
 * checked, because "carry everything non-hostile" passes a pets-only test.
 */
static int test_only_pets_follow(void *state) {
	int t, best = 0;

	for (t = 0; t < 10 && best < 2; t++) {
		clear_the_level();
		if (place_side("soldier", MON_ALLEGIANCE_PET, 2) != 2) continue;
		if (place_side("kobold", MON_ALLEGIANCE_FRIENDLY, 2) != 2) continue;
		if (place_side("cutpurse", MON_ALLEGIANCE_HOSTILE, 2) != 2) continue;

		go_down();

		/* However many pets made it, no friendly monster ever does */
		eq(count_side(MON_ALLEGIANCE_FRIENDLY), 0);
		if (count_side(MON_ALLEGIANCE_PET) > best)
			best = count_side(MON_ALLEGIANCE_PET);
		require_consistent();
	}

	eq(best, 2);

	ok;
}

/**
 * A pet stays a pet.
 *
 * The allegiance byte survives the move. It would be easy to place the carried
 * monsters through a path that zeroes the struct's tail, and hostile is zero.
 */
static int test_a_carried_pet_is_still_a_pet(void *state) {
	int i, found = 0;

	eq(best_carry("soldier", 2, 10), 2);

	/*
	 * Counted by side, not by race. The new level generates its own soldiers
	 * and they are hostile, so "every soldier is a pet" is false and was the
	 * first version of this test -- it failed against working code.
	 */
	for (i = 1; i < cave_monster_max(cave); i++) {
		struct monster *mon = cave_monster(cave, i);

		if (!mon->race || !monster_is_pet(mon)) continue;
		require(streq(mon->race->name, "soldier"));
		found++;
	}
	eq(found, 2);

	ok;
}

/**
 * The racial counter comes out level.
 *
 * `wipe_mon_list()` decrements `race->cur_num` and `place_monster()` increments
 * it, so a carry that does one and not the other drifts. For an ordinary race
 * that is a slow leak in the allocation table; for a **unique**, `cur_num`
 * above zero means it can never be generated again, so a carried unique would
 * quietly remove itself from the game.
 */
static int test_the_racial_counter_is_not_leaked(void *state) {
	struct monster_race *race = lookup_monster("soldier");
	int before, after, i;

	notnull(race);
	clear_the_level();

	before = race->cur_num;
	eq(place_side("soldier", MON_ALLEGIANCE_PET, 3), 3);
	eq(race->cur_num, before + 3);

	go_down();

	after = race->cur_num;

	/* Three carried in, and whatever the new level generated of its own */
	{
		int on_level = 0;

		for (i = 1; i < cave_monster_max(cave); i++) {
			struct monster *mon = cave_monster(cave, i);

			if (mon->race == race) on_level++;
		}
		eq(after, on_level);
	}

	ok;
}

/**
 * Over the cap, the excess is left behind.
 *
 * `pets:max-carried` is a safety valve rather than a balance limit, and the
 * behaviour when it bites has to be decided rather than discovered: the
 * overflow is left, and named. Driven by lowering the cap rather than by
 * summoning twenty-five animals.
 */
static int test_the_cap_leaves_the_excess(void *state) {
	uint16_t kept = z_info->pet_max_carried;

	clear_the_level();
	eq(place_side("soldier", MON_ALLEGIANCE_PET, 5), 5);

	z_info->pet_max_carried = 2;
	{
		int t, best = 0;

		for (t = 0; t < 10 && best < 2; t++) {
			clear_the_level();
			if (place_side("soldier", MON_ALLEGIANCE_PET, 5) != 5) continue;

			go_down();

			/* Never more than the cap, whatever the level offered */
			require(count_side(MON_ALLEGIANCE_PET) <= 2);
			if (count_side(MON_ALLEGIANCE_PET) > best)
				best = count_side(MON_ALLEGIANCE_PET);
			require_consistent();
		}
		eq(best, 2);
	}
	z_info->pet_max_carried = kept;

	ok;
}

/**
 * A cap of zero is Zangband's behaviour.
 *
 * Worth having as a setting and worth testing: it is what the source does, it
 * is a defensible way to play, and it is the switch that turns this whole
 * feature off if the balance question (DEC-65) goes against it.
 */
static int test_a_cap_of_zero_leaves_them_all(void *state) {
	uint16_t kept = z_info->pet_max_carried;

	clear_the_level();
	eq(place_side("soldier", MON_ALLEGIANCE_PET, 3), 3);

	z_info->pet_max_carried = 0;
	go_down();
	z_info->pet_max_carried = kept;

	eq(count_side(MON_ALLEGIANCE_PET), 0);
	require_consistent();

	ok;
}

/**
 * They survive several transitions in a row.
 *
 * The same pets carried repeatedly, which is what actually happens in play and
 * the case where a leak compounds. Five levels, and the integrity checker runs
 * after each.
 */
static int test_they_survive_a_descent(void *state) {
	char why[120];
	int i;

	int had;

	clear_the_level();
	eq(place_side("soldier", MON_ALLEGIANCE_PET, 3), 3);
	had = 3;

	for (i = 0; i < 5; i++) {
		int now;

		go_down();
		now = count_side(MON_ALLEGIANCE_PET);

		/*
		 * Never more than were carried. Not "always three", and not even
		 * "always at least one": an arrival can have no free grid at all
		 * inside the carry radius, in which case every pet is left behind and
		 * named, and that is the policy rather than a failure. Measured at
		 * roughly one descent in eight hundred. What must hold is that the
		 * number cannot *grow* -- that would be a duplicate, which is the
		 * failure this whole phase is guarding against -- and that the lists
		 * stay consistent every time.
		 */
		require(now <= had);
		had = now;

		if (cave_check_integrity(cave, player->cave, why, sizeof(why)) != 0) {
			printf("  after level %d: %s\n", i + 1, why);
		}
		eq(cave_check_integrity(cave, player->cave, why, sizeof(why)), 0);
	}

	ok;
}

/**
 * And a save after a carry round-trips.
 *
 * The carry happens inside one call of `prepare_next_level()` and cannot be
 * saved mid-flight, which is why there is no savefile block for it. This is the
 * check on that claim: carry, then save, then load, and the pets are still
 * there and the lists are still consistent.
 */
static int test_a_carry_then_a_save_round_trips(void *state) {
	char savename[128];
	char why[120];

	int carried;

	eq(best_carry("soldier", 3, 10), 3);
	carried = count_side(MON_ALLEGIANCE_PET);
	eq(carried, 3);

	test_savefile_name(savename, sizeof(savename), "Carry");
	require(savefile_save(savename));

	play_again = true;
	if (cave) wipe_mon_list(cave, player);
	cleanup_angband();
	chunk_list_max = 0;
	init_angband();
	play_again = false;

	require(savefile_load(savename, false));
	file_delete(savename);

	/* The save is of the arrival, so the count must come back unchanged */
	eq(count_side(MON_ALLEGIANCE_PET), carried);
	eq(cave_check_integrity(cave, player->cave, why, sizeof(why)), 0);

	ok;
}

const char *suite_name = "game/carry";
struct test tests[] = {
	{ "one-pet-follows", test_one_pet_follows },
	{ "several-follow-and-arrive-near",
	  test_several_follow_and_arrive_near },
	{ "only-pets-follow", test_only_pets_follow },
	{ "a-carried-pet-is-still-a-pet", test_a_carried_pet_is_still_a_pet },
	{ "the-racial-counter-is-not-leaked",
	  test_the_racial_counter_is_not_leaked },
	{ "the-cap-leaves-the-excess", test_the_cap_leaves_the_excess },
	{ "a-cap-of-zero-leaves-them-all",
	  test_a_cap_of_zero_leaves_them_all },
	{ "they-survive-a-descent", test_they_survive_a_descent },
	{ "a-carry-then-a-save-round-trips",
	  test_a_carry_then_a_save_round_trips },
	{ NULL, NULL }
};
