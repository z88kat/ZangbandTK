/* monster/ally-ai
 *
 * What a monster on the player's side does with its turn (ZangbandTK, PLR-23).
 *
 * Angband's monster AI has one goal in it. Every path through `get_move()`
 * measures the player: it follows the noise and scent heatmaps that flow out
 * from the player's grid, it flees *from* the player, and its pack AI works to
 * surround the player and drag them out of corridors. A monster fighting
 * alongside the player has no use for any of that, so PLR-23's monsters get
 * their own branch rather than a substituted target — which is how Zangband
 * split it too.
 *
 * Four things have to be true for a pet to be a pet, and each is a separate
 * failure:
 *
 * 1. it finds an enemy to fight;
 * 2. it will not pick a fight its standing orders forbid;
 * 3. it stays awake when the player is nowhere near, since every one of
 *    Angband's six wake-up tests asks about the player;
 * 4. and when two enemies meet, they fight rather than shove past.
 *
 * The fourth is the one with a trap in it. `monster_can_kill()` lets a monster
 * with KILL_BODY walk over a weaker one and delete it outright, and that check
 * used to come first — a pet standing between the player and something large
 * would simply stop existing, with no blows struck and no message.
 */

#include "unit-test.h"
#include "test-utils.h"

#include "cave.h"
#include "game-world.h"
#include "generate.h"
#include "init.h"
#include "mon-make.h"
#include "mon-move.h"
#include "mon-predicate.h"
#include "mon-util.h"
#include "monster.h"
#include "player-birth.h"
#include "project.h"

static void println(const char *str) {
	printf("%s\n", str);
}

int setup_tests(void **state) {
	plog_aux = println;
	set_file_paths();
	if (!init_angband()) return 1;
	(void) test_seed_rng_reported(suite_name);
	if (!player_make_simple(NULL, "Warrior", "Tester")) return 1;
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

/**
 * Put a monster of that race on that side, near the player.
 *
 * Returns NULL rather than asserting: a level can be crowded, and a test that
 * cannot set up should say so where it is written rather than here.
 */
static struct monster *place_side(const char *name,
								  enum monster_allegiance side, int within) {
	struct monster_group_info info = { 0, 0 };
	struct monster_race *race = lookup_monster(name);
	struct loc grid;
	int i;

	if (!race) return NULL;

	for (i = 0; i < 50; i++) {
		if (scatter_ext(cave, &grid, 1, player->grid, within, true,
						square_isempty) == 0) continue;
		if (!place_new_monster(cave, grid, race, false, false, info,
							   ORIGIN_DROP)) continue;
		monster_set_allegiance(square_monster(cave, grid), side);
		return square_monster(cave, grid);
	}

	return NULL;
}

/** A monster of that race on that side, at exactly that grid. */
static struct monster *place_at(struct loc grid, const char *name,
								enum monster_allegiance side) {
	struct monster_group_info info = { 0, 0 };
	struct monster_race *race = lookup_monster(name);

	if (!race) return NULL;
	if (!place_new_monster(cave, grid, race, false, false, info, ORIGIN_DROP))
		return NULL;

	monster_set_allegiance(square_monster(cave, grid), side);

	return square_monster(cave, grid);
}

/**
 * A monster of that race on that side, somewhere `near` can fight it.
 *
 * Adjacent for preference, because that is unambiguous. Failing that, a grid
 * within three that `near` has a clear line to -- which is what
 * `monster_nice_target()` actually requires, and the reason "within three"
 * alone is not enough: a randomly generated level will put a grid three away
 * round a corner, and the pet correctly refuses to target through a wall.
 *
 * The fallback is not decoration. Placing two enemies beside one pet needs two
 * free neighbours, and a pet in a corridor has one -- which failed about one
 * run in two hundred until `scripts/check-flakes` learned to report the seed
 * and named it twice in the same place.
 */
static struct monster *place_beside(struct monster *near, const char *name,
									enum monster_allegiance side) {
	int i;

	for (i = 0; i < 8; i++) {
		struct loc grid = loc_sum(near->grid, ddgrid_ddd[i]);

		if (!square_in_bounds_fully(cave, grid)) continue;
		if (!square_isempty(cave, grid)) continue;

		return place_at(grid, name, side);
	}

	for (i = 0; i < 60; i++) {
		struct loc grid;

		if (scatter_ext(cave, &grid, 1, near->grid, 3, true,
						square_isempty) == 0) continue;
		if (!projectable(cave, near->grid, grid, PROJECT_NONE)) continue;

		return place_at(grid, name, side);
	}

	return NULL;
}

/** Everything on the level goes back to being hostile and untargeted. */
static void clear_the_level(void) {
	int i;

	for (i = 1; i < cave_monster_max(cave); i++) {
		struct monster *mon = cave_monster(cave, i);

		if (!mon->race) continue;
		delete_monster_idx(cave, i);
	}
}

/**
 * A pet finds something to fight.
 *
 * The floor of the whole feature: without this a pet follows the player around
 * and never does anything, which is indistinguishable from a friendly monster.
 */
static int test_a_pet_finds_an_enemy(void *state) {
	struct monster *pet, *foe;

	clear_the_level();
	pet = place_side("soldier", MON_ALLEGIANCE_PET, 3);
	require(pet);
	foe = place_beside(pet, "kobold", MON_ALLEGIANCE_HOSTILE);
	require(foe);

	require(monster_find_enemy(pet));
	eq(pet->target.midx, foe->midx);

	ok;
}

/**
 * A hostile monster is not given a monster to chase.
 *
 * The same function, asked about the other side. `monster_find_enemy()` is
 * only called for non-hostile monsters, but it is a public function now and
 * the rule it encodes should hold whoever calls it: a hostile monster's enemy
 * is the player, and the pet standing in front of them is scenery until it is
 * in the way.
 */
static int test_a_hostile_monster_keeps_hunting_the_player(void *state) {
	struct monster *pet, *foe;

	clear_the_level();
	pet = place_side("soldier", MON_ALLEGIANCE_PET, 3);
	require(pet);
	foe = place_beside(pet, "kobold", MON_ALLEGIANCE_HOSTILE);
	require(foe);

	/* The hostile one does find the pet -- they are enemies both ways... */
	require(monsters_are_enemies(foe, pet));

	/* ...but nothing in the turn asks it to, so its target stays empty */
	eq(foe->target.midx, 0);

	ok;
}

/**
 * A pet told to keep its distance will not start a fight near the player.
 *
 * The leash's sign carries the meaning: negative is "stay at least this far
 * away", and a pet on it refuses any target closer to the player than that.
 * Checked by moving the *order*, not the monsters, so nothing else differs
 * between the two halves.
 */
static int test_the_leash_refuses_a_close_fight(void *state) {
	struct monster *pet, *foe;
	int16_t keep = player->pet_follow_distance;

	clear_the_level();
	pet = place_side("soldier", MON_ALLEGIANCE_PET, 1);
	require(pet);
	foe = place_beside(pet, "kobold", MON_ALLEGIANCE_HOSTILE);
	require(foe);

	/* Adjacent to a pet that is adjacent to the player, so at most two away */
	require(foe->cdis <= 2);

	/* Follow me: it takes the fight */
	player->pet_follow_distance = PET_FOLLOW_DIST;
	pet->target.midx = 0;
	require(monster_find_enemy(pet));

	/* Stay away: the same fight, now too close to the player */
	player->pet_follow_distance = PET_AWAY_DIST;
	pet->target.midx = 0;
	require(!monster_find_enemy(pet));

	player->pet_follow_distance = keep;

	ok;
}

/**
 * A pet keeps its target while the target is still worth keeping.
 *
 * Zangband re-uses the remembered target rather than rescanning, and drops it
 * only when it stops qualifying. Worth a test because the cheap implementation
 * — rescan every turn — passes the test above and produces a pet that switches
 * victim every time something new wanders into the room.
 */
static int test_a_pet_keeps_its_target(void *state) {
	struct monster *pet, *first, *second;

	clear_the_level();
	pet = place_side("soldier", MON_ALLEGIANCE_PET, 3);
	require(pet);
	first = place_beside(pet, "kobold", MON_ALLEGIANCE_HOSTILE);
	require(first);

	require(monster_find_enemy(pet));
	eq(pet->target.midx, first->midx);

	/* Something newer arrives; the pet is already busy */
	second = place_beside(pet, "large white snake", MON_ALLEGIANCE_HOSTILE);
	require(second);
	require(monster_find_enemy(pet));
	eq(pet->target.midx, first->midx);

	/* When the first one dies, it takes the other */
	delete_monster_idx(cave, first->midx);
	require(monster_find_enemy(pet));
	eq(pet->target.midx, second->midx);

	ok;
}

/**
 * Changing sides forgets the fight.
 *
 * A pet that turns hostile (PLR-33) keeps its target field unless something
 * clears it, and a hostile monster with a monster target aims its spells at
 * that monster. The player would be attacked by nothing at all.
 */
static int test_turning_hostile_forgets_the_target(void *state) {
	struct monster *pet, *foe;

	clear_the_level();
	pet = place_side("soldier", MON_ALLEGIANCE_PET, 3);
	require(pet);
	foe = place_beside(pet, "kobold", MON_ALLEGIANCE_HOSTILE);
	require(foe);

	require(monster_find_enemy(pet));
	require(pet->target.midx > 0);

	monster_set_allegiance(pet, MON_ALLEGIANCE_HOSTILE);
	eq(pet->target.midx, 0);

	ok;
}

/**
 * A monster on the player's side is awake wherever the player is.
 *
 * Every one of Angband's activity tests measures the player — can it see,
 * hear or smell them, is it hurt, is it standing in fire. A pet fighting on
 * the far side of a level satisfies none of them and would sleep through it.
 */
static int test_an_ally_stays_awake(void *state) {
	struct monster *pet, *plain;

	clear_the_level();

	/*
	 * Really far away and really out of sight, rather than by setting `cdis`
	 * -- the activity check also asks whether the grid is in view, and a
	 * monster standing next to the player with a faked distance is in view
	 * whatever the number says. Two grids, adjacent to each other so they are
	 * the same distance from the player and differ only in their side.
	 */
	{
		struct loc grid = loc(0, 0), next = loc(0, 0);
		int i, j;
		bool found = false;

		/*
		 * A *pair* of grids, not one and then a neighbour. Searching for a
		 * far grid first and hoping it has a free neighbour failed about one
		 * run in fifty: a dead-end corridor square satisfies every test and
		 * has no empty neighbour at all, and the test then died on setup
		 * rather than on anything it was measuring. Found by check-flakes at
		 * twenty passes, having survived six and forty.
		 */
		for (i = 0; i < 4000 && !found; i++) {
			grid = loc(randint0(cave->width), randint0(cave->height));
			if (!square_in_bounds_fully(cave, grid)) continue;
			if (!square_isempty(cave, grid)) continue;
			if (square_isview(cave, grid)) continue;
			if (distance(grid, player->grid) <= z_info->max_sight) continue;

			for (j = 0; j < 8 && !found; j++) {
				next = loc_sum(grid, ddgrid_ddd[j]);

				if (!square_in_bounds_fully(cave, next)) continue;
				if (!square_isempty(cave, next)) continue;
				if (square_isview(cave, next)) continue;
				found = true;
			}
		}
		require(found);

		pet = place_at(grid, "soldier", MON_ALLEGIANCE_PET);
		require(pet);
		plain = place_at(next, "kobold", MON_ALLEGIANCE_HOSTILE);
		require(plain);
	}

	/*
	 * Give them the energy and the cleared HANDLED flag a real game turn
	 * would: `process_monsters()` only reaches the activity check for a
	 * monster with a full move in hand.
	 */
	pet->hp = pet->maxhp;
	plain->hp = plain->maxhp;
	pet->energy = z_info->move_energy;
	plain->energy = z_info->move_energy;
	mflag_off(pet->mflag, MFLAG_ACTIVE);
	mflag_off(plain->mflag, MFLAG_ACTIVE);
	mflag_off(pet->mflag, MFLAG_HANDLED);
	mflag_off(plain->mflag, MFLAG_HANDLED);

	/*
	 * `monster_check_active()` is static, so this reaches it the way the game
	 * does: a monster processing pass.
	 */
	process_monsters(0);

	require(mflag_has(pet->mflag, MFLAG_ACTIVE));

	/*
	 * And the hostile one, which differs only in its side, is not woken by
	 * this rule.  Without the pair the test would pass against a change that
	 * simply woke everything.
	 */
	require(!mflag_has(plain->mflag, MFLAG_ACTIVE));

	ok;
}

/**
 * Two enemies that meet fight, and the fight does damage.
 *
 * Placed adjacent and given one turn. The assertion is on hit points rather
 * than on a message, because the message depends on what the player can see
 * and the damage does not.
 */
static int test_enemies_that_meet_fight(void *state) {
	struct monster *pet, *foe;
	int before, i;

	clear_the_level();
	pet = place_side("soldier", MON_ALLEGIANCE_PET, 3);
	require(pet);
	/*
	 * A white jelly, which never moves. The snake this used to use carries
	 * RAND_50 and wanders: once it turns a corner the pet cannot see it,
	 * `monster_nice_target()` drops it for want of a clear line, and the pet
	 * mills about for the rest of the loop. That failed about one run in
	 * three hundred, which is exactly the kind of thing that is unfindable
	 * without the seed -- `scripts/check-flakes` reports it now, and this was
	 * the first flake it named rather than shrugging at.
	 *
	 * The jelly staying put is the point: what is being tested is that two
	 * adjacent enemies fight, not that a pet can chase something.
	 */
	foe = place_beside(pet, "white jelly", MON_ALLEGIANCE_HOSTILE);
	require(foe);
	require(rf_has(foe->race->flags, RF_NEVER_MOVE));

	before = foe->hp;

	/*
	 * Several turns: the pet has to win its to-hit roll, and a soldier's
	 * chance against a snake's armour is good but not certain. Forty turns of
	 * a two-blow monster missing every time is not a thing that happens.
	 *
	 * `MFLAG_HANDLED` has to be cleared each time round. The game clears it
	 * once per game turn in `reset_monsters()`, and calling
	 * `process_monsters()` in a loop without it gives one real turn and
	 * thirty-nine no-ops -- which is why this failed about one run in ten
	 * before `scripts/check-flakes` said so.
	 */
	for (i = 0; i < 40 && foe->race && foe->hp >= before; i++) {
		int j;

		for (j = 1; j < cave_monster_max(cave); j++) {
			struct monster *mon = cave_monster(cave, j);

			if (!mon->race) continue;
			mflag_off(mon->mflag, MFLAG_HANDLED);
			mon->energy = z_info->move_energy;
		}
		process_monsters(0);
	}

	/* Either it is hurt or it is dead; both mean the fight happened */
	require(!foe->race || foe->hp < before);

	ok;
}

/**
 * The standing orders gate doors and pickup, and only for pets (PLR-25).
 *
 * The whole truth table, because each cell fails differently: a gate written
 * on the order alone stops *hostile* monsters opening doors, which would empty
 * the dungeon of door-opening monsters; a gate written on the race alone does
 * nothing at all; and a pet that ignores the order lets things out of rooms
 * the player deliberately shut.
 */
static int test_the_orders_gate_doors_and_pickup(void *state) {
	struct monster *pet, *foe;
	bool keep_doors = player->pet_open_doors;
	bool keep_items = player->pet_pickup_items;

	clear_the_level();

	/* A cutpurse opens doors and takes items; both flags on one race */
	pet = place_side("cutpurse", MON_ALLEGIANCE_PET, 3);
	require(pet);
	foe = place_beside(pet, "cutpurse", MON_ALLEGIANCE_HOSTILE);
	require(foe);
	require(rf_has(pet->race->flags, RF_OPEN_DOOR));
	require(rf_has(pet->race->flags, RF_TAKE_ITEM));

	player->pet_open_doors = false;
	player->pet_pickup_items = false;
	require(!monster_may_open_doors(pet));
	require(!monster_may_take_items(pet));

	/* The hostile one of the same race is not under orders */
	require(monster_may_open_doors(foe));
	require(monster_may_take_items(foe));

	player->pet_open_doors = true;
	player->pet_pickup_items = true;
	require(monster_may_open_doors(pet));
	require(monster_may_take_items(pet));

	player->pet_open_doors = keep_doors;
	player->pet_pickup_items = keep_items;

	ok;
}

/**
 * A race that cannot do it is not made able to by an order.
 *
 * Separate from the table above because it is the half a permissive
 * implementation gets wrong: `return player->pet_open_doors` passes every
 * assertion up there and gives a pet snake the run of the dungeon.
 */
static int test_an_order_grants_nothing_the_race_lacks(void *state) {
	struct monster *pet;
	bool keep_doors = player->pet_open_doors;
	bool keep_items = player->pet_pickup_items;

	clear_the_level();
	pet = place_side("large white snake", MON_ALLEGIANCE_PET, 3);
	require(pet);
	require(!rf_has(pet->race->flags, RF_OPEN_DOOR));
	require(!rf_has(pet->race->flags, RF_TAKE_ITEM));

	player->pet_open_doors = true;
	player->pet_pickup_items = true;
	require(!monster_may_open_doors(pet));
	require(!monster_may_take_items(pet));

	player->pet_open_doors = keep_doors;
	player->pet_pickup_items = keep_items;

	ok;
}

/**
 * Pets are left behind at a staircase (PLR-26, DEC-60).
 *
 * PLR-26 says pets "persist across level changes and saves, following the
 * player where the mode implies it". The saves half is true and tested in
 * `game/roundtrip`. The level-change half is **not Zangband's behaviour**:
 * there is no pet-carrying code anywhere in its source, and its own
 * documentation, which explains the upkeep and the experience rule and the
 * ways of getting a pet, never mentions taking one downstairs. Hengband added
 * that later; 2.7.5 did not have it.
 *
 * So a pet is a per-level asset here, and this test says so rather than
 * leaving it to be discovered. Reversing it later is a deliberate act with a
 * failing test attached, which is the point.
 */
static int test_pets_do_not_follow_you_downstairs(void *state) {
	struct monster *pet;
	int i, before = 0, after = 0;

	clear_the_level();
	pet = place_side("soldier", MON_ALLEGIANCE_PET, 3);
	require(pet);

	for (i = 1; i < cave_monster_max(cave); i++) {
		struct monster *mon = cave_monster(cave, i);

		if (mon->race && monster_is_pet(mon)) before++;
	}
	eq(before, 1);

	/* Down a level, the way the game does it */
	player->depth++;
	prepare_next_level(player);
	on_new_level();

	for (i = 1; i < cave_monster_max(cave); i++) {
		struct monster *mon = cave_monster(cave, i);

		if (mon->race && monster_is_pet(mon)) after++;
	}
	eq(after, 0);

	ok;
}

const char *suite_name = "monster/ally-ai";
struct test tests[] = {
	{ "a-pet-finds-an-enemy", test_a_pet_finds_an_enemy },
	{ "a-hostile-monster-keeps-hunting-the-player",
	  test_a_hostile_monster_keeps_hunting_the_player },
	{ "the-leash-refuses-a-close-fight",
	  test_the_leash_refuses_a_close_fight },
	{ "a-pet-keeps-its-target", test_a_pet_keeps_its_target },
	{ "turning-hostile-forgets-the-target",
	  test_turning_hostile_forgets_the_target },
	{ "an-ally-stays-awake", test_an_ally_stays_awake },
	{ "enemies-that-meet-fight", test_enemies_that_meet_fight },
	{ "the-orders-gate-doors-and-pickup",
	  test_the_orders_gate_doors_and_pickup },
	{ "an-order-grants-nothing-the-race-lacks",
	  test_an_order_grants_nothing_the_race_lacks },
	{ "pets-do-not-follow-you-downstairs",
	  test_pets_do_not_follow_you_downstairs },
	{ NULL, NULL }
};
