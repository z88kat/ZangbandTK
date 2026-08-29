/* monster/amberite
 *
 * The scions of Amber (ZangbandTK, CNT-02).
 *
 * Twelve uniques out of Zelazny rather than Tolkien, and the AMBERITE flag is
 * what makes them a family instead of twelve separate entries: it is what the
 * S_AMBERITES summon calls, what the lore names them by, and what the blood
 * curse on a dying one is keyed to.  These check the set is whole, since a
 * missing member is invisible in play -- an Amberite that never gets summoned
 * and never curses anybody just looks like an ordinary unique.
 */

#include "unit-test.h"

#include "init.h"
#include "mon-spell.h"
#include "mon-summon.h"
#include "mon-util.h"
#include "monster.h"
#include "test-utils.h"

int setup_tests(void **state) {
	set_file_paths();
	init_angband();
	*state = NULL;
	return 0;
}

int teardown_tests(void *state) {
	cleanup_angband();
	return 0;
}

/**
 * Twelve, and the twelve the books name.
 *
 * Counted as well as named, so that adding a thirteenth without meaning to --
 * a Chaos lord, say, who is not of Oberon's blood -- fails here rather than
 * quietly widening what the summon can call.
 */
static int test_the_family_is_twelve(void *state) {
	static const char *blood[] = {
		"Oberon, King of Amber", "Benedict, the Ideal Warrior",
		"Corwin, Lord of Avalon", "Eric the Usurper",
		"Caine, the Conspirator", "Gerard, Strongman of Amber",
		"Julian, Master of Arden Forest", "Bleys, Master of Manipulation",
		"Fiona the Sorceress", "Brand, Mad Visionary of Amber",
		"Dworkin Barimen", "Rinaldo, son of Brand"
	};
	int i, found = 0, flagged = 0;

	for (i = 0; i < z_info->r_max; i++) {
		struct monster_race *race = &r_info[i];

		if (!race->name) continue;
		if (rf_has(race->flags, RF_AMBERITE)) flagged++;
	}

	eq(flagged, (int) N_ELEMENTS(blood));

	/* And they are these, by name. */
	for (i = 0; i < (int) N_ELEMENTS(blood); i++) {
		struct monster_race *race = lookup_monster(blood[i]);

		notnull(race);
		require(rf_has(race->flags, RF_AMBERITE));
		found++;
	}

	eq(found, flagged);

	ok;
}

/**
 * Every one of them is a unique, which is why the summon must allow uniques.
 *
 * Worth its own check rather than folding into the summon test: if an Amberite
 * were ever added as an ordinary monster the summon would still work, and the
 * reason this constraint exists would be lost.
 */
static int test_every_amberite_is_a_unique(void *state) {
	int i, seen = 0;

	for (i = 0; i < z_info->r_max; i++) {
		struct monster_race *race = &r_info[i];

		if (!race->name) continue;
		if (!rf_has(race->flags, RF_AMBERITE)) continue;

		require(rf_has(race->flags, RF_UNIQUE));
		seen++;
	}

	require(seen > 0);

	ok;
}

/**
 * The summon calls the blood and nothing else (CNT-02).
 */
static int test_the_summon_calls_only_amberites(void *state) {
	const struct summon *s;
	int idx = summon_name_to_idx("AMBERITE");

	/* The summon exists at all. */
	require(idx >= 0);

	/*
	 * summons is a flat array once parsing has finished -- every entry's next
	 * pointer is nulled -- so it is indexed rather than walked.
	 */
	s = &summons[idx];
	require(streq(s->name, "AMBERITE"));

	/* Restricted by the flag, not by a monster base. */
	eq(s->race_flag, RF_AMBERITE);
	null(s->bases);

	/*
	 * And uniques are allowed.  Every Amberite is one, so a summon that refused
	 * them would be a spell that always summons nobody -- which looks exactly
	 * like a spell the monster never chose to cast.
	 */
	require(s->unique_allowed);

	ok;
}

/**
 * Somebody can actually cast it.
 *
 * A summon type nothing casts is dead data.  Zangband gave S_AMBERITES to seven
 * monsters; three of those live in Angband's own bestiary, which this project
 * keeps byte-identical to upstream, so the four here are the ones that got it.
 */
static int test_somebody_summons_them(void *state) {
	int i, casters = 0;

	for (i = 0; i < z_info->r_max; i++) {
		struct monster_race *race = &r_info[i];

		if (!race->name) continue;
		if (rsf_has(race->spell_flags, RSF_S_AMBERITES)) casters++;
	}

	require(casters > 0);

	/* Oberon calls his own house. */
	require(rsf_has(lookup_monster("Oberon, King of Amber")->spell_flags,
					RSF_S_AMBERITES));

	ok;
}

/**
 * The blood curse is keyed to the flag and to nothing else.
 *
 * Checked as the predicate mon_take_hit() applies, the way the open-country
 * quest test does, since the real one needs a live cave and a dead monster.
 */
static int test_only_amberites_curse_their_killer(void *state) {
	struct monster_race *amberite = lookup_monster("Corwin, Lord of Avalon");
	struct monster_race *other = lookup_monster("Morgoth, Lord of Darkness");

	notnull(amberite);
	notnull(other);

	require(rf_has(amberite->flags, RF_AMBERITE));

	/*
	 * Morgoth is the useful negative: a unique at least as dangerous, killed
	 * at least as deliberately, and no relation.
	 */
	require(!rf_has(other->flags, RF_AMBERITE));

	ok;
}

const char *suite_name = "monster/amberite";
struct test tests[] = {
	{ "the-family-is-twelve", test_the_family_is_twelve },
	{ "every-amberite-is-a-unique", test_every_amberite_is_a_unique },
	{ "the-summon-calls-only-amberites", test_the_summon_calls_only_amberites },
	{ "somebody-summons-them", test_somebody_summons_them },
	{ "only-amberites-curse-their-killer",
	  test_only_amberites_curse_their_killer },
	{ NULL, NULL }
};
