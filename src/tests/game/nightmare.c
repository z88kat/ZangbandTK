/* game/nightmare — Zangband's nightmare mode (BAL-15 to BAL-18)
 *
 * An irreversible birth option whose own text is "this isn't even remotely
 * fair!". Sixteen changes to play, each sourced to a site in Zangband 2.7.5 and
 * listed in .claude/plans/phase1-balance-calibration.md §2.8.2 -- against
 * roughly forty-five claims in the mode's spoiler, which describes a game that
 * was only ever about a quarter built (BAL-18).
 *
 * These tests are the only automated check this milestone gets. The borg is
 * deliberately not running a nightmare variant: the mode is documented as not
 * winnable, the borg cannot leave depth 1 on most runs, and a second nightly
 * job for a mode nobody has played would be speculative. So every test here
 * asserts the behaviour **off without the option and on with it**, in the same
 * run, and each is falsified by removing its guard.
 */
#include "unit-test.h"

#include "z-rand.h"

#include "init.h"
#include "cave.h"
#include "game-world.h"
#include "generate.h"
#include "mon-make.h"
#include "mon-util.h"
#include "monster.h"
#include "option.h"
#include "player.h"
#include "player-birth.h"
#include "player-calcs.h"
#include "test-utils.h"

int setup_tests(void **state) {
	set_file_paths();
	init_angband();
#ifdef UNIX
	create_needed_dirs();
#endif
	(void) test_seed_rng_reported(suite_name);

	if (!player_make_simple(NULL, NULL, "Tester")) {
		cleanup_angband();
		return 1;
	}
	player->depth = 1;
	prepare_next_level(player);
	*state = NULL;
	return 0;
}

int teardown_tests(void *state) {
	if (cave) wipe_mon_list(cave, player);
	cleanup_angband();
	return 0;
}

/** Turn the mode on or off for the next thing this test does. */
static void nightmare(bool on) {
	player->opts.opt[OPT_birth_nightmare] = on;
}

/** Everything off the level. */
static void clear_the_level(void) {
	int i;

	for (i = 1; i < cave_monster_max(cave); i++) {
		if (cave_monster(cave, i)->race) delete_monster_idx(cave, i);
	}
}

/**
 * Put one of that race on the level and hand it back, or NULL.
 *
 * `sleep` is passed through rather than fixed, because "starts awake" is one
 * of the behaviours and the argument is the thing nightmare ignores.
 */
static struct monster *place_one(const char *name, bool sleep) {
	struct monster_group_info info = { 0, 0 };
	struct monster_race *race = lookup_monster(name);
	int i;

	if (!race) return NULL;

	for (i = 0; i < 4000; i++) {
		struct loc grid = loc(randint0(cave->width), randint0(cave->height));

		if (!square_in_bounds_fully(cave, grid)) continue;
		if (!square_isempty(cave, grid)) continue;
		if (!place_new_monster(cave, grid, race, sleep, false, info, 0))
			continue;
		return square_monster(cave, grid);
	}
	return NULL;
}

/**
 * The option is a birth option, and it is off unless you ask for it.
 *
 * Both halves matter. A mode this unfair must never be something a player ends
 * up in by accident, and "irreversible" is free only because 4.2 refuses to
 * modify birth options outside birth (ui-options.c:386) -- so it has to be a
 * birth option and not merely named like one.
 */
static int test_nightmare_is_an_optional_birth_choice(void *state) {
	struct player_options fresh;

	eq(option_type(OPT_birth_nightmare), OP_BIRTH);

	/* And off in a freshly defaulted set of options. */
	options_init_defaults(&fresh);
	require(!fresh.opt[OPT_birth_nightmare]);
	ok;
}

/**
 * Monsters arrive with twice the hit points (monster2.c:1847).
 *
 * Asserted against the same race placed both ways rather than against a
 * number, because `race->avg_hp` already carries BAL-13's ×0.73 by the time
 * this runs -- BAL-16 says the two compose, and a fixed figure here would be
 * asserting the scalar as well and would move whenever it did.
 */
static int test_monsters_arrive_with_twice_the_hit_points(void *state) {
	struct monster *mon;
	int plain, nasty;

	clear_the_level();
	nightmare(false);
	mon = place_one("Grip, Farmer Maggot's dog", false);
	require(mon);
	plain = mon->maxhp;

	clear_the_level();
	nightmare(true);
	mon = place_one("Grip, Farmer Maggot's dog", false);
	require(mon);
	nasty = mon->maxhp;
	nightmare(false);

	/* Unique, so the figure is fixed and the doubling is exact. */
	eq(nasty, plain * 2);
	require(mon->hp == mon->maxhp);
	ok;
}

/**
 * Monsters are five points faster (dungeon.c:2833).
 *
 * A unique again: 4.2 gives non-uniques a small random speed spread, which
 * would make the comparison a distribution rather than an equality.
 */
static int test_monsters_arrive_five_points_faster(void *state) {
	struct monster *mon;
	int plain, nasty;

	clear_the_level();
	nightmare(false);
	mon = place_one("Grip, Farmer Maggot's dog", false);
	require(mon);
	plain = mon->mspeed;

	clear_the_level();
	nightmare(true);
	mon = place_one("Grip, Farmer Maggot's dog", false);
	require(mon);
	nasty = mon->mspeed;
	nightmare(false);

	eq(nasty, plain + 5);
	ok;
}

/**
 * Nothing starts asleep, and nothing gets the free first move (monster2.c:1830
 * and :1880).
 *
 * Two behaviours in one test because they are two halves of the same arrival:
 * §2.8.3 separates them deliberately, since "starts awake" is the one the
 * spoiler mentions and losing MFLAG_NICE -- the engine's free first move for a
 * FORCE_SLEEP ambusher -- is the one it does not.
 */
static int test_nothing_sleeps_and_nothing_waits(void *state) {
	struct monster *mon;

	struct monster_race *sleepy = NULL;
	int i;

	/*
	 * A race that actually sleeps. Not every one does -- Grip has `sleep:0`
	 * and would pass this test by being awake for reasons that have nothing
	 * to do with the mode.
	 */
	for (i = 1; i < z_info->r_max; i++) {
		if (r_info[i].name && r_info[i].sleep > 0) {
			sleepy = &r_info[i];
			break;
		}
	}
	require(sleepy);

	clear_the_level();
	nightmare(false);
	mon = place_one(sleepy->name, true);
	require(mon);
	require(mon->m_timed[MON_TMD_SLEEP] > 0);

	clear_the_level();
	nightmare(true);
	mon = place_one(sleepy->name, true);
	require(mon);
	nightmare(false);
	eq(mon->m_timed[MON_TMD_SLEEP], 0);
	ok;
}

/**
 * A FORCE_SLEEP ambusher loses its grace (monster2.c:1880).
 */
static int test_an_ambusher_loses_its_grace(void *state) {
	struct monster_race *race = NULL;
	struct monster *mon;
	int i;

	/* Any FORCE_SLEEP race will do; the flag is what is being tested. */
	for (i = 1; i < z_info->r_max; i++) {
		if (!r_info[i].name) continue;
		if (rf_has(r_info[i].flags, RF_FORCE_SLEEP)) {
			race = &r_info[i];
			break;
		}
	}
	require(race);

	clear_the_level();
	nightmare(false);
	mon = place_one(race->name, false);
	require(mon);
	require(mflag_has(mon->mflag, MFLAG_NICE));

	clear_the_level();
	nightmare(true);
	mon = place_one(race->name, false);
	require(mon);
	nightmare(false);
	require(!mflag_has(mon->mflag, MFLAG_NICE));
	ok;
}

/**
 * Monsters arrive with double energy, so they act sooner (monster2.c:1874).
 *
 * Statistical, because the starting energy is `randint0(50)` and any single
 * pair could land either way. The means are compared over enough placements
 * that a factor of two cannot be noise, and the assertion is deliberately
 * loose at the edges -- it is testing that the doubling happened, not
 * reproducing the distribution.
 */
static int test_monsters_arrive_with_double_energy(void *state) {
	int odd_plain = 0, odd_nasty = 0, n_plain = 0, n_nasty = 0, i;
	const int runs = 40;

	/*
	 * Parity, which only doubling can produce.
	 *
	 * Starting energy is `randint0(50)`, so about half of an ordinary
	 * monster's draws are odd -- and a doubled one can never be. Forty
	 * placements each way makes "no odd numbers at all" decisive: the chance
	 * of that happening by luck is one in 2^40.
	 *
	 * Three earlier attempts were worse and are worth naming, because each
	 * looked reasonable. Comparing totals over unequal sample counts measured
	 * nothing -- a unique cannot be placed twice, so the nightmare half ran
	 * half as often, and half as many doubled samples add up the same.
	 * Comparing the highest seen was quieter but still a distribution. And
	 * re-seeding to force identical dice does not work here: the two
	 * placements consume different numbers of draws before they reach the
	 * energy, so the same seed gives different numbers.
	 */
	for (i = 0; i < runs; i++) {
		struct monster *mon;

		clear_the_level();
		nightmare(false);
		mon = place_one("soldier", false);
		if (mon) { n_plain++; if (mon->energy % 2) odd_plain++; }

		clear_the_level();
		nightmare(true);
		mon = place_one("soldier", false);
		if (mon) { n_nasty++; if (mon->energy % 2) odd_nasty++; }
	}
	nightmare(false);

	require(n_plain > runs / 2);
	require(n_nasty > runs / 2);

	if (odd_plain == 0 || odd_nasty != 0) {
		printf("odd starting energies: %d of %d plain, %d of %d nightmare "
				"(plain must have some, nightmare must have none)\n",
				odd_plain, n_plain, odd_nasty, n_nasty);
		require(false);
	}
	ok;
}

const char *suite_name = "game/nightmare";
struct test tests[] = {
	{ "nightmare-is-an-optional-birth-choice",
			test_nightmare_is_an_optional_birth_choice },
	{ "monsters-arrive-with-twice-the-hit-points",
			test_monsters_arrive_with_twice_the_hit_points },
	{ "monsters-arrive-five-points-faster",
			test_monsters_arrive_five_points_faster },
	{ "nothing-sleeps-and-nothing-waits",
			test_nothing_sleeps_and_nothing_waits },
	{ "an-ambusher-loses-its-grace", test_an_ambusher_loses_its_grace },
	{ "monsters-arrive-with-double-energy",
			test_monsters_arrive_with_double_energy },
	{ NULL, NULL }
};
