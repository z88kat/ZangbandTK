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
#include "player-calcs.h"
#include "game-world.h"
#include "generate.h"
#include "init.h"
#include "mon-attack.h"
#include "mon-make.h"
#include "mon-spell.h"
#include "mon-move.h"
#include "mon-predicate.h"
#include "mon-util.h"
#include "monster.h"
#include "option.h"
#include "player-birth.h"
#include "player-util.h"
#include "project.h"
#include "trap.h"

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

	/*
	 * Bring the view and the monster list up to date before handing it back.
	 *
	 * `monster_is_visible()` reads a flag that `update_mon()` sets, and a unit
	 * test never takes a player turn, so nothing recalculated it -- and the
	 * helper above may have regenerated the level, which leaves the player's
	 * view stale. Two tests asserted visibility and failed on it a few runs in
	 * a hundred for that reason and no other.
	 */
	player->upkeep->update |= (PU_UPDATE_VIEW | PU_MONSTERS);
	update_stuff(player);

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
 * A monster of that side, standing next to the player, on an empty level.
 *
 * Adjacency matters for the three tests below in a way it does not for the
 * rest of the suite: one of them measures whether a *visible* monster
 * disturbs the player, and a monster placed three grids away down an unlit
 * corridor is not visible, so the test would pass whatever the code did.
 *
 * `place_side(..., 1)` is the obvious way to write this and is wrong about one
 * run in ten -- the player can start in a dead end, where there is no free
 * neighbour to place anything on and the test dies on its own setup. So the
 * level is regenerated rather than the requirement relaxed, and cleared again
 * after, since a fresh level arrives with its own monsters and a hostile one
 * wandering into view would disturb the player for reasons of its own.
 */
/** How many grids next to this one a monster could be put in. */
static int free_neighbours(struct loc grid) {
	int i, n = 0;

	for (i = 0; i < 8; i++) {
		struct loc adj = loc_sum(grid, ddgrid_ddd[i]);

		if (!square_in_bounds_fully(cave, adj)) continue;
		if (!square_isempty(cave, adj)) continue;
		n++;
	}
	return n;
}

/**
 * A monster of that side, next to the player, with room beside it for two more.
 *
 * The room is the part that took three red Windows nights to notice. Every
 * test here stands a pet next to the player and then puts one or two enemies
 * next to the *pet*, and this used to accept the first free neighbour it found
 * -- so a pet placed in a corridor left `place_beside()` nothing to work with
 * and `require(foe)` failed. It is rare locally, about one run in sixty; on the
 * Windows runners it came up three times in six pushes and kept master red.
 *
 * Asking for two free neighbours costs nothing -- an ordinary room grid has
 * seven -- and makes the placement the tests depend on deterministic rather
 * than a property of the level that happened to be generated.
 */
static struct monster *place_next_to_the_player(const char *name,
												enum monster_allegiance side) {
	int attempt;

	for (attempt = 0; attempt < 40; attempt++) {
		int i;

		for (i = 0; i < 8; i++) {
			struct loc grid = loc_sum(player->grid, ddgrid_ddd[i]);

			if (!square_in_bounds_fully(cave, grid)) continue;
			if (!square_isempty(cave, grid)) continue;

			/*
			 * The first grid that will also hold the enemies, in the same
			 * order as before -- skipping a cramped one rather than searching
			 * for the roomiest, so that a level which was fine before this
			 * check existed still puts the pet exactly where it used to.
			 */
			if (free_neighbours(grid) < 2) continue;

			return place_at(grid, name, side);
		}

		prepare_next_level(player);
		on_new_level();
		clear_the_level();
	}

	return NULL;
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
 * Pets follow you downstairs (PLR-26, DEC-60 as reversed).
 *
 * This test used to assert the opposite, and the reversal is the point of
 * keeping the note. Zangband leaves every pet behind -- there is no
 * pet-carrying code anywhere in its source and its documentation never
 * mentions the subject -- so the first reading of PLR-26 followed the source
 * and pinned that, with a comment saying reversing it would be a deliberate
 * act with a failing test attached.
 *
 * It was reversed, deliberately, and the test failed as designed. The project
 * owner's reasoning: "Pets are one element of zangband that people like.
 * Usually people don't leave their pets behind." DEC-60 records that the
 * recommendation was the other way and why it lost.
 */
static int test_pets_follow_you_downstairs(void *state) {
	int t, best = 0, placed = 0;

	/*
	 * Up to ten descents, and the question is whether the pet *can* arrive.
	 * A pet that finds no free grid within the carry radius is left behind
	 * and named, which is the policy rather than a failure, so asserting one
	 * arrival from one descent asserts the level generator's good behaviour.
	 * `game/carry` measures the rate; this only has to show the reversal
	 * happened.
	 */
	for (t = 0; t < 40 && best < 1; t++) {
		struct monster *pet;
		int i, after = 0;

		clear_the_level();
		pet = place_side("soldier", MON_ALLEGIANCE_PET, 3);
		if (!pet) continue;
		placed++;

		/* Down a level, the way the game does it */
		player->depth++;
		prepare_next_level(player);
		on_new_level();

		for (i = 1; i < cave_monster_max(cave); i++) {
			struct monster *mon = cave_monster(cave, i);

			if (mon->race && monster_is_pet(mon)) after++;
		}

		/* Never more than went down */
		require(after <= 1);
		if (after > best) best = after;
	}

	/*
	 * Counted, because "the pet did not follow" and "there was nowhere to put
	 * a pet" are different answers and the loop used to give the same one for
	 * both: a `place_side()` that failed simply burned an attempt in silence.
	 * The attempts are raised from ten to forty for the same reason -- the
	 * placement wants a grid three away that the level may not have.
	 */
	if (!placed) {
		printf("no pet could be placed in %d attempts; the follow was never "
			   "tested\n", t);
		require(false);
	}
	eq(best, 1);

	ok;
}

/**
 * A pet does not attack the player, whatever the movement code asks of it.
 *
 * The bug this exists for killed a first level character in a corridor. The
 * player walked into their own soldier and pushed past it, which is the
 * intended courtesy -- and left the animal standing behind them with nowhere
 * to go but back down the corridor or forward into them. A pet within its
 * leash moves randomly, so every other turn its random step was the player's
 * grid, and `monster_turn()` resolves a monster that wants the player's grid
 * by calling `make_attack_normal()`. Nothing on that path asked whose side
 * the monster was on, so the pet killed its owner in about four turns.
 *
 * The guard is in `make_attack_normal()` rather than only at that call site
 * because it is the one place every future path has to pass through, which is
 * where Zangband put it too.
 */
static int test_a_pet_does_not_attack_the_player(void *state) {
	struct monster *pet;
	int i;
	int16_t keep_chp = player->chp, keep_mhp = player->mhp;

	clear_the_level();
	pet = place_next_to_the_player("soldier", MON_ALLEGIANCE_PET);
	require(pet);

	/* Fifty chances to land a blow it should not land */
	for (i = 0; i < 50; i++) {
		require(!make_attack_normal(pet, player));
	}
	eq(player->chp, keep_chp);
	require(!player->is_dead);

	/*
	 * The same monster on the other side does hit, so the test is measuring
	 * the side and not some other reason the blows never landed. Given a deep
	 * pool of hit points first: a soldier's two blows outdo a first level
	 * character, and a test that kills the player it is testing with proves
	 * nothing about the next case in the suite.
	 */
	monster_set_allegiance(pet, MON_ALLEGIANCE_HOSTILE);
	player->mhp = 5000;
	player->chp = 5000;
	for (i = 0; i < 50; i++) {
		require(make_attack_normal(pet, player));
	}
	require(player->chp < 5000);

	player->mhp = keep_mhp;
	player->chp = keep_chp;

	ok;
}

/**
 * Nor does it cast at the player.
 *
 * The same defect one layer up, and the reason the guard here is not simply
 * `!monster_is_hostile()`. In 4.2 a monster's ranged attack and a monster's
 * ranged attack *on another monster* are one code path told apart by
 * `target.midx`, so a pet that found nothing to fight this turn has no target
 * -- and "no target" means the player. A pet apprentice would blind and bolt
 * its owner without ever turning hostile.
 */
static int test_a_pet_does_not_cast_at_the_player(void *state) {
	struct monster *pet, *foe;
	int i;
	bool cast = false;
	int16_t keep_chp = player->chp;

	clear_the_level();
	pet = place_next_to_the_player("apprentice", MON_ALLEGIANCE_PET);
	require(pet);
	require(pet->target.midx == 0);

	/*
	 * Take away the free first move. Every monster is placed with MFLAG_NICE
	 * and `monster_can_cast()` refuses while it is set, so without this the
	 * test passes against no guard at all -- which is exactly what it did
	 * when it was first written.
	 */
	mflag_off(pet->mflag, MFLAG_NICE);

	/*
	 * Four hundred chances at a one-in-twelve spell. It stands next to the
	 * player with nothing else on the level, which is every condition the
	 * spell code wants -- in range, in line of sight, awake.
	 */
	for (i = 0; i < 400; i++) {
		require(!make_ranged_attack(pet));
	}
	eq(player->chp, keep_chp);

	/*
	 * Give it an enemy and it casts. This half is what stops the guard being
	 * written as "pets never use spells", which would be a quieter version of
	 * the same bug -- a pet that cannot fight is not a pet.
	 */
	foe = place_beside(pet, "kobold", MON_ALLEGIANCE_HOSTILE);
	require(foe);

	for (i = 0; i < 400 && !cast; i++) {
		mflag_off(pet->mflag, MFLAG_NICE);
		if (!monster_find_enemy(pet)) break;
		cast = make_ranged_attack(pet);
	}
	require(cast);

	ok;
}

/**
 * A pet does not break the player's rest by moving about.
 *
 * `disturb_near` is on by default, it cancels resting, and `monster_turn()`
 * fired it for any visible monster that did anything. A pet follows you, so
 * it is always visible and always doing something: a player with one pet
 * could not rest a single turn, which is how this was noticed -- the character
 * bleeding in the corridor above could not heal either.
 *
 * The option means "warn me when something moves in view". Your own animal is
 * not a warning.
 */
static int test_a_pet_does_not_break_your_rest(void *state) {
	struct monster *pet, *foe;
	int i;
	bool kept = OPT(player, disturb_near);
	bool kept_open = false, kept_bash = false;
	struct monster_race *pet_race;
	int16_t keep_chp = player->chp, keep_mhp = player->mhp;

	option_set(option_name(OPT_disturb_near), true);

	clear_the_level();
	pet = place_next_to_the_player("soldier", MON_ALLEGIANCE_PET);
	require(pet);
	require(monster_is_visible(pet));

	/*
	 * And it is not allowed to touch a door while this runs.
	 *
	 * `monster_turn()` disturbs on a bashed door -- "You hear a door burst
	 * open!" at [mon-move.c:1505](../src/mon-move.c#L1505) -- and that path
	 * has no pet exemption, so a pet that wandered into a door ended the rest
	 * and the test failed on something it is not measuring. About one run in
	 * twenty locally and three of six on the Windows runners, which is what
	 * kept master red.
	 *
	 * Cleared here rather than exempted in the game, because whether your own
	 * animal breaking down a door should wake you is a question about the game
	 * and not about this test. What this test measures is a pet *moving* in
	 * view, and a door is not that.
	 */
	pet_race = pet->race;
	kept_open = rf_has(pet_race->flags, RF_OPEN_DOOR);
	kept_bash = rf_has(pet_race->flags, RF_BASH_DOOR);
	rf_off(pet_race->flags, RF_OPEN_DOOR);
	rf_off(pet_race->flags, RF_BASH_DOOR);

	/*
	 * `player_resting_cancel(p, false)` first, every time. Placing a hostile
	 * monster in view disturbs the player, and a disturb latches a static
	 * flag that swallows the *next* attempt to rest -- so without this the
	 * second half of the test cannot start resting at all, and fails on its
	 * own setup rather than on anything it is measuring.
	 */
	player_resting_cancel(player, false);
	player_resting_set_count(player, 100);
	require(player_is_resting(player));

	for (i = 0; i < 20; i++) {
		/*
		 * A scratch, to keep it active. `monster_check_active()` measures
		 * the player -- sight, then the noise and scent maps, which a unit
		 * test never fills in because it never takes a player turn. "The
		 * monster is hurt" is the one test that does not, and using it for
		 * both halves means they differ in their side and nothing else.
		 */
		pet->hp = pet->maxhp - 1;
		pet->energy = z_info->move_energy;
		mflag_off(pet->mflag, MFLAG_HANDLED);
		process_monsters(0);
	}
	require(player_is_resting(player));

	/*
	 * And a hostile one still stops you, which is the whole point of the
	 * option. Placed fresh rather than by turning this one: twenty turns of
	 * random walk may have carried the pet out of sight, and a monster the
	 * player cannot see disturbs nobody for reasons that have nothing to do
	 * with what is being tested here.
	 */
	clear_the_level();
	foe = place_next_to_the_player("soldier", MON_ALLEGIANCE_HOSTILE);
	require(foe);
	require(monster_is_visible(foe));

	player->mhp = 5000;
	player->chp = 5000;
	player_resting_cancel(player, false);
	player_resting_set_count(player, 100);
	require(player_is_resting(player));

	for (i = 0; i < 20 && player_is_resting(player); i++) {
		player->chp = 5000;
		foe->hp = foe->maxhp - 1;
		foe->energy = z_info->move_energy;
		mflag_off(foe->mflag, MFLAG_HANDLED);
		process_monsters(0);
	}
	require(!player_is_resting(player));

	/*
	 * The race is global, so the flags go back whatever happened -- the
	 * hostile half above uses the same race as the pet.
	 */
	if (kept_open) rf_on(pet_race->flags, RF_OPEN_DOOR);
	if (kept_bash) rf_on(pet_race->flags, RF_BASH_DOOR);

	player->mhp = keep_mhp;
	player->chp = keep_chp;
	option_set(option_name(OPT_disturb_near), kept);

	ok;
}


/**
 * A fight you cannot see does not stop you resting (PLR-23, DEC-95).
 *
 * `do_mon_spell()` disturbed on every cast with no test of distance or sight,
 * and a pet is always active however far away it is -- so an animal brawling
 * in the dark on the far side of the level cancelled the player's rest, run or
 * repeated command every time it landed a spell. 64% of monsters have spells,
 * so this was most pets.
 *
 * The gate is Zangband's pair from `monst_spell_monst()`: the caster or its
 * target within sight range, *and* one of them actually visible. Three cases,
 * and the third is the one that matters most -- a spell aimed at the player
 * must interrupt whether or not they can see who cast it, because an unseen
 * caster that could be rested through is a far worse bug than the one this
 * fixes.
 *
 * Asserted on whether the rest survives, which is the thing a player notices.
 */
static int test_an_unseen_fight_does_not_break_your_rest(void *state) {
	struct monster *pet, *foe;
	int spell = RSF_ARROW;
	bool missed = false;
	int i;

	notnull(monster_spell_by_index(spell));

	/* --- a pet fighting where the player cannot see it --- */
	clear_the_level();
	pet = place_next_to_the_player("apprentice", MON_ALLEGIANCE_PET);
	require(pet);
	foe = place_beside(pet, "kobold", MON_ALLEGIANCE_HOSTILE);
	require(foe);
	pet->target.midx = foe->midx;

	/*
	 * Too tough to kill, so the only thing that can end the rest is the line
	 * under test. An arrow that killed the kobold would put a death and its
	 * message between the cast and the assertion, and either half of this
	 * test could then pass for a reason that has nothing to do with sight.
	 */
	foe->hp = foe->maxhp = 5000;

	/* Neither of them visible: `see_either` is false and nothing else counts */
	mflag_off(pet->mflag, MFLAG_VISIBLE);
	mflag_off(foe->mflag, MFLAG_VISIBLE);

	player_resting_cancel(player, false);
	player_resting_set_count(player, 100);
	require(player_is_resting(player));

	do_mon_spell(spell, pet, false);
	require(player_is_resting(player));

	/* --- the same fight, in view --- */
	mflag_on(pet->mflag, MFLAG_VISIBLE);

	player_resting_cancel(player, false);
	player_resting_set_count(player, 100);
	require(player_is_resting(player));

	do_mon_spell(spell, pet, true);
	require(!player_is_resting(player));

	/* --- and something casting at the player, unseen, always interrupts --- */
	clear_the_level();
	foe = place_next_to_the_player("apprentice", MON_ALLEGIANCE_HOSTILE);
	require(foe);
	foe->target.midx = 0;
	mflag_off(foe->mflag, MFLAG_VISIBLE);

	player->chp = player->mhp = 5000;

	/*
	 * Cast until one of them misses, and require the rest broke on *that*
	 * cast.
	 *
	 * An arrow that lands disturbs through `take_hit()`, so a hit proves
	 * nothing about this gate -- gating the player's own case wrongly would
	 * still look right most of the time. A miss applies no effect at all
	 * (`do_mon_spell()` runs `effect_do()` only `if (hits)`), so the only
	 * thing left that can end the rest is the line being measured. Unchanged
	 * hit points are how the miss is recognised.
	 *
	 * 300 tries against a floor of 5% misses leaves a one-in-five-million
	 * chance of not finding one, which `require(missed)` reports as itself
	 * rather than as a failure of the gate.
	 */
	for (i = 0; i < 300 && !missed; i++) {
		player->chp = player->mhp;
		player_resting_cancel(player, false);
		player_resting_set_count(player, 100);
		require(player_is_resting(player));

		do_mon_spell(spell, foe, false);
		missed = (player->chp == player->mhp);
	}
	require(missed);
	require(!player_is_resting(player));

	ok;
}

/**
 * A pet penned behind a closed door, with the player on the other side.
 *
 * Both halves of the pen matter. The walls leave the pet one grid it could
 * move to, and that grid is the door -- otherwise a pet with nothing to fight
 * wanders, and `get_move_random()` only ever picks a grid that is already
 * walkable, so it would never touch the door at all.
 *
 * Returns NULL if the level will not take the arrangement, so the caller can
 * say so where it is written rather than failing here.
 */
static struct monster *pen_a_pet_behind_a_door(struct loc *door) {
	int i;

	for (i = 0; i < 8; i++) {
		struct loc mid = loc_sum(player->grid, ddgrid_ddd[i]);
		struct loc far = loc_sum(mid, ddgrid_ddd[i]);
		struct monster *pet;
		int j;

		if (!square_in_bounds_fully(cave, mid)) continue;
		if (!square_in_bounds_fully(cave, far)) continue;
		if (!square_isempty(cave, mid)) continue;
		if (!square_isempty(cave, far)) continue;

		pet = place_at(far, "soldier", MON_ALLEGIANCE_PET);
		if (!pet) continue;

		for (j = 0; j < 8; j++) {
			struct loc adj = loc_sum(far, ddgrid_ddd[j]);

			if (!square_in_bounds_fully(cave, adj)) continue;
			if (loc_eq(adj, mid)) continue;
			square_set_feat(cave, adj, FEAT_GRANITE);
		}

		square_set_feat(cave, mid, FEAT_CLOSED);
		square_set_door_lock(cave, mid, 0);
		*door = mid;

		player->upkeep->update |= (PU_UPDATE_VIEW | PU_MONSTERS);
		update_stuff(player);
		return pet;
	}

	return NULL;
}

/**
 * And a pet bursting through a door does not stop you either (DEC-95).
 *
 * The `disturb()` on a bashed door was upstream Angband's, written for a game
 * where everything that bashes a door is hostile. Zangband has pets and does
 * not disturb on a bash at all. The message stays -- you did hear it -- and
 * the interruption goes.
 *
 * Driven through `process_monsters()` rather than by calling
 * `square_smash_door()` directly, because the line under test is in the
 * monster turn and not in the door code: a test that smashed the door itself
 * would pass whether the `disturb()` were there or not.
 */
static int test_a_pet_bursting_a_door_does_not_break_your_rest(void *state) {
	struct monster *pet = NULL;
	struct monster_race *pet_race;
	struct loc door = loc(0, 0);
	bool kept_open = false, kept_bash = false;
	int16_t kept_leash = player->pet_follow_distance;
	int attempt, i;

	for (attempt = 0; attempt < 40 && !pet; attempt++) {
		clear_the_level();
		pet = pen_a_pet_behind_a_door(&door);
		if (!pet) {
			prepare_next_level(player);
			on_new_level();
		}
	}
	require(pet);

	/* Bashing, not opening, so the outcome is the one being measured */
	pet_race = pet->race;
	kept_open = rf_has(pet_race->flags, RF_OPEN_DOOR);
	kept_bash = rf_has(pet_race->flags, RF_BASH_DOOR);
	rf_off(pet_race->flags, RF_OPEN_DOOR);
	rf_on(pet_race->flags, RF_BASH_DOOR);

	/* Close enough that the pet wants to come back to the player */
	player->pet_follow_distance = 1;

	/*
	 * Two grids of the noise map, by hand.
	 *
	 * `get_move_advance()` finds a door by hearing something quieter on the
	 * other side of it, and the map it reads is only written by
	 * `make_noise()` on a player turn -- which a unit test never takes, so it
	 * is all zeroes and the pet stands still. These are the two values
	 * `make_noise()` would have left for this geometry: 1 at the door, which
	 * is next to the player, and 2 where the pet is, one further out. Noise
	 * is not recomputed while the player rests, so what is written here is
	 * what the pet reads for the whole test, exactly as in play.
	 */
	cave->noise.grids[door.y][door.x] = 1;
	cave->noise.grids[pet->grid.y][pet->grid.x] = 2;

	player_resting_cancel(player, false);
	player_resting_set_count(player, 100);
	require(player_is_resting(player));

	for (i = 0; i < 50 && !square_isbrokendoor(cave, door); i++) {
		/* Hurt, to keep it active -- see the note in the test above */
		pet->hp = pet->maxhp - 1;
		pet->energy = z_info->move_energy;
		mflag_off(pet->mflag, MFLAG_HANDLED);
		process_monsters(0);
	}

	if (kept_open) rf_on(pet_race->flags, RF_OPEN_DOOR);
	if (!kept_bash) rf_off(pet_race->flags, RF_BASH_DOOR);
	player->pet_follow_distance = kept_leash;

	require(square_isbrokendoor(cave, door));
	require(player_is_resting(player));

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
	{ "pets-follow-you-downstairs", test_pets_follow_you_downstairs },
	{ "a-pet-does-not-attack-the-player",
	  test_a_pet_does_not_attack_the_player },
	{ "a-pet-does-not-cast-at-the-player",
	  test_a_pet_does_not_cast_at_the_player },
	{ "an-unseen-fight-does-not-break-your-rest",
	  test_an_unseen_fight_does_not_break_your_rest },
	{ "a-pet-bursting-a-door-does-not-break-your-rest",
	  test_a_pet_bursting_a_door_does_not_break_your_rest },
	{ "a-pet-does-not-break-your-rest",
	  test_a_pet_does_not_break_your_rest },
	{ NULL, NULL }
};
