/* player/psionic-backfire
 *
 * A failed psionic power may go off inside the caster's head (CNT-10).
 *
 * Zangband's Mindcrafter is the third thing in the game that punishes a bad
 * casting -- `mind.c:515`, beside Chaos's wild magic and Death's miscast -- and
 * the last of the three to be built.  It rolls again at half the failure chance
 * and reads one of five bands off a d100: 4% forget the map, 10% hallucinate,
 * 30% confused, 45% stunned, 11% a mana storm.
 *
 * Two things this pins down.  The bands are checked by count over many rolls
 * rather than by a single draw, for the reason `player/miscast` sets out at
 * length: asserting that a 4% outcome appears at least once in a handful of
 * tries is a test that fails on its own randomness.  And the gate is checked
 * directly, because the failure that would matter is silent -- a racial power
 * is cast through the same function, and a Draconian's breath backfiring would
 * be a regression nothing else in the suite would catch.
 */

#include "unit-test.h"
#include "test-utils.h"

#include "cave.h"
#include "game-world.h"
#include "generate.h"
#include "init.h"
#include "mon-make.h"
#include "player.h"
#include "player-birth.h"
#include "player-calcs.h"
#include "player-timed.h"
#include "player-util.h"
#include "z-util.h"

static void println(const char *str) {
	printf("%s\n", str);
}

int setup_tests(void **state) {
	plog_aux = println;
	set_file_paths();
	if (!init_angband()) return 1;
#ifdef UNIX
	create_needed_dirs();
#endif
	if (!player_make_simple(NULL, "Mindcrafter", "Tester")) {
		cleanup_angband();
		return 1;
	}
	prepare_next_level(player);
	on_new_level();
	(void) test_seed_rng_reported(suite_name);
	return 0;
}

int teardown_tests(void *state) {
	if (cave) wipe_mon_list(cave, player);
	cleanup_angband();
	return 0;
}

/**
 * A character a mana storm cannot kill, with a pool to drain.
 *
 * The level matters more than it looks.  A Mindcrafter gains PROT_CONF at 30
 * (DEC-78), so above that it cannot be confused *by its own backfire* -- the
 * 30% band becomes a no-op for exactly the characters most likely to meet it.
 * That is Zangband's own progression and is pinned by its own case below; the
 * band census therefore runs at 25, where all five outcomes can be observed.
 */
static void healthy(int lev) {
	player->lev = player->max_lev = lev;
	player->upkeep->update |= (PU_BONUS | PU_HP | PU_SPELLS);
	update_stuff(player);
	player->mhp = 5000;
	player->chp = 5000;
	player->csp = player->msp;
	player_clear_timed(player, TMD_CONFUSED, false, false);
	player_clear_timed(player, TMD_STUN, false, false);
	player_clear_timed(player, TMD_IMAGE, false, false);
}

/**
 * The class's own powers backfire and the race's do not.
 *
 * A Mindcrafter carries both kinds at once -- its psionics and whatever its
 * race can do -- and they reach `player_use_power()` through the same door.
 */
static int test_only_class_powers_backfire(void *state) {
	const struct player_power *cls = player->class->powers;
	const struct player_power *rce = player->race->powers;
	int seen = 0;

	notnull(cls);

	for (; cls; cls = cls->next) {
		require(player_power_is_class_power(player, cls));
		seen++;
	}

	/* Zangband gives the Mindcrafter twelve. */
	eq(seen, 12);

	/* Whatever the race brought, none of it is the class's. */
	for (; rce; rce = rce->next) {
		require(!player_power_is_class_power(player, rce));
	}

	ok;
}

/**
 * Every backfire does something, and the five bands turn up at their rates.
 *
 * Counted over two thousand rolls.  The rarest band is 4%, so the expected
 * count is 80 with a standard deviation of 8.8; requiring 30 to 140 is better
 * than five deviations clear at both ends while still failing loudly if a band
 * is unreachable or has swallowed its neighbour.
 */
static int test_the_five_bands_are_reachable(void *state) {
	static const int rolls = 2000;
	int forgot = 0, image = 0, conf = 0, stun = 0, storm = 0;
	int i;

	for (i = 0; i < rolls; i++) {
		int before;

		healthy(25);
		before = player->csp;

		player_mind_backfires(player);

		if (player->timed[TMD_IMAGE]) {
			image++;
		} else if (player->timed[TMD_CONFUSED]) {
			conf++;
		} else if (player->timed[TMD_STUN]) {
			stun++;
		} else if (player->csp < before) {
			/* Only the storm spends mana. */
			storm++;
		} else {
			/* Nothing timed and nothing spent: the map was forgotten. */
			forgot++;
		}
	}

	/* 4%, 10%, 30%, 45%, 11% of 2000 -- generous bounds, see above. */
	require(forgot >= 30 && forgot <= 140);
	require(image >= 130 && image <= 270);
	require(conf >= 480 && conf <= 720);
	require(stun >= 750 && stun <= 1050);
	require(storm >= 150 && storm <= 300);

	/* Every roll landed somewhere. */
	eq(forgot + image + conf + stun + storm, rolls);

	ok;
}

/**
 * The storm drains the pool as well as filling the room.
 *
 * Zangband takes `plev * MAX(1, plev / 10)` mana, which at level 40 is 160, and
 * clamps at zero rather than going negative.
 */
static int test_the_storm_drains_mana(void *state) {
	int i, storms = 0;

	for (i = 0; i < 400 && storms < 1; i++) {
		int before;

		healthy(25);

		/*
		 * Fill from the character's *real* maximum rather than a made-up one.
		 * The backfire runs `update_stuff()` on its way through, which
		 * recomputes the pool and clamps anything above it -- so a test that
		 * sets `csp` to a convenient number measures the clamp instead of the
		 * drain.
		 */
		player->csp = player->msp;
		before = player->csp;

		player_mind_backfires(player);

		/* The storm is the only band that spends mana. */
		if (player->csp < before) {
			storms++;

			/* 25 * max(1, 2) = 50, or the whole pool if it holds less. */
			eq(before - player->csp, MIN(50, before));
		}
	}

	require(storms > 0);

	ok;
}

/**
 * Past level 30 the confusion band does nothing, and that is correct.
 *
 * The Mindcrafter's own PROT_CONF arrives at 30, so the single most common
 * backfire stops reaching it.  Worth a case of its own because it looks like a
 * bug from either side: a band that fires and does nothing, or a table that
 * seems to have lost thirty per cent of itself.
 */
static int test_a_veteran_shrugs_off_the_confusion(void *state) {
	int i, confused = 0;

	for (i = 0; i < 600; i++) {
		healthy(40);

		player_mind_backfires(player);

		if (player->timed[TMD_CONFUSED]) confused++;
	}

	/* Thirty per cent of six hundred rolls, and none of them lands. */
	eq(confused, 0);

	ok;
}

const char *suite_name = "player/psionic-backfire";
struct test tests[] = {
	{ "only-class-powers-backfire", test_only_class_powers_backfire },
	{ "the-five-bands-are-reachable", test_the_five_bands_are_reachable },
	{ "the-storm-drains-mana", test_the_storm_drains_mana },
	{ "a-veteran-shrugs-off-the-confusion",
	  test_a_veteran_shrugs_off_the_confusion },
	{ NULL, NULL }
};
