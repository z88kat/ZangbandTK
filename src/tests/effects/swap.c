/*
 * effects/swap
 *
 * Trading places, and stopping the breeding (ZangbandTK, PLR-16).
 *
 * Two mutation powers that M8 deferred with the note "deferred to M9, not
 * open-ended", built here. Neither is complicated; both are easy to get subtly
 * wrong in ways a count of working powers would not notice, which is why they
 * are exercised rather than merely present.
 */

#include "unit-test.h"
#include "test-utils.h"

#include "cave.h"
#include "effects.h"
#include "game-world.h"
#include "init.h"
#include "mon-make.h"
#include "monster.h"
#include "player-birth.h"
#include "player-util.h"

int setup_tests(void **state) {
	set_file_paths();
	if (!init_angband()) return 1;
#ifdef UNIX
	create_needed_dirs();
#endif
	if (!player_make_simple(NULL, NULL, "Tester")) {
		cleanup_angband();
		return 1;
	}
	(void) test_seed_rng_reported(suite_name);
	return 0;
}

int teardown_tests(void *state) {
	cleanup_angband();
	return 0;
}

/** A walled box of floor, big enough to stand two things apart in. */
static struct chunk *empty_cave(int height, int width) {
	struct chunk *c = cave_new(height, width);
	struct loc grid;

	for (grid.y = 0; grid.y < height; grid.y++) {
		for (grid.x = 0; grid.x < width; grid.x++) {
			bool edge = (grid.y == 0 || grid.x == 0
						 || grid.y == height - 1 || grid.x == width - 1);

			square_set_feat(c, grid, edge ? FEAT_PERM : FEAT_FLOOR);
		}
	}

	return c;
}

static void fresh_level(void) {
	character_dungeon = false;
	player->depth = 1;
	if (cave) cave_free(cave);
	if (player->cave) cave_free(player->cave);
	cave = empty_cave(11, 15);
	cave->depth = player->depth;
	player->cave = cave_new(cave->height, cave->width);
	player->cave->depth = cave->depth;
}

/** The first race with the flag set as asked, and not a breeder or unique. */
static struct monster_race *a_race(bool res_tele) {
	int i;

	for (i = 1; i < z_info->r_max; i++) {
		struct monster_race *race = &r_info[i];

		if (!race->name) continue;
		if (rf_has(race->flags, RF_UNIQUE)) continue;
		if (rf_has(race->flags, RF_RES_TELE) != res_tele) continue;

		return race;
	}

	return NULL;
}

static struct monster *put_monster(struct monster_race *race, struct loc grid) {
	struct monster_group_info info = { 0, 0 };

	if (!place_new_monster(cave, grid, race, false, false, info,
						   ORIGIN_DROP_WIZARD)) {
		return NULL;
	}

	return square_monster(cave, grid);
}

static bool fire(int index, int dir, const char *dice_string) {
	struct effect effect = { 0 };
	dice_t *dice = NULL;
	bool ident = false, used;

	effect.index = index;
	if (dice_string) {
		dice = dice_new();
		dice_parse_string(dice, dice_string);
		effect.dice = dice;
	}

	used = effect_do(&effect, source_player(), NULL, &ident, true, dir, 0, 0,
					 NULL);
	if (dice) dice_free(dice);

	return used;
}

/**
 * Swap position exchanges the two, rather than moving either one.
 *
 * The obvious wrong implementation is `TELEPORT_TO`, which brings the monster
 * to the player and leaves the player where they are -- two things in one
 * square. So both grids are checked afterwards, not just the player's: an
 * implementation that moved only the caster would satisfy half of this and
 * fail the other half.
 */
static int test_swap_position_trades_places(void *state) {
	struct monster_race *race = a_race(false);
	struct loc pgrid, mgrid;
	struct monster *mon;

	notnull(race);
	fresh_level();

	pgrid = loc(5, 5);
	mgrid = loc(6, 5);
	player_place(cave, player, pgrid);
	mon = put_monster(race, mgrid);
	notnull(mon);

	require(fire(EF_SWAP_POS, DIR_E, NULL));

	/* Each is now where the other was. */
	require(loc_eq(player->grid, mgrid));
	require(loc_eq(mon->grid, pgrid));
	require(square_monster(cave, pgrid) == mon);
	null(square_monster(cave, mgrid));

	ok;
}

/**
 * A monster that resists teleportation stays where it is, and so do you.
 *
 * Zangband refuses outright here rather than rolling a saving throw, which is
 * different from how RES_TELE works against teleport-other, and this pins the
 * difference. The second half matters as much as the first: a refusal that
 * still moved the player would leave them standing in a monster.
 */
static int test_a_resister_will_not_be_traded(void *state) {
	struct monster_race *race = a_race(true);
	struct loc pgrid, mgrid;
	struct monster *mon;

	notnull(race);
	fresh_level();

	pgrid = loc(5, 5);
	mgrid = loc(6, 5);
	player_place(cave, player, pgrid);
	mon = put_monster(race, mgrid);
	notnull(mon);

	require(!fire(EF_SWAP_POS, DIR_E, NULL));

	require(loc_eq(player->grid, pgrid));
	require(loc_eq(mon->grid, mgrid));

	ok;
}

/**
 * An empty square is refused rather than swapped with.
 */
static int test_an_empty_square_is_not_a_partner(void *state) {
	struct loc pgrid = loc(5, 5);

	fresh_level();
	player_place(cave, player, pgrid);

	require(!fire(EF_SWAP_POS, DIR_E, NULL));
	require(loc_eq(player->grid, pgrid));

	ok;
}

/**
 * Sterilize pushes the breeder count past the ceiling that gates breeding.
 *
 * Zangband adds `MAX_REPRO` to the count rather than setting a flag, and the
 * consequence is that this is temporary: breeding resumes once enough breeders
 * die to bring the count back under. So the assertion is on the relationship
 * between the count and `repro_monster_max` -- which is what `mon-move.c`
 * actually tests -- and not on any particular number.
 */
static int test_sterilize_stops_the_breeding(void *state) {
	int before;

	fresh_level();
	player_place(cave, player, loc(5, 5));

	before = cave->num_repro;
	require(before < z_info->repro_monster_max);

	require(fire(EF_STERILIZE, 0, NULL));

	/* This is the comparison `mon-move.c` makes before a breeder multiplies. */
	require(cave->num_repro >= z_info->repro_monster_max);
	eq(cave->num_repro, before + z_info->repro_monster_max);

	ok;
}

/**
 * And it costs between seventeen and thirty-four hit points.
 *
 * `16+1d18` is 17 to 34, which is Zangband's `rand_range(17, 34)`. Both ends
 * are checked over enough rolls to reach them, and the bounds are checked as
 * bounds -- a chain that dropped the damage effect would pass a test that only
 * looked at the average.
 */
static int test_the_headache_costs_what_it_costs(void *state) {
	int i, lowest = 1000, highest = 0;

	fresh_level();
	player_place(cave, player, loc(5, 5));

	for (i = 0; i < 200; i++) {
		int taken;

		player->chp = player->mhp = 500;
		require(fire(EF_DAMAGE, 0, "16+1d18"));

		taken = 500 - player->chp;
		require(taken >= 17);
		require(taken <= 34);
		if (taken < lowest) lowest = taken;
		if (taken > highest) highest = taken;
	}

	/*
	 * Both ends reached. With 200 rolls over 18 values, missing either end
	 * has probability (17/18)^200, about one in 90,000.
	 */
	eq(lowest, 17);
	eq(highest, 34);

	player->chp = player->mhp;

	ok;
}

const char *suite_name = "effects/swap";
struct test tests[] = {
	{ "swap-position-trades-places", test_swap_position_trades_places },
	{ "a-resister-will-not-be-traded", test_a_resister_will_not_be_traded },
	{ "an-empty-square-is-not-a-partner",
	  test_an_empty_square_is_not_a_partner },
	{ "sterilize-stops-the-breeding", test_sterilize_stops_the_breeding },
	{ "the-headache-costs-what-it-costs",
	  test_the_headache_costs_what_it_costs },
	{ NULL, NULL }
};
