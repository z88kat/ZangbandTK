/* monster/allegiance
 *
 * Whose side a monster is on (ZangbandTK, PLR-22, PLR-29).
 *
 * Angband 4.2 has no such notion. Every monster is an enemy by construction,
 * and a search of the tree for "friendly" before this returned two hits, both
 * of them prose. So this is not a feature sitting on top of the monster
 * subsystem; it is a change to an invariant the subsystem was built on, and
 * P-4 in the requirements scopes it that way.
 *
 * Three states rather than a boolean, because PLR-29 says so in terms: a
 * friendly monster will not attack you, cannot be commanded and is free, while
 * a pet can be commanded and costs mana upkeep. Collapse them and one of the
 * two becomes unreachable.
 *
 * Zangband carried this as two bits inside `m_ptr->smart`, the smart-learn
 * bitfield -- `SM_PET` and `SM_FRIENDLY`, both marked XXX -- with hostile
 * derived as neither. `set_pet()` never cleared `SM_FRIENDLY` and
 * `set_friendly()` never cleared `SM_PET`, so a monster could hold both and
 * behave as whichever predicate was tested first. The enum here makes that
 * state unrepresentable, and one of these tests is the reason why.
 *
 * `monsters_are_enemies()` is the other half. Two rules, and the alignment one
 * is easy to get wrong by assuming it only applies across sides: in Zangband a
 * good monster and an evil one fight whether they are both hostile, both pets,
 * or one of each.
 */

#include "unit-test.h"

#include "cave.h"
#include "init.h"
#include "mon-predicate.h"
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

/** A race carrying exactly the alignment flags asked for. */
static struct monster_race *race_with(bool good, bool evil) {
	int i;

	for (i = 1; i < z_info->r_max; i++) {
		struct monster_race *race = &r_info[i];

		if (!race->name) continue;
		if (rf_has(race->flags, RF_GOOD) != good) continue;
		if (rf_has(race->flags, RF_EVIL) != evil) continue;
		return race;
	}

	return NULL;
}

/** A monster of that race, on that side, on the stack. */
static struct monster mon_on(struct monster_race *race,
							 enum monster_allegiance side) {
	struct monster mon;

	memset(&mon, 0, sizeof(mon));
	mon.race = race;
	mon.allegiance = side;

	return mon;
}

/**
 * Hostile is zero, so nothing has to be told to be hostile.
 *
 * Every monster in the game arrives through `mem_zalloc` or an assignment from
 * a zeroed struct. If hostile were any other value, every creation path in the
 * game -- generation, summoning, breeding, shapechange, the townsfolk -- would
 * need to set it, and the one that got missed would produce a monster that
 * quietly refuses to fight.
 */
static int test_a_monster_starts_hostile(void *state) {
	struct monster mon;

	memset(&mon, 0, sizeof(mon));

	eq(MON_ALLEGIANCE_HOSTILE, 0);
	eq(mon.allegiance, MON_ALLEGIANCE_HOSTILE);
	require(monster_is_hostile(&mon));
	require(!monster_is_pet(&mon));
	require(!monster_is_friendly(&mon));

	ok;
}

/**
 * The three states partition: exactly one predicate holds for each.
 *
 * Written as a count rather than three assertions because the failure this
 * guards against is a fourth state being added without a predicate, or two
 * predicates overlapping -- both of which look right one assertion at a time.
 */
static int test_the_predicates_partition(void *state) {
	int side;

	for (side = 0; side < MON_ALLEGIANCE_MAX; side++) {
		struct monster mon = mon_on(&r_info[1], side);
		int hold = 0;

		if (monster_is_hostile(&mon)) hold++;
		if (monster_is_friendly(&mon)) hold++;
		if (monster_is_pet(&mon)) hold++;

		eq(hold, 1);
	}

	eq(MON_ALLEGIANCE_MAX, 3);

	ok;
}

/**
 * A pet made friendly is friendly, and is no longer a pet.
 *
 * This is the Zangband bug written as a test. `set_pet()` there was
 * `m_ptr->smart |= SM_PET` and `set_friendly()` was `|= SM_FRIENDLY`, so this
 * sequence left both bits standing and `is_pet()` -- tested first almost
 * everywhere -- kept answering yes. A monster the player had deliberately
 * released still cost upkeep and still took orders.
 */
static int test_a_side_replaces_the_last_one(void *state) {
	struct monster mon = mon_on(&r_info[1], MON_ALLEGIANCE_HOSTILE);

	monster_set_allegiance(&mon, MON_ALLEGIANCE_PET);
	require(monster_is_pet(&mon));

	monster_set_allegiance(&mon, MON_ALLEGIANCE_FRIENDLY);
	require(monster_is_friendly(&mon));
	require(!monster_is_pet(&mon));
	require(!monster_is_hostile(&mon));

	monster_set_allegiance(&mon, MON_ALLEGIANCE_HOSTILE);
	require(monster_is_hostile(&mon));
	require(!monster_is_pet(&mon));
	require(!monster_is_friendly(&mon));

	ok;
}

/**
 * Opposite sides are enemies; the same side is not.
 *
 * Pet and friendly are the same side as each other, which is why the rule is
 * written on `monster_is_hostile()` rather than on the pet predicate: a pet
 * and a wandering friendly monster have no quarrel.
 */
static int test_opposite_sides_are_enemies(void *state) {
	struct monster_race *plain = race_with(false, false);
	int a, b, pairs = 0, enemies = 0;

	require(plain);

	for (a = 0; a < MON_ALLEGIANCE_MAX; a++) {
		for (b = 0; b < MON_ALLEGIANCE_MAX; b++) {
			struct monster one = mon_on(plain, a);
			struct monster two = mon_on(plain, b);
			bool want = (a == MON_ALLEGIANCE_HOSTILE)
				!= (b == MON_ALLEGIANCE_HOSTILE);

			pairs++;
			if (monsters_are_enemies(&one, &two)) enemies++;
			eq(monsters_are_enemies(&one, &two), want);
		}
	}

	/* Nine ordered pairs, of which four cross the line */
	eq(pairs, 9);
	eq(enemies, 4);

	ok;
}

/**
 * Good and evil fight whatever side they are on.
 *
 * The rule Zangband checks *before* the side rule
 * ([monster1.c:1760](../archive/zangband/src/monster1.c#L1760)), and the one
 * that is easy to lose: it means two of the player's own pets will turn on
 * each other, which is a consequence worth having a test say out loud.
 */
static int test_alignment_crosses_sides(void *state) {
	struct monster_race *good = race_with(true, false);
	struct monster_race *evil = race_with(false, true);
	int side, fights = 0;

	require(good);
	require(evil);

	for (side = 0; side < MON_ALLEGIANCE_MAX; side++) {
		struct monster one = mon_on(good, side);
		struct monster two = mon_on(evil, side);

		/* Same side, opposed alignment: still enemies */
		require(monsters_are_enemies(&one, &two));
		require(monsters_are_enemies(&two, &one));
		fights++;
	}

	eq(fights, MON_ALLEGIANCE_MAX);

	ok;
}

/**
 * Nothing is its own enemy.
 *
 * A monster of opposed alignment to itself is impossible, but a *balance*
 * creature carries both GOOD and EVIL, so without the identity check the
 * alignment rule would make it its own enemy -- and `nice_target()` in the AI
 * picks targets by walking every monster including itself.
 */
static int test_nothing_fights_itself(void *state) {
	struct monster_race *both = race_with(true, true);
	struct monster mon;

	require(both);
	mon = mon_on(both, MON_ALLEGIANCE_PET);

	require(!monsters_are_enemies(&mon, &mon));

	ok;
}

/**
 * A balance creature is the enemy of both alignments.
 *
 * Zangband gives the balance drake and the Great Wyrm of Balance GOOD *and*
 * EVIL, and its `are_enemies()` reads them one clause at a time, so each rule
 * fires in one direction. The result is a creature at war with everything
 * aligned and at peace with everything neutral, which is a good description of
 * Balance and not obviously what the code was trying to say -- so it is pinned
 * here rather than left to be tidied away as a bug.
 */
static int test_balance_fights_both(void *state) {
	struct monster_race *both = race_with(true, true);
	struct monster_race *good = race_with(true, false);
	struct monster_race *evil = race_with(false, true);
	struct monster_race *plain = race_with(false, false);
	struct monster balance, other;

	require(both && good && evil && plain);

	balance = mon_on(both, MON_ALLEGIANCE_PET);

	other = mon_on(good, MON_ALLEGIANCE_PET);
	require(monsters_are_enemies(&balance, &other));

	other = mon_on(evil, MON_ALLEGIANCE_PET);
	require(monsters_are_enemies(&balance, &other));

	/* But not with an unaligned one on its own side */
	other = mon_on(plain, MON_ALLEGIANCE_PET);
	require(!monsters_are_enemies(&balance, &other));

	ok;
}

/**
 * The GOOD flag reached the creatures Zangband gives it to.
 *
 * Angband 4.2 has EVIL and no counterpart, so `monsters_are_enemies()` had
 * nothing to read on one side of its alignment rule. Zangband flags 33 races
 * GOOD, mostly angels, of which seven are races we have imported. A count,
 * because carrying one of the seven and carrying all of them look the same
 * from a spot check.
 */
static int test_the_good_flag_is_on_the_right_races(void *state) {
	static const char *want[] = {
		"priest", "law drake", "balance drake", "The Queen Ant",
		"The Phoenix", "great wyrm of law", "great wyrm of balance"
	};
	int i, found = 0, total = 0;

	for (i = 1; i < z_info->r_max; i++) {
		if (r_info[i].name && rf_has(r_info[i].flags, RF_GOOD)) total++;
	}
	eq(total, (int) N_ELEMENTS(want));

	for (i = 0; i < (int) N_ELEMENTS(want); i++) {
		struct monster_race *race = lookup_monster(want[i]);

		require(race);
		require(rf_has(race->flags, RF_GOOD));
		found++;
	}
	eq(found, (int) N_ELEMENTS(want));

	ok;
}

/**
 * Balance is both, and Law is not.
 *
 * Two data lines that a bulk edit would flatten in either direction: dropping
 * EVIL from the balance creatures to "tidy" the contradiction, or spreading it
 * to the law ones. The first would stop a balance drake fighting demons; the
 * second would make a Great Wyrm of Law fight itself.
 */
static int test_balance_is_flagged_both_ways(void *state) {
	struct monster_race *bal = lookup_monster("balance drake");
	struct monster_race *wyrm = lookup_monster("great wyrm of balance");
	struct monster_race *law = lookup_monster("great wyrm of law");

	require(bal && wyrm && law);

	require(rf_has(bal->flags, RF_GOOD));
	require(rf_has(bal->flags, RF_EVIL));
	require(rf_has(wyrm->flags, RF_GOOD));
	require(rf_has(wyrm->flags, RF_EVIL));

	require(rf_has(law->flags, RF_GOOD));
	require(!rf_has(law->flags, RF_EVIL));

	ok;
}

const char *suite_name = "monster/allegiance";
struct test tests[] = {
	{ "a-monster-starts-hostile", test_a_monster_starts_hostile },
	{ "the-predicates-partition", test_the_predicates_partition },
	{ "a-side-replaces-the-last-one", test_a_side_replaces_the_last_one },
	{ "opposite-sides-are-enemies", test_opposite_sides_are_enemies },
	{ "alignment-crosses-sides", test_alignment_crosses_sides },
	{ "nothing-fights-itself", test_nothing_fights_itself },
	{ "balance-fights-both", test_balance_fights_both },
	{ "the-good-flag-is-on-the-right-races",
	  test_the_good_flag_is_on_the_right_races },
	{ "balance-is-flagged-both-ways", test_balance_is_flagged_both_ways },
	{ NULL, NULL }
};
