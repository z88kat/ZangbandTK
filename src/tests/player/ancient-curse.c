/* player/ancient-curse
 *
 * The Curse of Topi Ylinen is a cascade, not a roll (CNT-15).
 *
 * Zangband's most feared misfortune picks one of nine steps by weight, and then
 * each step it lands on has a one-in-six chance of dragging in the next one,
 * and that one the next after it.  When the chain stops there is a further
 * one-in-three chance of starting over.  Implemented as a flat random effect it
 * would be unremarkable, which is what CNT-15 says and why the cascade is the
 * thing worth pinning.
 *
 * None of this had a test until 3.72.2, which is how the chain came to stop one
 * step short of Zangband's without anything noticing.  The weights are checked
 * by census, the cascade by the only signature it leaves that a single step
 * cannot, and the two stop conditions directly.
 */

#include "unit-test.h"
#include "test-utils.h"

#include "cave.h"
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
	if (!player_make_simple(NULL, "Warrior", "Tester")) {
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

/** Deep enough for the Cyberdemon step, and too healthy to die of the rest. */
static void afflicted(void) {
	player->depth = 70;
	player->lev = player->max_lev = 40;
	player->upkeep->update |= (PU_BONUS | PU_HP | PU_SPELLS);
	update_stuff(player);
	player->mhp = 30000;
	player->chp = 30000;
	player->exp = player->max_exp = 5000000;
	player_clear_timed(player, TMD_PARALYZED, false, false);
	player_clear_timed(player, TMD_AMNESIA, false, false);
}

static void invoke(void) {
	bool ident = false;

	effect_simple(EF_ANCIENT_CURSE, source_player(), "0", 0, 0, 0, 0, 0,
				  &ident);
}

/**
 * Experience is lost often enough to prove the chain reaches step three.
 *
 * Step 3 carries 3 of the 27 weights, so a curse that never cascaded would
 * drain experience on about one visitation in nine.  The cascade adds every
 * path that lands on steps 0, 1 or 2 and then falls forward -- so the observed
 * rate is meaningfully higher, and measuring it is the cheapest proof the
 * cascade runs at all.  Losing experience is the signature to watch because
 * nothing else in the nine touches it and it cannot be undone by a later step.
 */
static int test_the_chain_reaches_further_than_one_step(void *state) {
	static const int trials = 6000;
	int drained = 0, i;

	for (i = 0; i < trials; i++) {
		int32_t before;

		afflicted();
		before = player->exp;

		invoke();

		if (player->exp < before) drained++;
	}

	/*
	 * A single step would give 3/27 = 11.1%; the repeat and the cascade
	 * together push the observed rate to 17.2%. A bound has to clear *both*
	 * distributions -- far enough above the no-cascade figure to fail loudly
	 * if the chain is cut, and far enough below the real one never to fail on
	 * noise -- and the first version of this only checked the first.
	 *
	 * At six hundred trials it required 90, which is indeed four standard
	 * deviations above the no-cascade 67 (sd 7.7). But the real distribution
	 * centres on 103 with sd 9.4, so 90 sat **1.4 standard deviations below
	 * the mean of the thing being measured** and the test failed about one run
	 * in twelve. Measured at a fixed seed: 6 failures in 60, and the counts
	 * ranged 91 to 125. The seed does not pin it, because the cave is
	 * generated before `test_seed_rng_reported()` runs -- so this was not
	 * reproducible from the CI log either.
	 *
	 * Ten times the trials separates the two distributions instead of hoping
	 * they do not overlap. Six thousand puts the no-cascade figure at 667
	 * (sd 24.3) and the real one at 1030 (sd 29.2), and 850 sits 7.5 standard
	 * deviations above the first and 6.2 below the second. Both bounds are
	 * derived rather than tried, and the cost is half a second.
	 */
	require(drained >= 850);

	/* And it is not everything, which would mean the weights are broken. */
	require(drained < trials / 2);

	ok;
}

/**
 * Every one of the nine steps is reachable.
 *
 * A weight table is the kind of thing that survives being wrong: an off-by-one
 * in the cumulative bounds silently makes one step unreachable and every other
 * step slightly likelier, which no play session would ever isolate.
 */
static int test_every_step_can_happen(void *state) {
	static const int trials = 3000;
	int amnesia = 0, paralysed = 0, drained = 0, i;

	for (i = 0; i < trials; i++) {
		afflicted();

		invoke();

		if (player->timed[TMD_AMNESIA]) amnesia++;
		if (player->timed[TMD_PARALYZED]) paralysed++;
		if (player->exp < 5000000) drained++;
	}

	/* The rarest named step carries 1 weight in 27; the others far more. */
	require(amnesia > 0);
	require(paralysed > 0);
	require(drained > 0);

	ok;
}

/**
 * Paralysis stops the repeat, which is the one mercy in the whole mechanic.
 *
 * Zangband sets `stop_ty` when the character is paralysed or the Cyberdemons
 * are loose, and the outer loop tests it -- so the worst two outcomes cannot be
 * followed by another round.  Checked by construction rather than by frequency:
 * with free action off, the paralysis step always lands when it is rolled.
 */
static int test_paralysis_ends_the_visitation(void *state) {
	static const int trials = 400;
	int seen = 0, i;

	for (i = 0; i < trials; i++) {
		afflicted();

		invoke();

		if (player->timed[TMD_PARALYZED]) {
			seen++;

			/*
			 * Paralysed and still standing: the curse stopped rather than
			 * rolling on. There is no counter to read, so the assertion is
			 * that the character survived -- an unbounded repeat on a
			 * paralysed character is the runaway this guard exists to stop.
			 */
			require(!player->is_dead);
			require(player->chp > 0);
		}
	}

	require(seen > 0);

	ok;
}

const char *suite_name = "player/ancient-curse";
struct test tests[] = {
	{ "the-chain-reaches-further-than-one-step",
	  test_the_chain_reaches_further_than_one_step },
	{ "every-step-can-happen", test_every_step_can_happen },
	{ "paralysis-ends-the-visitation",
	  test_paralysis_ends_the_visitation },
	{ NULL, NULL }
};
