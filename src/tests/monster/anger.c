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
 *
 * The polymorph tests at the end are the same subject from the third side: a
 * thing that replaces a monster wholesale, and therefore a way to lose an ally
 * without ever deciding to. Zangband preserved the attitude through a
 * polymorph deliberately, because turning your own pet into something better
 * is one of the things the feature is for.
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

static void clear_the_level(void) {
	int i;

	for (i = 1; i < cave_monster_max(cave); i++) {
		if (cave_monster(cave, i)->race) delete_monster_idx(cave, i);
	}
}

/** A monster of that race on that side, in a grid next to the player. */
static struct monster *place_side(const char *name,
								  enum monster_allegiance side) {
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
		monster_set_allegiance(square_monster(cave, grid), side);
		return square_monster(cave, grid);
	}

	return NULL;
}

/** A pet of that race in a grid next to the player, or NULL. */
static struct monster *pet_beside_player(const char *name) {
	return place_side(name, MON_ALLEGIANCE_PET);
}

/**
 * Polymorph whatever stands at `grid`, and hand back what stands there after.
 *
 * NULL when the shape did not change -- `poly_race()` gives up after a
 * thousand draws and returns the race it was given, and `place_new_monster()`
 * can fail after the old monster has already been deleted -- so a caller that
 * wants to inspect a *polymorphed* monster has to be told the difference
 * between "changed" and "nothing happened". The power is well past what a
 * shallow monster saves against: the throw is `randint1(power - 10) + 10`
 * against the monster's level, so 100 against a soldier never fails and these
 * tests never turn on the roll.
 */
static struct monster *poly_at(struct loc grid) {
	struct monster *before = square_monster(cave, grid);
	struct monster *after;
	struct monster_race *was;

	if (!before) return NULL;
	was = before->race;

	(void) project(source_player(), 0, grid, 100, PROJ_MON_POLY,
				   PROJECT_STOP | PROJECT_KILL | PROJECT_PLAY, 0, 0, NULL);

	after = square_monster(cave, grid);
	if (!after || after->race == was) return NULL;

	return after;
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

/**
 * A polymorphed pet is still a pet.
 *
 * The mechanic Zangband is remembered for -- summon an animal, take it deep,
 * polymorph it and see what it becomes -- and it turns on this one line.
 * `poly_race()` aims at `(depth + monster level) / 2 + 5`, so a weak pet on a
 * deep level comes back stronger, which is the whole point of doing it.
 *
 * A polymorph is one monster to the player and two to the code: the old one is
 * deleted and a new one placed, and anything not carried across that seam is
 * lost. Zangband read the attitude before the delete and passed it to
 * `place_monster_aux()` afterwards
 * ([spells3.c:4368](../archive/zangband/src/spells3.c#L4368)); this does the
 * same thing at the same two points.
 *
 * Three polymorphs rather than one because the failure being guarded against
 * is a field silently not copied, and one success is one draw from
 * `poly_race()` -- a race that happened to be placed a particular way could
 * pass by luck.
 */
static int test_a_polymorphed_pet_is_still_a_pet(void *state) {
	int t, polys = 0;

	for (t = 0; t < 20 && polys < 3; t++) {
		struct monster *pet, *now;
		struct loc grid;

		clear_the_level();
		pet = pet_beside_player("soldier");
		if (!pet) continue;
		grid = pet->grid;

		now = poly_at(grid);
		if (!now) continue;

		require(monster_is_pet(now));
		polys++;
	}

	eq(polys, 3);

	ok;
}

/**
 * And a polymorphed friendly monster is still friendly.
 *
 * The three sides are a partition, not a boolean, so a fix that reads "was it
 * a pet, make it a pet again" passes the test above and quietly drops the
 * middle state -- a friendly creature would come back hostile. Carrying the
 * field rather than the answer to a question about it is what makes both
 * true, and this is the test that says so.
 */
static int test_a_polymorphed_friend_is_still_a_friend(void *state) {
	int t, polys = 0;

	for (t = 0; t < 20 && polys < 3; t++) {
		struct monster *friend, *now;
		struct loc grid;

		clear_the_level();
		friend = place_side("soldier", MON_ALLEGIANCE_FRIENDLY);
		if (!friend) continue;
		grid = friend->grid;

		now = poly_at(grid);
		if (!now) continue;

		require(monster_is_friendly(now));
		polys++;
	}

	eq(polys, 3);

	ok;
}

/**
 * A chaos attack that polymorphs a pet still turns it against you.
 *
 * The two rules meet here and the order is what keeps them apart. A direct
 * polymorph does no damage -- `MON_POLY` zeroes it -- so nothing angers the
 * pet and it keeps its side. Chaos *does* damage, and damage runs before the
 * side effects in `project_m()`, so the pet is already hostile by the time the
 * shape changes and the side carried across is the one anger just wrote.
 *
 * Which means the preserved allegiance cannot be used to launder an attack:
 * dropping a chaos ball on your own animal does not become free by virtue of
 * it polymorphing. Read the side at the top of the side-effect handler and
 * both rules hold; read it any earlier and this test fails.
 */
static int test_a_chaos_poly_still_angers(void *state) {
	int t, hits = 0;

	for (t = 0; t < 20 && hits < 3; t++) {
		struct monster *pet, *now;
		struct loc grid;

		clear_the_level();
		pet = pet_beside_player("soldier");
		if (!pet) continue;
		grid = pet->grid;

		(void) project(source_player(), 0, grid, 5, PROJ_CHAOS,
					   PROJECT_STOP | PROJECT_KILL | PROJECT_PLAY, 0, 0, NULL);

		/* NULL if it died of the damage, or the new shape failed to place */
		now = square_monster(cave, grid);
		if (!now) continue;

		require(monster_is_hostile(now));
		hits++;
	}

	eq(hits, 3);

	ok;
}

/**
 * A polymorph never leaves an empty square, and never merely fizzles.
 *
 * 4.2 deletes the monster and then places the new shape, and never asks
 * whether the placing worked. When it did not, the monster was simply gone --
 * a polymorph that sometimes worked as a deletion, and for a pet a way to lose
 * one with no message at all, which is the silence the tests above are about.
 *
 * Measured before it was fixed, over two thousand casts at each depth: 0.2 per
 * cent in the town, 1.2 at fifteen, and 3.5 per cent from thirty down. The
 * curve is the bestiary's -- the deep fish are krakens, whales and Charybdis
 * and they sit squarely in the band a deep polymorph draws from, where the
 * shallow ones are a handful of piranha a shallow draw rarely reaches. So this
 * goes down to thirty: at the town's rate, a few hundred casts would find
 * nothing better than half the time, and a regression test that only sometimes
 * regresses is not one.
 *
 * A soldier rather than something deep because the *saving throw* is the other
 * way to leave a monster unchanged -- ``randint1(power - 10) + 10``, which
 * bottoms out at 11 whatever the power -- so anything above level 11 saves
 * sometimes and muddies the third count below. Nothing of level 2 ever saves.
 *
 * Three counts, because there are three ways to get this wrong and they are
 * not the same mistake. Measured over 1500 casts in this exact setup:
 *
 * - ``vanished`` is the defect. 18 before the change, 0 after.
 * - ``fish`` is the mechanism, and stays 0 either way once the monster cannot
 *   vanish -- it is here to say the square is occupied by something that
 *   *belongs* there rather than by luck.
 * - ``unchanged`` is what separates the fix from the near miss. Keep only the
 *   fallback and drop the filter in `poly_race()` and nothing vanishes, so the
 *   first count goes quiet -- but 25 of those 1500 casts become a spell that
 *   visibly does nothing, because the shape was drawn, refused, and the old
 *   one put back. With the filter it is 0 -- measured over five thousand
 *   casts, so the assertion is exact rather than a threshold. That is what
 *   says which half of this change is load-bearing.
 */
static int test_a_polymorph_never_empties_the_square(void *state) {
	int was = player->depth;
	int t, tries, cast = 0, vanished = 0, fish = 0, unchanged = 0;
	bool room = false;

	/*
	 * Down to thirty, and onto a level with somewhere to stand.
	 *
	 * The generator will sometimes start the character in a doorway with
	 * seven walls round it, and `place_side()` wants an empty *floor* grid --
	 * an open door is passable and is not floor -- so on about one level in
	 * twenty there is nowhere to put anything and the loop below measures
	 * nothing at all. Generating one level and trusting it cost a 12 per cent
	 * flake rate, and the failure was `cast` collapsing to zero rather than
	 * anything to do with polymorph. The other tests in this suite use the
	 * same helper and do not need this, because they stay in the town, where
	 * the character starts in the open.
	 */
	for (tries = 0; tries < 20 && !room; tries++) {
		player->depth = 30;
		prepare_next_level(player);
		on_new_level();
		clear_the_level();
		room = (pet_beside_player("soldier") != NULL);
	}

	for (t = 0; t < 600 && room; t++) {
		struct monster *mon, *now;
		struct monster_race *was_race;
		struct loc grid;

		clear_the_level();
		mon = pet_beside_player("soldier");
		if (!mon) continue;
		grid = mon->grid;
		was_race = mon->race;

		/* The rule is about dry land; a fish in water is welcome to stay */
		if (square_iswater(cave, grid)) continue;
		cast++;

		(void) project(source_player(), 0, grid, 100, PROJ_MON_POLY,
					   PROJECT_STOP | PROJECT_KILL | PROJECT_PLAY, 0, 0, NULL);

		now = square_monster(cave, grid);
		if (!now) {
			vanished++;
			continue;
		}
		if (monster_is_aquatic(now->race)) fish++;
		if (now->race == was_race) unchanged++;
	}

	/* Put the character back where the suite left it, before asserting */
	player->depth = was;
	prepare_next_level(player);
	on_new_level();

	require(room);
	require(cast > 500);
	eq(vanished, 0);
	eq(fish, 0);
	eq(unchanged, 0);

	ok;
}

/**
 * And the rule underneath it, asked directly.
 *
 * The test above is a rate and this is a certainty, which is the right
 * division of labour: three hundred casts say the defect is gone, and this
 * says what stops it coming back. `monster_race_fits_grid()` is the one
 * definition of "can this race be here" -- `place_new_monster_one()` obeys it
 * and `poly_race()` consults it, so a change to either cannot silently
 * disagree with the other.
 *
 * Both directions, because half of it is the interesting half: a fish is
 * refused on the floor *and accepted in the water*. A filter that simply never
 * returned a fish would pass the first assertion and quietly remove two dozen
 * races from every polymorph cast at sea.
 */
static int test_a_fish_belongs_in_water(void *state) {
	struct monster_race *fish = lookup_monster("giant piranha");
	struct monster_race *land = lookup_monster("soldier");
	struct loc grid;
	int was;

	require(fish && land);
	require(monster_is_aquatic(fish));
	require(!monster_is_aquatic(land));

	clear_the_level();
	grid = loc_sum(player->grid, ddgrid_ddd[0]);
	require(square_in_bounds_fully(cave, grid));
	was = square(cave, grid)->feat;

	square_set_feat(cave, grid, FEAT_FLOOR);
	require(!monster_race_fits_grid(cave, grid, fish));
	require(monster_race_fits_grid(cave, grid, land));

	square_set_feat(cave, grid, FEAT_WATER);
	require(monster_race_fits_grid(cave, grid, fish));
	require(monster_race_fits_grid(cave, grid, land));

	square_set_feat(cave, grid, was);

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
	{ "a-polymorphed-pet-is-still-a-pet",
	  test_a_polymorphed_pet_is_still_a_pet },
	{ "a-polymorphed-friend-is-still-a-friend",
	  test_a_polymorphed_friend_is_still_a_friend },
	{ "a-chaos-poly-still-angers", test_a_chaos_poly_still_angers },
	{ "a-polymorph-never-empties-the-square",
	  test_a_polymorph_never_empties_the_square },
	{ "a-fish-belongs-in-water", test_a_fish_belongs_in_water },
	{ NULL, NULL }
};
