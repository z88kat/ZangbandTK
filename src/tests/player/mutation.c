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
#include "effects.h"
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

/**
 * Every activatable mutation either has a power or has a reason.
 *
 * Twenty-four of the thirty-two are expressible as 4.2 effect chains and eight
 * are not, and the split is asserted as a count so that neither side can drift
 * quietly. A mutation that lost its effect chain to a converter change would
 * still parse, still appear in the power list, and do nothing when invoked --
 * which is exactly what the nine deferred ones look like, so counting is the
 * only way to tell them apart.
 *
 * The nine are named here rather than counted, because "nine are deferred" is
 * a fact about a decision and should fail if somebody implements one without
 * saying so.
 */
static int test_the_activatable_split_is_what_was_decided(void *state) {
	static const char *const deferred[] = {
		"TELEKINES", "SWAP_POS", "DET_CURSE", "MIDAS_TCH", "GROW_MOLD",
		"WEIGH_MAG", "STERILITY", "LAUNCHER"
	};
	const struct mutation *m;
	int with = 0, without = 0;
	size_t i;

	for (m = mutations; m; m = m->next) {
		if (m->kind != MUTATION_KIND_ACTIVATABLE) continue;

		if (m->action) with++; else without++;
	}

	eq(with, 24);
	eq(without, 8);

	for (i = 0; i < N_ELEMENTS(deferred); i++) {
		m = mutation_by_name(deferred[i]);

		notnull(m);
		null(m->action);
	}

	/* And nothing that is not activatable has a power. */
	for (m = mutations; m; m = m->next) {
		if (m->kind == MUTATION_KIND_ACTIVATABLE) continue;

		null(m->action);
	}

	ok;
}

/**
 * A power carries the mutation's own level, cost, stat and failure.
 *
 * The effect chains come from `mutmap.toml` and everything else comes from
 * Zangband's table, so the join between the two is where a power would end up
 * with a plausible chain and a level of zero -- usable from level one, free,
 * and never failing. Checked against three mutations at different ends of the
 * range.
 */
static int test_a_power_is_built_from_its_mutation(void *state) {
	static const struct {
		const char *name;
		int level, cost, stat, fail;
	} expect[] = {
		{ "COLD_TOUCH", 2, 2, STAT_CON, 11 },
		{ "SPIT_ACID", 9, 9, STAT_DEX, 15 },
		{ "BANISH", 25, 25, STAT_WIS, 18 },
	};
	size_t i;

	for (i = 0; i < N_ELEMENTS(expect); i++) {
		const struct mutation *m = mutation_by_name(expect[i].name);

		notnull(m);
		notnull(m->action);
		notnull(m->action->effects);
		eq(m->action->level, expect[i].level);
		eq(m->action->cost, expect[i].cost);
		eq(m->action->stat, expect[i].stat);
		eq(m->action->fail, expect[i].fail);
	}

	ok;
}

/**
 * Spitting acid widens at level thirty, and not before.
 *
 * Zangband's radius is `1 + level / 30`, which 4.2 cannot write as a scaling
 * radius -- a ball's radius is a fixed number on the effect line. The answer
 * is `power-when` bands, which DEC-37 added for exactly this. Asserted as two
 * bands with the boundary in the right place, because a single band would be
 * the obvious simplification and would make a level-9 character spit as wide
 * as a level-50 one.
 */
static int test_a_level_scaled_radius_became_two_bands(void *state) {
	const struct mutation *m = mutation_by_name("SPIT_ACID");
	const struct power_effect *first, *second;

	notnull(m);
	notnull(m->action);

	first = m->action->effects;
	notnull(first);
	eq(first->from, 1);
	eq(first->to, 29);
	notnull(first->effect);
	eq(first->effect->radius, 1);

	second = first->next;
	notnull(second);
	eq(second->from, 30);
	eq(second->to, 0);
	notnull(second->effect);
	eq(second->effect->radius, 2);

	null(second->next);

	ok;
}

/**
 * A mutation's power appears in the list only while the character has it.
 *
 * The list is built fresh from race, class and mutations each time it opens,
 * so the failure worth catching is a power that stays after the mutation that
 * brought it has been shed -- a character who lost their fire breath in a
 * cancelling pair and can still breathe.
 */
static int test_a_power_arrives_and_leaves_with_its_mutation(void *state) {
	const struct mutation *acid = mutation_by_name("SPIT_ACID");
	const struct mutation *m;
	int held;

	notnull(acid);
	flag_wipe(player->mutations, MUT_SIZE);

	held = 0;
	for (m = mutations; m; m = m->next) {
		if (m->action && player_has_mutation(player, m)) held++;
	}
	eq(held, 0);

	require(player_gain_mutation(player, acid));

	held = 0;
	for (m = mutations; m; m = m->next) {
		if (m->action && player_has_mutation(player, m)) held++;
	}
	eq(held, 1);

	require(player_lose_mutation(player, acid));

	held = 0;
	for (m = mutations; m; m = m->next) {
		if (m->action && player_has_mutation(player, m)) held++;
	}
	eq(held, 0);

	ok;
}

/**
 * No mutation still advertises a stat the game does not have.
 *
 * Twelve of the ninety-six descriptions named a charisma change, and 4.2
 * removed the stat in 4.2.0. Editing generated text is worth being uneasy
 * about, so the converter does the least it can -- takes out the charisma term
 * and the punctuation holding it, and nothing else -- and this pins the
 * result. A silly squeak that still reads "(-4 CHR)" is telling the player
 * about an effect that cannot happen.
 */
static int test_no_description_promises_charisma(void *state) {
	const struct mutation *m;

	for (m = mutations; m; m = m->next) {
		notnull(m->desc);
		require(!strstr(m->desc, "CHR"));

		/* And nothing was left with a dangling bracket or a stray comma. */
		require(!strstr(m->desc, "()"));
		require(!strstr(m->desc, " ."));
		require(!strstr(m->desc, ", )"));
	}

	ok;
}

/**
 * The five melee mutations roll what the code rolls, not what the text said.
 *
 * `natural_attack()` sets `dss` and `ddd` and then calls `damroll(ddd, dss)`,
 * whose parameters are `(num, sides)` -- so a scorpion tail rolls 7d3 while its
 * own description reads "3d7". All five are written the wrong way round and
 * all five hit harder in the code: a trunk does a flat 4 rather than 1d4's
 * average of 2.5.
 *
 * Asserted with the sides and the number apart, because a test that only
 * checked the average would pass on either reading of 2d4 and 4d2.
 */
static int test_the_melee_dice_are_the_code_s(void *state) {
	static const struct {
		const char *name;
		int dice, sides, weight;
	} expect[] = {
		{ "SCOR_TAIL", 7, 3, 5 },
		{ "HORNS", 6, 2, 15 },
		{ "BEAK", 4, 2, 5 },
		{ "TRUNK", 4, 1, 35 },
		{ "TENTACLES", 5, 2, 5 },
	};
	size_t i;

	for (i = 0; i < N_ELEMENTS(expect); i++) {
		const struct mutation *m = mutation_by_name(expect[i].name);

		notnull(m);
		eq(m->kind, MUTATION_KIND_MELEE);
		eq(m->blow.dice, expect[i].dice);
		eq(m->blow.sides, expect[i].sides);
		eq(m->blow_weight, expect[i].weight);
		notnull(m->blow_verb);

		/* And the description agrees, because the player only sees that. */
		require(strstr(m->desc, format("%dd%d", expect[i].dice,
									   expect[i].sides)));
	}

	/* Only the tail carries an element. */
	eq(mutation_by_name("SCOR_TAIL")->blow_element, ELEM_POIS);
	eq(mutation_by_name("HORNS")->blow_element, -1);

	ok;
}

/**
 * Every random mutation either fires or has a reason not to.
 *
 * Twenty-two of the twenty-seven are effect chains and five are not. The count
 * is the assertion, because a chain lost to a converter change leaves a
 * mutation that is gained, described, saved, rolled for every single turn, and
 * silently does nothing -- indistinguishable from the six that are meant to.
 */
static int test_the_random_split_is_what_was_decided(void *state) {
	static const char *const deferred[] = {
		"WRAITH", "CHAOS_GIFT", "WARNING", "SP_TO_HP", "HP_TO_SP"
	};
	const struct mutation *m;
	int with = 0, without = 0;
	size_t i;

	for (m = mutations; m; m = m->next) {
		if (m->kind != MUTATION_KIND_RANDOM) continue;

		if (m->fires) with++; else without++;
	}

	eq(with, 22);
	eq(without, 5);

	for (i = 0; i < N_ELEMENTS(deferred); i++) {
		m = mutation_by_name(deferred[i]);

		notnull(m);
		null(m->fires);
	}

	/* Nothing that is not a random mutation fires on a turn passing. */
	for (m = mutations; m; m = m->next) {
		if (m->kind == MUTATION_KIND_RANDOM) continue;

		null(m->fires);
	}

	ok;
}

/**
 * A random mutation with no chance would never fire, and none has one.
 *
 * Zangband stores the rarity in hundredths, so a stored 30 means one turn in
 * three thousand. A mutation whose chance came through as zero would be rolled
 * against `one_in_(0)`; the guard for that is in the firing loop, but the data
 * should not need it. The one exception is the chaos gift, which fires on
 * gaining a level rather than on a turn passing.
 */
static int test_every_firing_mutation_has_a_rarity(void *state) {
	const struct mutation *m;

	for (m = mutations; m; m = m->next) {
		if (!m->fires) continue;

		require(m->chance > 0);

		/* And the rarest of them is still reachable in a long game. */
		require(m->chance <= 12000);
	}

	null(mutation_by_name("CHAOS_GIFT")->fires);
	eq(mutation_by_name("CHAOS_GIFT")->chance, 0);

	ok;
}

/**
 * Every documented acquisition path that 4.2 can carry is wired.
 *
 * `spoilers/mutation.txt` names six ways to gain a mutation and this checks
 * the machinery each one reaches rather than the path itself -- driving a
 * patron reward or a chaos breath from a unit test needs a dungeon, and what
 * would break is not the dungeon.
 *
 * The chaos gift is the one worth spelling out: it has no effect chain of its
 * own and works by carrying the PATRON flag, which `patron_owes_reward()`
 * already reads. A mutation that quietly lost that flag would still be gained,
 * described and saved, and would simply never do the one thing it is for.
 */
static int test_the_acquisition_paths_are_wired(void *state) {
	const struct mutation *gift = mutation_by_name("CHAOS_GIFT");
	const struct mutation *poly = mutation_by_name("POLYMORPH");
	const struct mutation *normal = mutation_by_name("NORMALITY");

	/* The chaos gift makes a Lord take an interest, and nothing else. */
	notnull(gift);
	require(of_has(gift->flags, OF_PATRON));
	null(gift->fires);
	null(gift->action);

	/* Polymorph Self reaches the effect the patron's reward also reaches. */
	notnull(poly);
	notnull(poly->action);
	notnull(poly->action->effects);
	notnull(poly->action->effects->effect);
	eq(poly->action->effects->effect->index, EF_POLY_SELF);

	/* And "strangely normal" is the one that gives one back. */
	notnull(normal);
	notnull(normal->fires);
	notnull(normal->fires->effects);
	notnull(normal->fires->effects->effect);
	eq(normal->fires->effects->effect->index, EF_LOSE_MUTATION);

	ok;
}

/**
 * Shedding a mutation can take the shedder with it.
 *
 * "Strangely normal" removes one of the character's mutations at random and is
 * not excluded from its own draw -- Zangband guards nothing, and the spoiler
 * says so: it removes mutations "including, eventually, itself". The obvious
 * defensive fix would be to skip it, so this pins that it is not skipped.
 *
 * Sampled, because it is a one-in-N draw. With that mutation and one other,
 * it should pick itself about half the time.
 */
static int test_being_normal_can_cure_itself(void *state) {
	const struct mutation *normal = mutation_by_name("NORMALITY");
	const struct mutation *other = mutation_by_name("HALLU");
	int self = 0, i;

	notnull(normal);
	notnull(other);

	for (i = 0; i < 200; i++) {
		flag_wipe(player->mutations, MUT_SIZE);
		require(player_gain_mutation(player, normal));
		require(player_gain_mutation(player, other));

		require(player_lose_random_mutation(player));
		if (!player_has_mutation(player, normal)) self++;
	}
	flag_wipe(player->mutations, MUT_SIZE);

	require(self > 60);
	require(self < 140);

	ok;
}

/**
 * The ceiling on what one character can carry, measured.
 *
 * Eighty-nine: ninety-six less the seven that the cancelling pairs make
 * unreachable together. It is worth pinning because two other things are sized
 * against it -- the Chaos Tower's list and the power list -- and because it is
 * the cancelling table that sets it, and that table has been rewritten once
 * already, when it turned out iron skin sits at the centre of a star rather
 * than in a clique of four.
 *
 * Asserted twice on purpose: once against the roster so the relationship is
 * visible, and once as a literal so that a change to either the roster or the
 * table has to be looked at rather than absorbed.
 */
static int test_the_mutation_ceiling_is_eighty_nine(void *state) {
	const struct mutation *m;
	int held;

	flag_wipe(player->mutations, MUT_SIZE);

	/*
	 * Gained in file order, so each one that drives out an earlier one leaves
	 * the total where it was -- which is the point. This is the most a
	 * character can actually hold, not the size of the roster.
	 */
	for (m = mutations; m; m = m->next) (void) player_gain_mutation(player, m);

	held = player_mutation_total(player);
	eq(held, mutation_count() - 7);
	eq(held, 89);

	/*
	 * Which is more rows than a lettered menu has letters. That is no longer a
	 * hazard -- `menu_dynamic_labels()` bounds the write and rows past the
	 * fifty-first are chosen with the cursor instead -- but it is why the
	 * bound had to exist. See ui/menu-labels.
	 */
	require(held > 51);

	flag_wipe(player->mutations, MUT_SIZE);

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
	{ "the-activatable-split-is-what-was-decided",
	  test_the_activatable_split_is_what_was_decided },
	{ "a-power-is-built-from-its-mutation",
	  test_a_power_is_built_from_its_mutation },
	{ "a-level-scaled-radius-became-two-bands",
	  test_a_level_scaled_radius_became_two_bands },
	{ "a-power-arrives-and-leaves-with-its-mutation",
	  test_a_power_arrives_and_leaves_with_its_mutation },
	{ "no-description-promises-charisma",
	  test_no_description_promises_charisma },
	{ "the-melee-dice-are-the-code-s", test_the_melee_dice_are_the_code_s },
	{ "the-random-split-is-what-was-decided",
	  test_the_random_split_is_what_was_decided },
	{ "every-firing-mutation-has-a-rarity",
	  test_every_firing_mutation_has_a_rarity },
	{ "the-acquisition-paths-are-wired",
	  test_the_acquisition_paths_are_wired },
	{ "being-normal-can-cure-itself", test_being_normal_can_cure_itself },
	{ "the-mutation-ceiling-is-eighty-nine",
	  test_the_mutation_ceiling_is_eighty_nine },
	{ NULL, NULL }
};
