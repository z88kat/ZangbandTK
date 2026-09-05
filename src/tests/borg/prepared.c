/* borg/prepared
 *
 * The borg has two rules about depth and they have to agree.
 *
 *   borg_prepared(d)  "may I go to depth d"      -- gates descent
 *   borg_restock(d)   "must I leave depth d"     -- sends it to town
 *
 * These are asked in different places and neither knows about the other, so
 * nothing stops them from disagreeing in the one way that matters: prepared
 * refuses to let the borg descend because it lacks some item, and restock
 * does not send it home to buy that item. The borg is then forbidden to go
 * deeper and not required to go anywhere else, so it grinds where it stands
 * until the run ends. It looks like a borg that has decided shallow is
 * better; it is a borg with no legal move.
 *
 * This was not hypothetical. A Warrior granted level 30 and 200,000 gold sat
 * at depth 5 for thirty thousand turns because borg_prepared() wanted one
 * Word of Recall and borg_restock() had its matching rule commented out. The
 * scroll is stocked in the Alchemist and the borg could have bought two
 * hundred of them.
 */
#include "unit-test.h"
#include "test-utils.h"

#include "init.h"
#include "player.h"
#include "player-birth.h"
#include "z-virt.h"

#include "borg/borg.h"
#include "borg/borg-init.h"
#include "borg/borg-prepared.h"
#include "borg/borg-trait.h"
#include "borg/borg.h"

int setup_tests(void **state)
{
	set_file_paths();
	init_angband();

	/*
	 * `borg_prepared()` reads `borg_cfg[]` on its first line and the array
	 * is a null pointer until the borg reads its settings file, so a test
	 * that only sets traits crashes before it asks anything. The settings
	 * themselves come from borg.txt; the defaults are what a real run uses
	 * when that file says nothing, which is the case being tested.
	 */
	/*
	 * The deeper rungs of the ladder ask whether the borg can cast its way
	 * out of trouble, and those checks read the character. A Warrior is the
	 * right one to ask with: it can cast nothing, so every refusal the tests
	 * below see is about carried items rather than about spells.
	 */
	if (!player_make_simple(NULL, "Warrior", "Tester")) {
		printf("failed to make a character\n");
		return 1;
	}

	/* `borg.trait[]` is an allocated array, not a member array */
	if (!borg.trait)
		borg_trait_init();

	if (!borg_cfg) {
		int i;
		borg_cfg = mem_alloc(sizeof(int) * BORG_MAX_SETTINGS);
		for (i = 0; i < BORG_MAX_SETTINGS; i++)
			borg_cfg[i] = borg_settings[i].default_value;
	}

	return 0;
}

int teardown_tests(void *state)
{
	borg_trait_free();
	cleanup_angband();
	return 0;
}

/**
 * A borg that wants for nothing, so each test can take away one thing and
 * know that the thing it took away is what the rules are answering about.
 *
 * The numbers are above every threshold either rule tests up to depth 30,
 * which is the depth the scoped route aims at.
 */
static void a_well_supplied_borg(void)
{
	int i;

	for (i = 0; i < BI_MAX; i++)
		borg.trait[i] = 0;

	borg.trait[BI_CLASS]     = CLASS_WARRIOR;
	borg.trait[BI_LIGHT]     = 3;
	borg.trait[BI_AFUEL]     = 20;
	borg.trait[BI_FOOD]      = 20;
	borg.trait[BI_MAXHP]     = 400;
	borg.trait[BI_CURHP]     = 400;
	borg.trait[BI_CLEVEL]    = 40;
	borg.trait[BI_MAXCLEVEL] = 40;
	borg.trait[BI_MAXDEPTH]  = 30;
	borg.trait[BI_CDEPTH]    = 0;

	borg.trait[BI_ACLW]      = 10;
	borg.trait[BI_ACSW]      = 10;
	borg.trait[BI_ACCW]      = 10;
	borg.trait[BI_APHASE]    = 10;
	borg.trait[BI_ATELEPORT] = 10;
	borg.trait[BI_AESCAPE]   = 10;
	borg.trait[BI_RECALL]    = 5;

	borg.trait[BI_FRACT]     = 1;
	borg.trait[BI_SINV]      = 1;
	/*
	 * Both forms of each resist. The shallow rules count temporary
	 * resistance and the rules from depth 25 down want the permanent kind,
	 * so a fixture carrying only one of them stops being well supplied
	 * partway down the ladder -- which is what the baseline test is for.
	 */
	borg.trait[BI_RFIRE]     = 1;
	borg.trait[BI_RCOLD]     = 1;
	borg.trait[BI_RELEC]     = 1;
	borg.trait[BI_RACID]     = 1;
	borg.trait[BI_SRFIRE]    = 1;
	borg.trait[BI_SRCOLD]    = 1;
	borg.trait[BI_SRELEC]    = 1;
	borg.trait[BI_SRACID]    = 1;
	borg.trait[BI_SRPOIS]    = 1;
	borg.trait[BI_SRCONF]    = 1;

	borg.trait[BI_STR]       = 18;
	borg.trait[BI_INT]       = 18;
	borg.trait[BI_WIS]       = 18;
	borg.trait[BI_DEX]       = 18;
	borg.trait[BI_CON]       = 18;

	borg.ready_morgoth       = 1;

	/*
	 * `borg_must_return_to_town()` ignores the question for the first
	 * hundred turns on a level, so that a borg does not arrive and turn
	 * straight round. The tests are about a borg that has been somewhere a
	 * while, so put the clock past that.
	 */
	borg_began = 0;
	borg_t     = 1000;
}

/**
 * The baseline. If a borg carrying ten of everything cannot walk the ladder
 * to depth 30, then the tests below are measuring the fixture rather than the
 * rules, and every one of them is worthless.
 */
static int test_a_well_supplied_borg_may_reach_depth_thirty(void *state)
{
	int d;

	a_well_supplied_borg();

	for (d = 1; d <= 30; d++) {
		const char *why = borg_prepared(d);
		if (why) {
			printf("  depth %d refused: %s\n", d, why);
			require(!why);
		}
	}

	ok;
}

/**
 * The deadlock itself, stated as the invariant it breaks.
 *
 * At every depth the borg might stand at, if it may not descend then it must
 * have somewhere to go. "Somewhere to go" is either town (restock says so) or
 * deeper into the character (the refusal is about clevel or hit points, which
 * the borg fixes by killing things where it stands).
 *
 * A refusal that is neither -- an item refusal with no matching restock -- is
 * a borg with no legal move.
 */
static int test_a_borg_that_may_not_descend_has_somewhere_to_go(void *state)
{
	static const struct {
		const char *what;
		int trait;
		int value;
	} take_away[] = {
		{ "word of recall", BI_RECALL,    0 },
		{ "teleportation",  BI_ATELEPORT, 0 },
		{ "cure wounds",    BI_ACLW,      0 },
		{ "phase door",     BI_APHASE,    0 },
	};
	size_t t;
	int    stuck = 0;

	for (t = 0; t < N_ELEMENTS(take_away); t++) {
		int d;

		for (d = 1; d < 30; d++) {
			const char *cannot_descend, *must_go_home;

			a_well_supplied_borg();
			borg.trait[take_away[t].trait] = take_away[t].value;

			/* Teleportation has a second source the rules add together */
			if (take_away[t].trait == BI_ATELEPORT)
				borg.trait[BI_AESCAPE] = 0;

			borg.trait[BI_CDEPTH] = d;

			cannot_descend = borg_prepared(d + 1);
			if (!cannot_descend)
				continue;

			/*
			 * The question the borg actually asks itself, rather than
			 * `borg_restock()` underneath it -- a test that asked the
			 * inner rule would pass while the borg still had nowhere
			 * to go.
			 */
			must_go_home = borg_must_return_to_town();
			if (must_go_home)
				continue;

			printf("  stuck at depth %d with no %s: "
				   "cannot descend (%s) and restock says stay\n",
				   d, take_away[t].what, cannot_descend);
			stuck++;
		}
	}

	require(stuck == 0);
	ok;
}

/**
 * The specific case, named, so that a regression says what it broke rather
 * than only that the invariant above went red.
 *
 * Word of Recall is the one that bit, and it is the worst kind: the scroll is
 * always stocked, so the borg is stopped by something it could buy at once.
 */
static int test_no_recall_at_depth_five_sends_the_borg_to_town(void *state)
{
	const char *why;

	a_well_supplied_borg();
	borg.trait[BI_RECALL] = 0;
	borg.trait[BI_CDEPTH] = 5;

	/* It must indeed be blocked, or this test proves nothing */
	require(borg_prepared(6) != NULL);

	why = borg_must_return_to_town();
	require(why != NULL);

	ok;
}

/**
 * And the other direction, which is the failure the fix could plausibly
 * cause: a borg sent home for a scroll it already carries never gets to play.
 */
static int test_a_borg_with_recall_is_left_alone(void *state)
{
	a_well_supplied_borg();
	borg.trait[BI_CDEPTH] = 5;
	require(borg_must_return_to_town() == NULL);

	borg.trait[BI_CDEPTH] = 12;
	require(borg_must_return_to_town() == NULL);

	/* And a borg that has only just arrived is never turned round */
	borg.trait[BI_RECALL] = 0;
	borg_t = 10;
	require(borg_must_return_to_town() == NULL);

	ok;
}

const char *suite_name = "borg/prepared";

struct test tests[] = {
	{ "a well supplied borg may reach depth thirty",
	  test_a_well_supplied_borg_may_reach_depth_thirty },
	{ "a borg that may not descend has somewhere to go",
	  test_a_borg_that_may_not_descend_has_somewhere_to_go },
	{ "no recall at depth five sends the borg to town",
	  test_no_recall_at_depth_five_sends_the_borg_to_town },
	{ "a borg with recall is left alone",
	  test_a_borg_with_recall_is_left_alone },
	{ NULL, NULL }
};
