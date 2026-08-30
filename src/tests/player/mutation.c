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
#include "player-calcs.h"
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

/**
 * What the source says a mutation does, not what the spoiler says.
 *
 * The whole reason Phase 2 reads `mutation_effect()` rather than
 * `spoilers/mutation.txt` is that the spoiler gives the headline and drops the
 * rest. These four are the cases where the difference is largest, and each is
 * asserted on the part the documentation omits as well as the part it gives:
 * a test that only checked "+4 STR" would pass against a build that had
 * quietly lost the intelligence and wisdom that come with it.
 */
static int test_the_source_is_richer_than_the_spoiler(void *state) {
	const struct mutation *m;

	/* "+4 STR", and also -1 INT and -1 WIS. */
	m = mutation_by_name("HYPER_STR");
	notnull(m);
	eq(m->modifiers[STAT_STR], 4);
	eq(m->modifiers[STAT_INT], -1);
	eq(m->modifiers[STAT_WIS], -1);

	/* "-4 STR", and also *+2* DEX -- being puny makes you nimbler. */
	m = mutation_by_name("PUNY");
	notnull(m);
	eq(m->modifiers[STAT_STR], -4);
	eq(m->modifiers[STAT_DEX], 2);

	/* "-4 INT/WIS", and a moron cannot be frightened or confused. */
	m = mutation_by_name("MORONIC");
	notnull(m);
	eq(m->modifiers[STAT_INT], -4);
	eq(m->modifiers[STAT_WIS], -4);
	require(of_has(m->flags, OF_PROT_FEAR));
	require(of_has(m->flags, OF_PROT_CONF));

	/* "+25 AC", and -3 DEX, which the spoiler puts at -1. */
	m = mutation_by_name("IRON_SKIN");
	notnull(m);
	eq(m->armour, 25);
	eq(m->modifiers[STAT_DEX], -3);

	ok;
}

/**
 * The searching bonus survived the change of scale.
 *
 * `calc_bonuses()` multiplies `OBJ_MOD_SEARCH` by five on its way into the
 * skill and leaves `OBJ_MOD_STEALTH` alone. Extra eyes are +15 searching in
 * Zangband, so the modifier has to be 3; taken literally it would have been
 * +75, and nothing about a large searching skill looks wrong on a character
 * sheet. Stealth is asserted beside it because it is the case that must *not*
 * be divided.
 */
static int test_the_searching_bonus_kept_its_size(void *state) {
	const struct mutation *eyes = mutation_by_name("XTRA_EYES");
	const struct mutation *noise = mutation_by_name("XTRA_NOIS");

	notnull(eyes);
	notnull(noise);

	eq(eyes->modifiers[OBJ_MOD_SEARCH], 3);
	eq(noise->modifiers[OBJ_MOD_STEALTH], -3);

	ok;
}

/**
 * Magic resistance grows with the character, and resisting elements costs.
 *
 * The two saving-throw mutations, and they are the only things in the
 * calculation block that are not object properties. Magic resistance is
 * `15 + level/5`, which is why the data file carries a scale as well as a
 * flat amount; the elemental resistance power carries a permanent -10, which
 * is the one continuous effect that belongs to an *activatable* mutation and
 * would be easy to lose by only reading the continuous ones.
 */
static int test_saving_throws_scale_and_cost(void *state) {
	const struct mutation *res = mutation_by_name("MAGIC_RES");
	const struct mutation *elem = mutation_by_name("RESIST");

	notnull(res);
	eq(res->save, 15);
	eq(res->save_scale, 5);

	notnull(elem);
	eq(elem->save, -10);
	eq(elem->save_scale, 0);

	ok;
}

/**
 * A mutation reaches the character's state, and leaves when it does.
 *
 * The end-to-end check: parsed, applied by `calc_bonuses()`, and gone again.
 * Asserted as a difference from the same character without it rather than
 * against an absolute, so the test does not have to know what a Tester's
 * unmutated speed and armour happen to be.
 *
 * Scales rather than iron skin for the armour, deliberately. Iron skin is +25
 * AC and -3 DEX, and the dexterity costs a point of armour back on its way
 * through `adj_dex_ta[]` -- so the honest total is +24 and an assertion of +25
 * fails for a reason that has nothing wrong with it. Scales are +10 and
 * nothing else, which tests the path without testing arithmetic that belongs
 * to a different part of the game.
 */
static int test_a_mutation_reaches_the_character(void *state) {
	const struct mutation *fat = mutation_by_name("XTRA_FAT");
	const struct mutation *scales = mutation_by_name("SCALES");
	int base_speed, base_ac;

	notnull(fat);
	notnull(scales);
	flag_wipe(player->mutations, MUT_SIZE);

	calc_bonuses(player, &player->state, false, true);
	base_speed = player->state.speed;
	base_ac = player->state.to_a;

	require(player_gain_mutation(player, fat));
	require(player_gain_mutation(player, scales));
	calc_bonuses(player, &player->state, false, true);
	eq(player->state.speed, base_speed - 2);
	eq(player->state.to_a, base_ac + 10);

	require(player_lose_mutation(player, fat));
	require(player_lose_mutation(player, scales));
	calc_bonuses(player, &player->state, false, true);
	eq(player->state.speed, base_speed);
	eq(player->state.to_a, base_ac);

	ok;
}

/**
 * Elemental vulnerability is not lost on the way through.
 *
 * Vulnerabilities are the half of the element handling that is easy to get
 * wrong, because `calc_bonuses()` defers them into `vuln[]` and applies them
 * only once every resistance is in. A vulnerability written straight into
 * `el_info` would be overwritten by any resistance found later in the same
 * pass, and the character would simply not be vulnerable -- which looks like
 * nothing at all.
 *
 * All four at once, because this mutation is the only one that has any, and
 * because a loop that applied the first and stopped would still pass on acid.
 */
static int test_a_vulnerability_is_not_lost_on_the_way(void *state) {
	static const int elems[] = { ELEM_ACID, ELEM_ELEC, ELEM_FIRE, ELEM_COLD };
	const struct mutation *vuln = mutation_by_name("VULN_ELEM");
	int base[N_ELEMENTS(elems)];
	size_t i;

	notnull(vuln);
	flag_wipe(player->mutations, MUT_SIZE);

	calc_bonuses(player, &player->state, false, true);
	for (i = 0; i < N_ELEMENTS(elems); i++) {
		base[i] = player->state.el_info[elems[i]].res_level;
	}

	require(player_gain_mutation(player, vuln));
	calc_bonuses(player, &player->state, false, true);
	for (i = 0; i < N_ELEMENTS(elems); i++) {
		eq(player->state.el_info[elems[i]].res_level, base[i] - 1);
	}

	flag_wipe(player->mutations, MUT_SIZE);

	ok;
}

/**
 * A body of fire and a touch of lightning are auras, not resistances.
 *
 * Worth its own test because the natural assumption is the other way round:
 * being made of fire sounds like it should resist cold, and it does not.
 * Zangband gives these two an aura and a point of light and nothing else, and
 * a "helpful" resistance added here would be a mechanic this game invented.
 */
static int test_the_elemental_bodies_are_auras_only(void *state) {
	const struct mutation *fire = mutation_by_name("FIRE_BODY");
	const struct mutation *elec = mutation_by_name("ELEC_TOUC");
	int j;

	notnull(fire);
	notnull(elec);

	require(of_has(fire->flags, OF_SH_FIRE));
	require(of_has(elec->flags, OF_SH_ELEC));

	eq(fire->modifiers[OBJ_MOD_LIGHT], 1);

	for (j = 0; j < ELEM_MAX; j++) {
		eq(fire->el_info[j], 0);
		eq(elec->el_info[j], 0);
	}

	ok;
}

/**
 * Two mutations do nothing, and that is the correct answer.
 *
 * A silly voice and an illusory normal appearance moved charisma and nothing
 * else, and 4.2 removed charisma in 4.2.0. They are still gained, still
 * described, still saved, and have no effect -- which is worth pinning,
 * because "this mutation does nothing" is indistinguishable from "the
 * converter dropped this mutation's effects" unless somebody wrote down which
 * of the two it was.
 */
static int test_the_two_charisma_mutations_are_inert(void *state) {
	static const char *const inert[] = { "SILLY_VOI", "ILL_NORM" };
	size_t i;
	int j;

	for (i = 0; i < N_ELEMENTS(inert); i++) {
		const struct mutation *m = mutation_by_name(inert[i]);

		notnull(m);
		eq(m->armour, 0);
		eq(m->save, 0);
		require(of_is_empty(m->flags));

		for (j = 0; j < OBJ_MOD_MAX; j++) eq(m->modifiers[j], 0);
		for (j = 0; j < ELEM_MAX; j++) eq(m->el_info[j], 0);
	}

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
	{ "the-source-is-richer-than-the-spoiler",
	  test_the_source_is_richer_than_the_spoiler },
	{ "the-searching-bonus-kept-its-size",
	  test_the_searching_bonus_kept_its_size },
	{ "saving-throws-scale-and-cost", test_saving_throws_scale_and_cost },
	{ "a-mutation-reaches-the-character",
	  test_a_mutation_reaches_the_character },
	{ "a-vulnerability-is-not-lost-on-the-way",
	  test_a_vulnerability_is_not_lost_on_the_way },
	{ "the-elemental-bodies-are-auras-only",
	  test_the_elemental_bodies_are_auras_only },
	{ "the-two-charisma-mutations-are-inert",
	  test_the_two_charisma_mutations_are_inert },
	{ NULL, NULL }
};
