/* player/pet-upkeep
 *
 * What pets cost, and what they earn you (ZangbandTK, PLR-30, PLR-31).
 *
 * These two requirements are the whole of pet balance, and the documentation
 * says so in as many words: "it takes a lot of mental energy to maintain the
 * control over the charmed monsters", and "you will only gain experience for
 * creatures to whom you deliver the death blow yourself". Without the first, a
 * summoner's stable grows without limit; without the second, it grows the
 * character too.
 *
 * PLR-30's rule has two halves that measure different things, which is the
 * sharpest edge in it and the thing most likely to be smoothed away by
 * accident:
 *
 *   - a **count** is free: `1 + level / pet_upkeep_div` pets;
 *   - past that count, the **sum of the pets' levels** is the percentage of
 *     mana regeneration withheld, clamped to 5..95.
 *
 * So the charge is not per-pet-over-the-limit. One pet over the allowance
 * turns the meter on for the entire stable at once, and the third animal can
 * cost more than the first two put together. A plausible-looking
 * implementation that charges only for the excess passes any test that just
 * asks "do pets cost mana", and is a different game.
 *
 * The weight is the monster's level. Zangband's code says `hdice * 2` and its
 * documentation says "the sum of the levels of your pets"; measured across its
 * 883 monsters those are the same number written twice -- equal for 48%
 * exactly, median difference zero, within two for 96%. DEC-59 records the
 * measurement.
 */

#include "unit-test.h"
#include "test-utils.h"

#include "cave.h"
#include "game-world.h"
#include "generate.h"
#include "init.h"
#include "mon-make.h"
#include "mon-predicate.h"
#include "mon-util.h"
#include "monster.h"
#include "player-birth.h"
#include "player-util.h"

static void println(const char *str) {
	printf("%s\n", str);
}

int setup_tests(void **state) {
	plog_aux = println;
	set_file_paths();
	if (!init_angband()) return 1;
	(void) test_seed_rng_reported(suite_name);
	if (!player_make_simple(NULL, "Mage", "Tester")) return 1;
	prepare_next_level(player);
	on_new_level();
	*state = NULL;
	return 0;
}

int teardown_tests(void *state) {
	if (cave) wipe_mon_list(cave, player);
	cleanup_angband();
	return 0;
}

/** Remove every monster from the level. */
static void clear_the_level(void) {
	int i;

	for (i = 1; i < cave_monster_max(cave); i++) {
		if (cave_monster(cave, i)->race) delete_monster_idx(cave, i);
	}
}

/** Put `n` pets of that race on the level; returns how many were placed. */
static int place_pets(const char *name, int n) {
	struct monster_group_info info = { 0, 0 };
	struct monster_race *race = lookup_monster(name);
	int placed = 0, i;

	if (!race) return 0;

	for (i = 0; i < 400 && placed < n; i++) {
		struct loc grid;

		if (scatter_ext(cave, &grid, 1, player->grid, 20, true,
						square_isempty) == 0) continue;
		if (!place_new_monster(cave, grid, race, false, false, info,
							   ORIGIN_DROP)) continue;
		monster_set_allegiance(square_monster(cave, grid),
							   MON_ALLEGIANCE_PET);
		placed++;
	}

	return placed;
}

/**
 * Nothing following you costs nothing.
 */
static int test_no_pets_cost_nothing(void *state) {
	clear_the_level();

	eq(player_pet_upkeep(player), 0);

	ok;
}

/**
 * The allowance is a count, and it is free.
 *
 * A level 1 Mage keeps `1 + 1/15` = one pet for nothing. Placed one at a time
 * so the boundary is crossed under the test's nose rather than jumped over.
 */
static int test_the_allowance_is_free(void *state) {
	int free_pets;

	clear_the_level();
	player->lev = 1;
	free_pets = 1 + (player->lev / player->class->pet_upkeep_div);
	eq(free_pets, 1);

	eq(place_pets("large white snake", 1), 1);
	eq(player_pet_upkeep(player), 0);

	/* One more and the meter starts */
	eq(place_pets("large white snake", 1), 1);
	require(player_pet_upkeep(player) > 0);

	ok;
}

/**
 * Going one over charges for the whole stable, not for the excess.
 *
 * The rule most likely to be quietly reasonable-ised into charging per extra
 * pet. Two snakes over a one-pet allowance cost the sum of *both* their levels,
 * not one.
 */
static int test_the_charge_covers_every_pet(void *state) {
	struct monster_race *race = lookup_monster("large white snake");
	int one_over, two_over;

	notnull(race);
	require(race->level >= 1);

	clear_the_level();
	player->lev = 1;

	eq(place_pets("large white snake", 2), 2);
	one_over = player_pet_upkeep(player);

	eq(place_pets("large white snake", 1), 1);
	two_over = player_pet_upkeep(player);

	/* Both figures are the running total of levels, floored at five */
	eq(one_over, MAX(5, 2 * race->level));
	eq(two_over, MAX(5, 3 * race->level));

	ok;
}

/**
 * The charge is the sum of the pets' levels, and it is clamped.
 *
 * Both ends. The floor matters because a stable of weak animals would
 * otherwise be free past the allowance, and the ceiling because without it a
 * single deep pet would stop mana regeneration outright and then go negative.
 */
static int test_the_charge_is_clamped(void *state) {
	int upkeep;

	clear_the_level();
	player->lev = 1;

	/* Weak animals over the allowance: floored at five */
	eq(place_pets("large white snake", 3), 3);
	upkeep = player_pet_upkeep(player);
	require(upkeep >= 5);

	/* Something enormous: capped at ninety-five */
	clear_the_level();
	eq(place_pets("great wyrm of power", 2), 2);
	upkeep = player_pet_upkeep(player);
	eq(upkeep, 95);

	ok;
}

/**
 * A class with a smaller divider keeps more pets for nothing.
 *
 * The per-class figure is data, and Zangband's three distinct values are the
 * only thing distinguishing a Mage's stable from a Warrior's. Read off the
 * classes rather than off a character, so this checks the data file.
 */
static int test_the_divider_is_per_class(void *state) {
	struct player_class *mage = NULL, *high = NULL, *warrior = NULL, *c;
	int seen = 0;

	for (c = classes; c; c = c->next) {
		if (streq(c->name, "Mage")) mage = c;
		if (streq(c->name, "High-Mage")) high = c;
		if (streq(c->name, "Warrior")) warrior = c;
	}
	notnull(mage);
	notnull(high);
	notnull(warrior);

	eq(warrior->pet_upkeep_div, 20);
	eq(mage->pet_upkeep_div, 15);
	eq(high->pet_upkeep_div, 12);

	/* At level 48 that is three free pets, four, and five */
	eq(1 + 48 / warrior->pet_upkeep_div, 3);
	eq(1 + 48 / mage->pet_upkeep_div, 4);
	eq(1 + 48 / high->pet_upkeep_div, 5);

	/* And every class has one, since dividing by zero is not an allowance */
	for (c = classes; c; c = c->next) {
		require(c->pet_upkeep_div >= 1);
		seen++;
	}
	eq(seen, 14);

	ok;
}

/**
 * Mana regained over a hundred turns, from empty.
 *
 * A hundred rather than one, and the fraction reset with the pool: a single
 * turn's regeneration is a fraction of a spell point, kept in `csp_frac`, and
 * one call can round to nothing at all -- or to a whole point, if the previous
 * measurement left the fraction nearly full. Both of those made this test lie
 * before it was written this way.
 */
static int32_t regained_over_a_hundred_turns(void) {
	int i;

	player->csp = 0;
	player->csp_frac = 0;

	for (i = 0; i < 100; i++) player_regen_mana(player);

	return player->csp;
}

/**
 * The upkeep actually comes out of mana regeneration.
 *
 * The calculation could be right and connected to nothing. Compared against
 * the same character in the same state without the pets, because the
 * regeneration formula has several other terms and the absolute numbers are
 * not the point.
 */
static int test_the_upkeep_slows_regeneration(void *state) {
	int32_t without, with;

	clear_the_level();
	player->lev = 20;
	player->msp = 100;

	without = regained_over_a_hundred_turns();
	require(without > 0);

	/* Enough deep pets to be charged the maximum */
	eq(place_pets("great wyrm of power", 3), 3);
	eq(player_pet_upkeep(player), 95);

	with = regained_over_a_hundred_turns();

	require(with < without);

	/* And by about the right amount: 95% withheld leaves a twentieth */
	require(with <= without / 10);

	ok;
}

/** Mana *lost* over a hundred turns, from full, for a Blackguard. */
static int32_t burned_over_a_hundred_turns(void) {
	int i;

	player->csp = player->msp;
	player->csp_frac = 0;

	for (i = 0; i < 100; i++) {
		/* Kept above half health, since that is when a Blackguard burns */
		player->chp = player->mhp;
		player_regen_mana(player);
	}

	return player->msp - player->csp;
}

/**
 * A Blackguard is not paid for keeping pets.
 *
 * PF_COMBAT_REGEN makes a Blackguard's mana regeneration *negative* -- it
 * burns spell points rather than restoring them -- so scaling it by the
 * upkeep factor would make a stable of pets slow the burn down. Zangband had
 * no such class and multiplied unconditionally; here that would pay the player
 * for the thing the mechanism charges for.
 */
static int test_a_blackguard_is_not_paid_for_pets(void *state) {
	int32_t alone, with_pets;

	if (cave) wipe_mon_list(cave, player);
	cleanup_angband();
	set_file_paths();
	require(init_angband());
	require(player_make_simple(NULL, "Blackguard", "Tester"));
	prepare_next_level(player);
	on_new_level();

	require(player_has(player, PF_COMBAT_REGEN));

	clear_the_level();
	player->lev = 20;
	player->msp = 100;

	alone = burned_over_a_hundred_turns();
	require(alone > 0);

	eq(place_pets("great wyrm of power", 3), 3);
	eq(player_pet_upkeep(player), 95);

	with_pets = burned_over_a_hundred_turns();

	/* The burn is the same with a stable as without one */
	eq(with_pets, alone);

	ok;
}

/**
 * Experience is for the killing blow (PLR-31).
 *
 * A pet's kill goes through `mon_take_nonplayer_hit()`, which awards none, and
 * the player's through `mon_take_hit()`, which does. Both halves, because a
 * game that awards nothing for anything also satisfies the first.
 */
static int test_experience_is_for_the_killing_blow(void *state) {
	struct monster_group_info info = { 0, 0 };
	struct monster_race *race;
	struct loc grid;
	struct monster *victim;
	int32_t before;
	bool fear = false;

	/*
	 * A Warrior, so the kill is worth something and nothing else is going on.
	 * The race is looked up *after* the reset: `cleanup_angband()` frees
	 * `r_info`, and a pointer taken before it is dangling. ASAN said so, for
	 * the second time this milestone.
	 */
	if (cave) wipe_mon_list(cave, player);
	cleanup_angband();
	set_file_paths();
	require(init_angband());
	require(player_make_simple(NULL, "Warrior", "Tester"));
	prepare_next_level(player);
	on_new_level();
	clear_the_level();

	race = lookup_monster("large white snake");
	notnull(race);
	require(race->mexp > 0);

	/* Killed by something that is not the player */
	require(scatter_ext(cave, &grid, 1, player->grid, 6, true,
						square_isempty) > 0);
	require(place_new_monster(cave, grid, race, false, false, info,
							  ORIGIN_DROP));
	victim = square_monster(cave, grid);
	notnull(victim);

	before = player->exp;
	mon_take_nonplayer_hit(victim->hp + 100, victim, MON_MSG_NONE,
						   MON_MSG_DIE);
	eq(player->exp, before);

	/* Killed by the player */
	require(scatter_ext(cave, &grid, 1, player->grid, 6, true,
						square_isempty) > 0);
	require(place_new_monster(cave, grid, race, false, false, info,
							  ORIGIN_DROP));
	victim = square_monster(cave, grid);
	notnull(victim);

	before = player->exp;
	(void) mon_take_hit(victim, player, victim->hp + 100, &fear, NULL);
	require(player->exp > before);

	ok;
}

/**
 * A pet cannot finish a unique.
 *
 * The other half of PLR-31's protection, and the reason a pet cannot be sent
 * into a vault to farm the interesting things: `mon_take_nonplayer_hit()`
 * caps the damage so a unique lives at one hit point whatever hits it.
 */
static int test_a_pet_cannot_kill_a_unique(void *state) {
	struct monster_group_info info = { 0, 0 };
	struct monster_race *race = lookup_monster("Grip, Farmer Maggot's dog");
	struct loc grid;
	struct monster *victim;

	notnull(race);
	require(rf_has(race->flags, RF_UNIQUE));

	clear_the_level();
	require(scatter_ext(cave, &grid, 1, player->grid, 6, true,
						square_isempty) > 0);
	require(place_new_monster(cave, grid, race, false, false, info,
							  ORIGIN_DROP));
	victim = square_monster(cave, grid);
	notnull(victim);

	mon_take_nonplayer_hit(victim->hp + 1000, victim, MON_MSG_NONE,
						   MON_MSG_DIE);

	/*
	 * Still there. 4.2 caps the damage at the unique's remaining hit points
	 * rather than leaving it at one, as Zangband did, so it survives on zero
	 * -- death in this codebase is hp < 0, not hp <= 0. Either way it lives
	 * and the pet does not get the kill.
	 */
	require(victim->race);
	require(victim->hp >= 0);
	eq(victim->hp, 0);

	ok;
}

/**
 * What a stable actually costs, in numbers (PLR-26 phase F, for DEC-65).
 *
 * Pets following the player between levels changes what limits a stable. It
 * used to be limited twice: by the mana upkeep, and by having to be rebuilt on
 * every level. The second limit is gone, so the upkeep is now the only one --
 * and the upkeep is capped at withholding 95 per cent of mana regeneration,
 * which is a percentage of a small number.
 *
 * This test asserts almost nothing. It prints the figures the decision needs,
 * so that the argument is made against measurements rather than against
 * intuition, and so a later change to the regeneration formula shows up here.
 */
static int test_what_a_stable_costs(void *state) {
	static const int sizes[] = { 1, 2, 3, 4, 6, 10, 24 };
	struct monster_race *race = lookup_monster("soldier");
	int32_t base;
	size_t k;

	notnull(race);
	clear_the_level();

	player->lev = 30;
	player->msp = 120;
	base = regained_over_a_hundred_turns();
	require(base > 0);

	printf("  a level 30 Mage with %d sp regains %d sp per 100 turns with no "
		   "pets\n", (int) player->msp, (int) base);
	printf("  free allowance at level 30: %d pets\n",
		   1 + player->lev / player->class->pet_upkeep_div);
	printf("  %-6s %-8s %-8s %-10s %s\n", "pets", "levels", "upkeep",
		   "sp/100t", "vs none");

	for (k = 0; k < N_ELEMENTS(sizes); k++) {
		int32_t with;
		int upkeep, placed;

		clear_the_level();
		placed = place_pets("soldier", sizes[k]);
		if (placed != sizes[k]) continue;

		upkeep = player_pet_upkeep(player);
		with = regained_over_a_hundred_turns();

		printf("  %-6d %-8d %-8d %-10d %d%%\n", placed,
			   placed * race->level, upkeep, (int) with,
			   base ? (int) (100 * with / base) : 0);
	}

	/* And the same for pets worth having, which is where the cap binds */
	{
		struct monster_race *deep = lookup_monster("young red dragon");
		int placed;

		notnull(deep);
		printf("  a %s is level %d\n", deep->name, deep->level);

		for (k = 0; k < N_ELEMENTS(sizes); k++) {
			int32_t with;
			int upkeep;

			clear_the_level();
			placed = place_pets(deep->name, sizes[k]);
			if (placed != sizes[k]) continue;

			upkeep = player_pet_upkeep(player);
			with = regained_over_a_hundred_turns();

			printf("  %-6d %-8d %-8d %-10d %d%%\n", placed,
				   placed * deep->level, upkeep, (int) with,
				   base ? (int) (100 * with / base) : 0);
		}
	}

	/*
	 * The property worth asserting, rather than a figure that moves with the
	 * monster data: the charge is monotonic in the size of the stable, and it
	 * is capped. Those two together are the whole of what limits a stable now
	 * that it no longer has to be rebuilt every level -- which is the
	 * substance of DEC-65.
	 */
	{
		int small, large;

		clear_the_level();
		if (place_pets("soldier", 3) == 3) {
			small = player_pet_upkeep(player);
			require(small > 0);

			if (place_pets("soldier", 7) == 7) {
				large = player_pet_upkeep(player);
				require(large > small);
				require(large <= 95);
			}
		}
	}

	ok;
}

const char *suite_name = "player/pet-upkeep";
struct test tests[] = {
	{ "no-pets-cost-nothing", test_no_pets_cost_nothing },
	{ "the-allowance-is-free", test_the_allowance_is_free },
	{ "the-charge-covers-every-pet", test_the_charge_covers_every_pet },
	{ "the-charge-is-clamped", test_the_charge_is_clamped },
	{ "the-divider-is-per-class", test_the_divider_is_per_class },
	{ "the-upkeep-slows-regeneration", test_the_upkeep_slows_regeneration },
	{ "a-pet-cannot-kill-a-unique", test_a_pet_cannot_kill_a_unique },
	{ "a-blackguard-is-not-paid-for-pets",
	  test_a_blackguard_is_not_paid_for_pets },
	{ "experience-is-for-the-killing-blow",
	  test_experience_is_for_the_killing_blow },
	{ "what-a-stable-costs", test_what_a_stable_costs },
	{ NULL, NULL }
};
