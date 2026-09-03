/* monster/anger
 *
 * What turns an ally against you, and what stops you turning on it by accident
 * (ZangbandTK, PLR-24, PLR-33).
 *
 * These are one mechanism seen from two sides. PLR-33 is the documented
 * counterweight to pets -- "pets are also rather easily irritable and once you
 * do something which causes the slightest discomfort to them, they will revert
 * to their normal behavior and consider you their main target", with the
 * warning to "think about [this] before lighting up a room if you have pet
 * orcs". PLR-24 is what keeps that from firing on every step.
 *
 * The interesting finding is that PLR-24's "confirmation before harming a pet"
 * is not what Zangband does, and what it does instead is better: walking into
 * something on your side **changes places with it**. The danger was never that
 * a player would decide to punch their own animal; it is that the animal steps
 * into the corridor mouth on the turn the player was walking that way. A prompt
 * on every such step would appear constantly -- pets follow you -- and train
 * the player to dismiss it unread.
 *
 * A player who is not in command of themselves gets no such courtesy, and that
 * list is Zangband's: confused, hallucinating, stunned, berserk, or unable to
 * see what is there.
 */

#include "unit-test.h"
#include "test-utils.h"

#include "cave.h"
#include "cmd-core.h"
#include "game-world.h"
#include "generate.h"
#include "init.h"
#include "cmds.h"
#include "mon-make.h"
#include "mon-move.h"
#include "mon-predicate.h"
#include "mon-util.h"
#include "monster.h"
#include "player-birth.h"
#include "player-timed.h"
#include "player-util.h"
#include "player-virtue.h"

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

static void clear_the_level(void) {
	int i;

	for (i = 1; i < cave_monster_max(cave); i++) {
		if (cave_monster(cave, i)->race) delete_monster_idx(cave, i);
	}
}

/** A pet of that race in a grid next to the player, or NULL. */
static struct monster *pet_beside_player(const char *name) {
	struct monster_group_info info = { 0, 0 };
	struct monster_race *race = lookup_monster(name);
	int i;

	if (!race) return NULL;

	for (i = 0; i < 8; i++) {
		struct loc grid = loc_sum(player->grid, ddgrid_ddd[i]);

		if (!square_in_bounds_fully(cave, grid)) continue;
		if (!square_isempty(cave, grid)) continue;
		if (!place_new_monster(cave, grid, race, false, false, info,
							   ORIGIN_DROP)) continue;
		monster_set_allegiance(square_monster(cave, grid),
							   MON_ALLEGIANCE_PET);
		return square_monster(cave, grid);
	}

	return NULL;
}

/** Which direction the monster lies in from the player. */
static int direction_to(struct monster *mon) {
	int i;

	for (i = 0; i < 8; i++) {
		if (loc_eq(loc_sum(player->grid, ddgrid_ddd[i]), mon->grid)) {
			return ddd[i];
		}
	}

	return 0;
}

/**
 * Hurting a pet turns it hostile.
 *
 * Through `mon_take_hit()`, which is the one place all player-caused damage
 * arrives: melee, missiles and every projection. Zangband had to write its
 * anger call at each site because its damage path had no such choke point, and
 * the sites it missed are why a player there could drop a wall on a pet for
 * nothing.
 */
static int test_hurting_a_pet_angers_it(void *state) {
	struct monster *pet;
	bool fear = false;

	clear_the_level();
	pet = pet_beside_player("soldier");
	require(pet);
	require(monster_is_pet(pet));

	(void) mon_take_hit(pet, player, 1, &fear, NULL);

	require(monster_is_hostile(pet));

	ok;
}

/**
 * A blow that does nothing does not.
 *
 * `mon_take_hit()` returns early on zero damage, and the anger sits after that
 * return on purpose: a resisted breath or a spell the monster shrugged off is
 * not "the slightest discomfort", it is nothing happening.
 */
static int test_a_harmless_hit_does_not(void *state) {
	struct monster *pet;
	bool fear = false;

	clear_the_level();
	pet = pet_beside_player("soldier");
	require(pet);

	(void) mon_take_hit(pet, player, 0, &fear, NULL);

	require(monster_is_pet(pet));

	ok;
}

/**
 * A monster killed by another monster does not anger the player's pets.
 *
 * The other half of the choke point: `mon_take_nonplayer_hit()` is a separate
 * function and must stay separate, or a pet caught in a hostile monster's
 * fireball would blame the player.
 */
static int test_another_monsters_damage_is_not_your_fault(void *state) {
	struct monster *pet;

	clear_the_level();
	pet = pet_beside_player("soldier");
	require(pet);

	mon_take_nonplayer_hit(1, pet, MON_MSG_NONE, MON_MSG_DIE);

	require(monster_is_pet(pet));

	ok;
}

/**
 * Turning on you costs you virtue.
 *
 * Zangband's own four writes, and they are pointed: a creature that trusted
 * you no longer does, which is a gain in Individualism and a loss in Honour,
 * Justice and Compassion. Dead numbers in Zangband, which had no consumer for
 * any virtue; live ones here since PLR-21.
 *
 * Only the virtues this character actually tracks can move -- a character has
 * eight of the eighteen (PLR-18) -- so this asks that at least one did and
 * that none moved the wrong way.
 */
static int test_anger_moves_the_virtues(void *state) {
	struct monster *pet;
	bool fear = false;
	int individualism, honour, moved = 0;

	clear_the_level();
	pet = pet_beside_player("soldier");
	require(pet);

	individualism = virtue_value(player, V_INDIVIDUALISM);
	honour = virtue_value(player, V_HONOUR);

	(void) mon_take_hit(pet, player, 1, &fear, NULL);

	if (player_has_virtue(player, V_INDIVIDUALISM)) {
		require(virtue_value(player, V_INDIVIDUALISM) > individualism);
		moved++;
	}
	if (player_has_virtue(player, V_HONOUR)) {
		require(virtue_value(player, V_HONOUR) < honour);
		moved++;
	}

	/* A Warrior tracks Honour, so at least one of the two must have moved */
	require(moved > 0);

	ok;
}

/**
 * Walking into a pet changes places with it.
 *
 * The step that would otherwise start a fight with your own animal every time
 * it gets in a doorway.
 */
static int test_walking_into_a_pet_swaps(void *state) {
	struct monster *pet;
	struct loc was_player, was_pet;
	int dir;

	clear_the_level();
	pet = pet_beside_player("soldier");
	require(pet);

	dir = direction_to(pet);
	require(dir != 0);

	was_player = player->grid;
	was_pet = pet->grid;

	move_player(dir, false);

	require(loc_eq(player->grid, was_pet));
	require(loc_eq(pet->grid, was_player));
	require(monster_is_pet(pet));

	ok;
}

/**
 * A confused player hits it instead.
 *
 * The exception is not decoration: it is what keeps confusion dangerous when
 * you have a stable.
 *
 * The assertion is that the two did **not** trade places -- attacking leaves
 * the player where they were. Written the obvious way first ("they are not on
 * the same grid", "one of them moved") it asserted things that are true after
 * a swap as well, and removing the confusion guard from the code did not fail
 * it. That is the test being wrong rather than the code being right, and it
 * only came out because the falsification was run.
 *
 * `move_player()` goes exactly where it is told; 4.2 randomises a confused
 * step in the command layer above this, so the direction here is not a lottery.
 */
static int test_a_confused_player_does_not_swap(void *state) {
	struct monster *pet;
	struct loc was_player;
	int dir;

	clear_the_level();
	pet = pet_beside_player("soldier");
	require(pet);

	dir = direction_to(pet);
	require(dir != 0);
	was_player = player->grid;

	player_set_timed(player, TMD_CONFUSED, 20, false, false);
	move_player(dir, false);
	player_set_timed(player, TMD_CONFUSED, 0, false, false);

	/* They did not trade places: an attack leaves the player standing still */
	require(loc_eq(player->grid, was_player));
	require(!loc_eq(pet->grid, was_player));

	ok;
}

/**
 * Walking into something hostile still attacks it.
 *
 * The half a permissive swap gets wrong. Without this, an implementation that
 * swaps with everything passes every test above and makes the player unable to
 * fight anything at all.
 */
static int test_walking_into_an_enemy_still_attacks(void *state) {
	struct monster_group_info info = { 0, 0 };
	struct monster_race *race;
	struct monster *foe = NULL;
	struct loc was_player;
	int i, dir, before;

	clear_the_level();
	race = lookup_monster("soldier");
	require(race);

	for (i = 0; i < 8 && !foe; i++) {
		struct loc grid = loc_sum(player->grid, ddgrid_ddd[i]);

		if (!square_in_bounds_fully(cave, grid)) continue;
		if (!square_isempty(cave, grid)) continue;
		if (!place_new_monster(cave, grid, race, false, false, info,
							   ORIGIN_DROP)) continue;
		foe = square_monster(cave, grid);
	}
	require(foe);
	require(monster_is_hostile(foe));

	dir = direction_to(foe);
	require(dir != 0);
	was_player = player->grid;
	before = foe->hp;

	/* Swing until something lands; a Warrior does not miss twenty times */
	for (i = 0; i < 20 && foe->race && foe->hp >= before; i++) {
		move_player(dir, false);
	}

	/* The player did not move, and the monster is worse off */
	require(loc_eq(player->grid, was_player));
	require(!foe->race || foe->hp < before);

	ok;
}

/**
 * Aggravation turns them against you, awake or asleep.
 *
 * 4.2 reads ``OF_AGGRAVATE`` in `monster_reduce_sleep()`, which only sleeping
 * monsters reach -- and since PLR-23 no ally is ever asleep, so the rule would
 * have been unreachable for exactly the monsters it is about. Checked here in
 * the monster's turn, where Zangband checks it.
 */
static int test_aggravation_turns_them(void *state) {
	struct monster *pet;
	int i;

	clear_the_level();
	pet = pet_beside_player("soldier");
	require(pet);

	/* Aggravate, without an item: the flag is what the code reads */
	of_on(player->state.flags, OF_AGGRAVATE);

	for (i = 0; i < 5 && monster_is_pet(pet); i++) {
		int j;

		for (j = 1; j < cave_monster_max(cave); j++) {
			struct monster *mon = cave_monster(cave, j);

			if (!mon->race) continue;
			mflag_off(mon->mflag, MFLAG_HANDLED);
			mon->energy = z_info->move_energy;
		}
		process_monsters(0);
	}

	of_off(player->state.flags, OF_AGGRAVATE);

	require(monster_is_hostile(pet));

	ok;
}

const char *suite_name = "monster/anger";
struct test tests[] = {
	{ "hurting-a-pet-angers-it", test_hurting_a_pet_angers_it },
	{ "a-harmless-hit-does-not", test_a_harmless_hit_does_not },
	{ "another-monsters-damage-is-not-your-fault",
	  test_another_monsters_damage_is_not_your_fault },
	{ "anger-moves-the-virtues", test_anger_moves_the_virtues },
	{ "walking-into-a-pet-swaps", test_walking_into_a_pet_swaps },
	{ "a-confused-player-does-not-swap",
	  test_a_confused_player_does_not_swap },
	{ "walking-into-an-enemy-still-attacks",
	  test_walking_into_an_enemy_still_attacks },
	{ "aggravation-turns-them", test_aggravation_turns_them },
	{ NULL, NULL }
};
