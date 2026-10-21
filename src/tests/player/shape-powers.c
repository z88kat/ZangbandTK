/* player/shape-powers
 *
 * The shapechanges came back as class powers (PLR-35), and this suite covers
 * the three things that were new about doing it that way rather than the
 * shapechange system itself, which is 4.2's and unaltered.
 *
 * **A class carrying both books and powers.** No class did before this: the
 * Mindcrafter has powers and no `magic:` line, every other caster has books and
 * no powers.  `calc_mana()` is `if (magic.total_spells) ... else if (powers)`,
 * so the Druid, Necromancer and Blackguard must keep taking the first branch
 * now that they have a power list hanging off them.  If they ever took the
 * second they would read `class->power_first` and `class->power_stat`, neither
 * of which those classes set, and the pool would silently become something else.
 * That is the case this suite exists for.
 *
 * **A latent backfire, made reachable.** `player_use_power()` asked
 * `player_power_is_class_power()` before firing `player_mind_backfires()`, and
 * that was an exact test of "is this psionic" for exactly as long as the
 * Mindcrafter was the only class with powers.  Giving the Druid four powers
 * would have made a fumbled Bear Form a psionic accident.  It is now
 * `player_power_is_psionic()`; this asserts the behaviour rather than the
 * predicate, because the predicate is static and because the predicate is what
 * was wrong last time.
 *
 * **One shape at a time.** `do_cmd_cast()` refuses outright while shapechanged,
 * so no shape was ever reachable from inside another; powers are not gated, so
 * the gate had to be written rather than inherited.
 *
 * Every case here has been seen to fail -- see PLR-35 in decisions.md for what
 * each one was broken with.
 */

#include "unit-test.h"
#include "test-utils.h"

#include "effects.h"
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
	if (!player_make_simple(NULL, "Druid", "Tester")) {
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
 * Find one of the character's class powers by name.
 *
 * By name rather than by position, so that adding a fifth power to the Druid
 * does not quietly point a case at a different one.
 */
static struct player_power *class_power(const char *name) {
	struct player_power *it;

	for (it = (struct player_power *) player->class->powers; it;
			it = it->next) {
		if (streq(it->name, name)) return it;
	}

	return NULL;
}

/**
 * A character at `lev` who cannot be killed by a power and has a full pool,
 * back in their own shape and unmarked.
 */
static void healthy(int lev) {
	player->lev = player->max_lev = lev;
	player->upkeep->update |= (PU_BONUS | PU_HP | PU_SPELLS | PU_MANA);
	update_stuff(player);
	player->mhp = 5000;
	player->chp = 5000;
	player->csp = player->msp;
	if (player_is_shapechanged(player)) player_resume_normal_shape(player);
	player_clear_timed(player, TMD_CONFUSED, false, false);
	player_clear_timed(player, TMD_STUN, false, false);
	player_clear_timed(player, TMD_IMAGE, false, false);
}

/**
 * A Druid has both lists, and the mana still comes off the books.
 *
 * Asserted by taking the power list away and putting it back: if `calc_mana()`
 * is reading the books, detaching the powers changes nothing, and if it is ever
 * reading the powers it changes a great deal -- `class->power_first` is zero
 * for a Druid and `class->power_stat` is unset, so the pool would be computed
 * from the wrong level offset and off strength.
 *
 * This is the shape of assertion CONTRIBUTING asks for: not "the Druid has some
 * mana", which is true however calc_mana() branches, but "the presence of the
 * power list does not move it".
 */
static int test_a_caster_keeps_both_lists(void *state) {
	struct player_power *saved;
	int with, without;

	require(player_make_simple(NULL, "Druid", "Tester"));
	prepare_next_level(player);

	/* The combination itself: books and powers on one class. */
	require(player->class->magic.total_spells > 0);
	notnull(player->class->powers);

	healthy(20);
	with = player->msp;

	/* A pool at all, or the rest of this measures nothing. */
	require(with > 0);

	/* Now the same character with nothing hanging off class->powers. */
	saved = (struct player_power *) player->class->powers;
	((struct player_class *) player->class)->powers = NULL;
	healthy(20);
	without = player->msp;
	((struct player_class *) player->class)->powers = saved;

	if (with != without) {
		printf("a Druid's maximum mana at level 20 was %d with its power "
			   "list attached and %d without it; calc_mana() is reading the "
			   "powers branch for a class that has books\n", with, without);
		require(false);
	}

	/* And it is back where it was, for the cases that follow. */
	healthy(20);
	eq(player->msp, with);

	ok;
}

/**
 * The power changes the shape, and takes its price out of the pool.
 *
 * `cheat_powers` rather than a loop: the roll is not what is under test here,
 * and the suite should not depend on winning it.
 */
static int test_a_druid_becomes_a_fox(void *state) {
	struct player_power *fox;
	bool keep;
	int before;

	require(player_make_simple(NULL, "Druid", "Tester"));
	prepare_next_level(player);

	fox = class_power("become a fox");
	notnull(fox);

	/* 4.2's own numbers, carried over rather than re-chosen. */
	eq(fox->level, 1);
	eq(fox->cost, 2);

	keep = player->opts.opt[OPT_cheat_powers];
	player->opts.opt[OPT_cheat_powers] = true;

	healthy(20);
	before = player->csp;

	require(player_use_power(player, fox, 0));
	require(player_is_shapechanged(player));
	require(streq(player->shape->name, "fox"));

	/*
	 * Powers charge randint1(cost - cost/2) + cost/2, so a cost of 2 takes
	 * either 1 or 2 -- never nothing, never more than the number in the data.
	 */
	require(before - player->csp >= 1);
	require(before - player->csp <= fox->cost);

	player->opts.opt[OPT_cheat_powers] = keep;
	healthy(20);

	ok;
}

/**
 * One shape at a time, and the refusal is free.
 *
 * Two halves, and the second is the one that keeps the gate honest: a racial
 * power must still work in fox form.  Blanket-gating powers while shapechanged
 * would have taken the Vampire's drink blood away from a shapechanged Vampire,
 * which is the only food it can reach with the pack closed to it.
 */
static int test_one_shape_at_a_time(void *state) {
	struct player_power *fox, *bear, *racial;
	bool keep;
	int before;

	/* A Half-Orc for the racial half -- "play tough" arrives at level 3. */
	require(player_make_simple("Half-Orc", "Druid", "Tester"));
	prepare_next_level(player);

	fox = class_power("become a fox");
	bear = class_power("become a bear");
	racial = (struct player_power *) player->race->powers;
	notnull(fox);
	notnull(bear);
	notnull(racial);

	keep = player->opts.opt[OPT_cheat_powers];
	player->opts.opt[OPT_cheat_powers] = true;

	healthy(40);
	require(player_use_power(player, fox, 0));
	require(streq(player->shape->name, "fox"));

	/* A second shape is refused, and costs nothing to be refused. */
	before = player->csp;
	require(!player_use_power(player, bear, 0));
	eq(player->csp, before);
	require(streq(player->shape->name, "fox"));

	/* The character's own race is not caught by it. */
	require(player_use_power(player, racial, 0));
	require(streq(player->shape->name, "fox"));

	player->opts.opt[OPT_cheat_powers] = keep;
	healthy(20);

	ok;
}

/**
 * A caster's class power fails quietly.
 *
 * Bear Form at level 40 carries an 85% base failure, so this exercises the
 * failure branch thousands of times rather than hoping to reach it.  Both
 * numbers are asserted: no marks at all, and -- the half that stops this being
 * a test that passes whatever the code does -- enough observed failures that
 * the branch is known to have run.
 *
 * The marks watched are the three bands of `player_mind_backfires()` that leave
 * a timed effect.  A Druid gains none of PROT_CONF, PROT_STUN or free action,
 * so all three can land on one.
 */
static int test_a_casters_power_does_not_backfire(void *state) {
	static const int tries = 3000;
	struct player_power *bear;
	int i, marks = 0, failures = 0;

	require(player_make_simple(NULL, "Druid", "Tester"));
	prepare_next_level(player);

	bear = class_power("become a bear");
	notnull(bear);
	eq(bear->level, 40);

	for (i = 0; i < tries; i++) {
		healthy(40);

		(void) player_use_power(player, bear, 0);

		/*
		 * The power succeeding is what changes the shape, so the shape is
		 * what says whether this one failed.  healthy() puts it back.
		 */
		if (!player_is_shapechanged(player)) failures++;

		if (player->timed[TMD_STUN] || player->timed[TMD_CONFUSED]
				|| player->timed[TMD_IMAGE])
			marks++;
	}

	healthy(20);

	if (marks != 0) {
		printf("over %d uses of a Druid's Bear Form, %d left a psionic mark; "
			   "a class that casts from books is not a Mindcrafter and must "
			   "not reach player_mind_backfires()\n", tries, marks);
		require(false);
	}

	/*
	 * And the branch was actually reached.  At 85% base failure this should be
	 * most of them; a tenth is a floor well clear of the roll, and if it is not
	 * met the zero above proved nothing.
	 */
	if (failures < tries / 10) {
		printf("only %d of %d uses failed, so the backfire branch was barely "
			   "exercised and the count above is not evidence\n",
			   failures, tries);
		require(false);
	}

	ok;
}

const char *suite_name = "player/shape-powers";
struct test tests[] = {
	{ "a-caster-keeps-both-lists", test_a_caster_keeps_both_lists },
	{ "a-druid-becomes-a-fox", test_a_druid_becomes_a_fox },
	{ "one-shape-at-a-time", test_one_shape_at_a_time },
	{ "a-casters-power-does-not-backfire",
	  test_a_casters_power_does_not_backfire },
	{ NULL, NULL }
};
