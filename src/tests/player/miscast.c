/* player/miscast
 *
 * A miscast Death spell hurts, and how much depends on the book (CNT-10).
 *
 * Zangband punishes a failed Death spell as surely as it lets a failed Chaos
 * one run wild, and on the same roll -- the spell's own place in its realm. The
 * punishment is graded: `(book + 2)d6` hit points out of the first three books,
 * a chance of losing experience from the second half of the realm, and out of
 * the Necronomicon something worse half the time.
 *
 * It was deliberately left out of 3.59.0 with Chaos's backfire table, because
 * it changes the balance of a class whose spell content had not been replaced
 * yet. It arrives with that replacement, and this is the test that says the
 * grading is real rather than nominal -- removing the whole branch was caught
 * by nothing at all before this suite existed.
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
#include "player-spell.h"
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
	if (!player_make_simple(NULL, "Necromancer", "Tester")) {
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

/** A character big enough that a miscast cannot kill it. */
static void healthy(void) {
	player->lev = player->max_lev = 40;
	player->upkeep->update |= (PU_BONUS | PU_HP | PU_SPELLS);
	update_stuff(player);
	player->mhp = 5000;
	player->chp = 5000;
	player->exp = player->max_exp = 1000000;
	player_clear_timed(player, TMD_CONFUSED, false, false);
	player_clear_timed(player, TMD_IMAGE, false, false);
}

/**
 * The damage is `(book + 2)d6`, and the deeper book hurts more.
 *
 * Bounded per roll, and then checked by **mean** rather than by extremes.
 * Asserting that both ends of the range turn up is the obvious thing and is
 * wrong here: the maximum of 4d6 has probability 1/1296, so over four hundred
 * rolls there is a 73% chance of never seeing it. The first version of this
 * test asserted exactly that and failed on its first run, which is the second
 * time a bound in this suite has been drawn without checking the distribution
 * behind it.
 *
 * The mean is the sound statistic. For `n`d6 it is 3.5n with a standard
 * deviation of 1.71*sqrt(n), so over four hundred rolls the *sample* mean has a
 * standard deviation of 0.086*sqrt(n) -- at most 0.18 for the widest book here.
 * Requiring it within 1.0 of 3.5n is therefore about six deviations clear,
 * while a wrong die count is nowhere near: 3d6 in place of 2d6 moves the mean
 * from 7 to 10.5, twenty deviations away.
 */
static int test_the_penalty_is_graded_by_book(void *state) {
	static const int rolls = 400;
	int book, total[3];

	for (book = 0; book < 3; book++) {
		int dice = book + 2, i;

		total[book] = 0;

		for (i = 0; i < rolls; i++) {
			int taken;

			healthy();
			death_miscast(player, 0, book);

			taken = 5000 - player->chp;

			/* Never outside the dice, on any roll. */
			require(taken >= dice);
			require(taken <= dice * 6);
			total[book] += taken;
		}

		/*
		 * Mean within 1.0 of 3.5 * dice, in integers: |2*total - 7*dice*rolls|
		 * must be no more than 2 * rolls.
		 */
		require(abs(2 * total[book] - 7 * dice * rolls) <= 2 * rolls);
	}

	/* And the three books are three different penalties, in order. */
	require(total[0] < total[1]);
	require(total[1] < total[2]);

	ok;
}

/**
 * Experience is at risk from the second half of the realm and not the first.
 *
 * Zangband's gate is `spell > 15`, the realm's midpoint, and a one-in-six roll
 * on top of it. So the first half must *never* cost experience however many
 * times it is cast, and the second half must sometimes -- which is two
 * assertions of different kinds, and the first is the one a careless
 * implementation fails.
 */
static int test_experience_is_only_at_risk_deep(void *state) {
	int i, shallow_losses = 0, deep_losses = 0;

	for (i = 0; i < 600; i++) {
		healthy();
		death_miscast(player, 15, 1);
		if (player->exp < 1000000) shallow_losses++;
	}

	for (i = 0; i < 600; i++) {
		healthy();
		death_miscast(player, 16, 1);
		if (player->exp < 1000000) deep_losses++;
	}

	/* Never below the midpoint. */
	eq(shallow_losses, 0);

	/*
	 * And above it, one time in six. Over 600 rolls the count is around 100
	 * with a standard deviation near 9, so 40 is six deviations clear of the
	 * mean and 250 is well above it -- wide enough not to flake, narrow
	 * enough that a gate stuck open or shut fails.
	 */
	require(deep_losses > 40);
	require(deep_losses < 250);

	ok;
}

/**
 * Hold Life stops the experience loss and nothing else.
 */
static int test_hold_life_keeps_the_experience(void *state) {
	int i, losses = 0;

	for (i = 0; i < 600; i++) {
		healthy();
		player->state.pflags[0] = player->state.pflags[0];
		of_on(player->state.flags, OF_HOLD_LIFE);
		death_miscast(player, 31, 1);
		if (player->exp < 1000000) losses++;
	}

	eq(losses, 0);

	/* The hit points still went, so this is not a spell that did nothing. */
	require(player->chp < 5000);

	of_off(player->state.flags, OF_HOLD_LIFE);

	ok;
}

/**
 * Two realms punish a miscast, and five do not.
 *
 * The wiring rather than the penalty. `death_miscast()` can be perfectly
 * correct and never reached, which is exactly what happened when this was two
 * `streq` calls inside a branch that only runs on a failed roll of a failed
 * cast: deleting the Death arm outright passed every test in the suite.
 *
 * So the decision is a function now, and this walks every spell of every class
 * through it. A realm gaining or losing a backfire fails here, and so does a
 * spell in the wrong realm.
 */
static int test_two_realms_punish_a_miscast(void *state) {
	const struct player_class *c;
	int chaos = 0, death = 0, quiet = 0;

	for (c = classes; c; c = c->next) {
		int b, k;

		for (b = 0; b < c->magic.num_books; b++) {
			const struct class_book *book = &c->magic.books[b];

			for (k = 0; k < book->num_spells; k++) {
				const struct class_spell *sp = &book->spells[k];
				enum spell_backfire kind = spell_backfire_kind(sp);

				notnull(sp->realm);
				if (streq(sp->realm->name, "chaos")) {
					eq(kind, BACKFIRE_CHAOS);
					chaos++;
				} else if (streq(sp->realm->name, "death")) {
					eq(kind, BACKFIRE_DEATH);
					death++;
				} else {
					eq(kind, BACKFIRE_NONE);
					quiet++;
				}
			}
		}
	}

	/*
	 * Chaos: 32 each for the Mage, the Priest and the Chaos-Warrior, and 28
	 * for the Ranger, whose figures put four of them past level 50.
	 */
	eq(chaos, 124);

	/* Death: 32 each for the Necromancer and the Blackguard. */
	eq(death, 64);

	/* And the other five realms say nothing, which is most of the game. */
	require(quiet > 300);

	/* A spell with no realm at all is answered rather than dereferenced. */
	eq(spell_backfire_kind(NULL), BACKFIRE_NONE);

	ok;
}

const char *suite_name = "player/miscast";
struct test tests[] = {
	{ "two-realms-punish-a-miscast", test_two_realms_punish_a_miscast },
	{ "the-penalty-is-graded-by-book", test_the_penalty_is_graded_by_book },
	{ "experience-is-only-at-risk-deep",
	  test_experience_is_only_at_risk_deep },
	{ "hold-life-keeps-the-experience",
	  test_hold_life_keeps_the_experience },
	{ NULL, NULL }
};
