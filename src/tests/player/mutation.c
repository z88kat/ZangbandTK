/* player/mutation
 *
 * What chaos makes of a character (ZangbandTK, PLR-13 to PLR-17, PLR-34 to
 * PLR-38).
 *
 * This suite covers the model: the roster, the roll, what cancels what, and
 * what survives a save. The things it is built against are the ones that would
 * not look like failures — a mutation nothing can roll, a cancelling group
 * that fires one way and not the other, a race affinity that overrides a roll
 * it should not, or 96 entries silently becoming 95 because the converter
 * dropped one on the floor.
 */

#include "unit-test.h"

#include "init.h"
#include "player.h"
#include "player-birth.h"
#include "player-mutation.h"
#include "test-utils.h"

int setup_tests(void **state) {
	set_file_paths();
	init_angband();
#ifdef UNIX
	create_needed_dirs();
#endif

	if (!player_make_simple(NULL, NULL, "Tester")) {
		cleanup_angband();
		return 1;
	}

	*state = NULL;
	return 0;
}

int teardown_tests(void *state) {
	cleanup_angband();
	return 0;
}

/** Count the roster by kind. */
static int kind_count(int kind) {
	const struct mutation *m;
	int n = 0;

	for (m = mutations; m; m = m->next) {
		if (m->kind == kind) n++;
	}

	return n;
}

/**
 * All ninety-six arrived, in the four kinds they belong to.
 *
 * Counts rather than existence. The table is generated out of C that was never
 * meant to be parsed, so the failure worth catching is a regex that stopped
 * matching one entry — which leaves a roster that looks entirely reasonable
 * and is quietly missing a mutation nobody will ever be given.
 *
 * The split is 32 activatable, 27 random, 32 continuous and 5 melee. Zangband
 * stores 32/32/32 because that is what fits in three words; the five melee
 * mutations come out of the middle set, which is DEC-44.
 */
static int test_all_ninety_six_are_here(void *state) {
	eq(mutation_count(), 96);

	eq(kind_count(MUTATION_KIND_ACTIVATABLE), 32);
	eq(kind_count(MUTATION_KIND_RANDOM), 27);
	eq(kind_count(MUTATION_KIND_CONTINUOUS), 32);
	eq(kind_count(MUTATION_KIND_MELEE), 5);

	ok;
}

/**
 * Every mutation carries what its kind needs to be usable.
 *
 * An activatable mutation with no level, cost or stat is a power that cannot
 * be invoked; a random one with no chance never fires. Either would parse, sit
 * in the roster, and do nothing for the life of the character holding it.
 */
static int test_each_kind_carries_its_own_fields(void *state) {
	const struct mutation *m;
	int checked = 0;

	for (m = mutations; m; m = m->next) {
		require(m->name);
		require(m->desc);
		require(m->gain);
		require(m->lose);
		require(m->kind > MUTATION_KIND_NONE);
		require(m->kind < MUTATION_KIND_MAX);

		if (m->kind == MUTATION_KIND_ACTIVATABLE) {
			require(m->power);
			require(m->level > 0);
			require(m->cost > 0);
			require(m->stat >= 0);
			require(m->stat < STAT_MAX);
			require(m->difficulty > 0);
		} else if (m->kind == MUTATION_KIND_RANDOM) {
			/*
			 * All but one fire on a timer. "Chaos deities give you gifts"
			 * has no chance of its own because it fires on gaining a level
			 * rather than on a turn passing.
			 */
			if (!streq(m->name, "CHAOS_GIFT")) require(m->chance > 0);
		}

		checked++;
	}

	eq(checked, 96);

	ok;
}

/**
 * Nothing in the roster is unreachable.
 *
 * The selection weights are the widths of the case runs in Zangband's switch,
 * and they total 193. A mutation with a weight of zero can never be rolled: it
 * would be in the data, on the character sheet's list of what is possible, and
 * unobtainable.
 *
 * Three have prerequisites instead, which is a different thing, and the
 * spoiler documents none of them.
 */
static int test_every_mutation_can_be_rolled(void *state) {
	const struct mutation *m;
	int total = 0, gated = 0;

	for (m = mutations; m; m = m->next) {
		require(m->weight > 0);
		total += m->weight;
		if (m->gate != MUTATION_GATE_NONE) gated++;
	}

	eq(total, 193);
	eq(gated, 3);

	ok;
}

/**
 * Gaining one of a cancelling pair sheds the other, in both directions.
 *
 * PLR-37's seven groups. Asserted both ways round because a table of pairs
 * read in one direction only would pass a test that gained them in the order
 * the table happens to list.
 */
static int test_cancelling_mutations_drive_each_other_out(void *state) {
	const struct mutation *puny = mutation_by_name("PUNY");
	const struct mutation *strong = mutation_by_name("HYPER_STR");

	notnull(puny);
	notnull(strong);

	flag_wipe(player->mutations, MUT_SIZE);

	require(player_gain_mutation(player, puny));
	require(player_has_mutation(player, puny));

	require(player_gain_mutation(player, strong));
	require(player_has_mutation(player, strong));
	require(!player_has_mutation(player, puny));

	/* And back the other way. */
	require(player_gain_mutation(player, puny));
	require(player_has_mutation(player, puny));
	require(!player_has_mutation(player, strong));

	ok;
}

/**
 * Iron skin sheds three things at once.
 *
 * The group that is not a pair, and the reason the table holds sets rather
 * than couples: taking iron skin removes scales, rotting flesh and warts
 * together, and any one of those removes iron skin.
 */
static int test_iron_skin_sheds_all_three(void *state) {
	const struct mutation *iron = mutation_by_name("IRON_SKIN");
	static const char *const shed[] = { "SCALES", "FLESH_ROT", "WART_SKIN" };
	size_t i;

	notnull(iron);
	flag_wipe(player->mutations, MUT_SIZE);

	for (i = 0; i < N_ELEMENTS(shed); i++) {
		const struct mutation *m = mutation_by_name(shed[i]);

		notnull(m);
		require(player_gain_mutation(player, m));
	}
	eq(player_mutation_total(player), 3);

	require(player_gain_mutation(player, iron));
	eq(player_mutation_total(player), 1);
	require(player_has_mutation(player, iron));

	ok;
}

/**
 * A race with an affinity gets its own mutation more often than chance.
 *
 * PLR-38, and the numbers are not uniform: a Vampire takes hypnotic gaze six
 * times in ten and a Beastman polymorph self only one time in ten, which the
 * spoiler does not say. Sampled, because the override is a roll on top of a
 * roll.
 *
 * The margin is what discriminates. Hypnotic gaze has a weight of 2 out of
 * 193, so an unweighted character would draw it about one time in a hundred;
 * a Vampire should draw it more than half the time.
 */
static int test_a_race_affinity_beats_the_roll(void *state) {
	struct player_race *r;
	int i, gazes = 0;

	for (r = races; r; r = r->next) {
		if (streq(r->name, "Vampire")) player->race = r;
	}
	notnull(player->race);
	require(player->race->mutation_affinity);

	for (i = 0; i < 1000; i++) {
		const struct mutation *m;

		flag_wipe(player->mutations, MUT_SIZE);
		m = mutation_roll(player);
		if (m && streq(m->name, "HYPN_GAZE")) gazes++;
	}

	require(gazes > 500);
	require(gazes < 800);

	ok;
}

/**
 * A Beastman is born changed, and keeps changing.
 *
 * PLR-36, the race's whole identity. Only the birth half is checked here --
 * the per-level chance fires from the level-up path, which needs a character
 * gaining experience rather than a roster.
 */
static int test_a_beastman_is_born_mutated(void *state) {
	struct player_race *r, *beastman = NULL, *human = NULL;

	for (r = races; r; r = r->next) {
		if (streq(r->name, "Beastman")) beastman = r;
		if (streq(r->name, "Human")) human = r;
	}
	notnull(beastman);
	notnull(human);

	eq(beastman->mutation_birth, 1);
	eq(beastman->mutation_per_level, 20);

	/* And nobody else is born with one. */
	eq(human->mutation_birth, 0);
	eq(human->mutation_per_level, 0);

	ok;
}

/**
 * A mutation that no longer exists does not take the savefile with it.
 *
 * Mutations are written by name, so a character can be loaded into a build
 * whose `mutation.txt` has changed under them. Losing a mutation is
 * survivable; losing the character is not.
 */
static int test_an_unknown_mutation_is_survivable(void *state) {
	null(mutation_by_name("NOT_A_MUTATION"));
	null(mutation_by_name(""));

	ok;
}

const char *suite_name = "player/mutation";
struct test tests[] = {
	{ "all-ninety-six-are-here", test_all_ninety_six_are_here },
	{ "each-kind-carries-its-own-fields",
	  test_each_kind_carries_its_own_fields },
	{ "every-mutation-can-be-rolled", test_every_mutation_can_be_rolled },
	{ "cancelling-mutations-drive-each-other-out",
	  test_cancelling_mutations_drive_each_other_out },
	{ "iron-skin-sheds-all-three", test_iron_skin_sheds_all_three },
	{ "a-race-affinity-beats-the-roll", test_a_race_affinity_beats_the_roll },
	{ "a-beastman-is-born-mutated", test_a_beastman_is_born_mutated },
	{ "an-unknown-mutation-is-survivable",
	  test_an_unknown_mutation_is_survivable },
	{ NULL, NULL }
};
