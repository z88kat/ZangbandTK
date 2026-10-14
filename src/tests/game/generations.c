/* game/generations.c
 *
 * Many levels made in one process.
 *
 * The phase 2 plan once carried a "known defect": a segmentation fault at
 * about six hundred level generations, found by game/carry when its samples
 * grew.  It was not a count.  game/carry's go_down() adds one to the depth
 * and never stops, and below the bottom of the world no greater vault is
 * allowed, so the first hard-centre level made down there dereferenced the
 * NULL that random_vault() returned.  Fifteen thousand levels at depths the
 * game can reach made no crash and held the same memory throughout.
 *
 * So these defend three things: every depth the game has can be made; no way
 * down goes below the bottom; and a hard centre with no vault gives up and
 * lets the generator choose again rather than crashing.  And a long run of
 * levels leaves nothing stored behind it.
 */

#include "unit-test.h"
#include "test-utils.h"

#include "cave.h"
#include "game-world.h"
#include "generate.h"
#include "init.h"
#include "mon-make.h"
#include "player-birth.h"
#include "player-util.h"

static void println(const char *str) {
	(void) fputs(str, stdout);
	(void) fputc('\n', stdout);
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

/** Make the level at a depth, as taking a staircase there does. */
static void arrive(int depth) {
	player->depth = depth;
	prepare_next_level(player);
	on_new_level();
}

static int test_every_depth_makes_a_level(void *state) {
	int depth;

	for (depth = 0; depth < z_info->max_depth; depth++) {
		arrive(depth);
		notnull(cave);
		eq(cave->depth, depth);
	}
	ok;
}

static int test_no_way_down_goes_below_the_bottom(void *state) {
	int bottom = z_info->max_depth - 1;
	int dungeon = player->dungeon;

	/* The world's own floor, outside any one dungeon's */
	player->dungeon = 0;
	eq(dungeon_get_next_level(player, bottom, 1), bottom);
	eq(dungeon_get_next_level(player, bottom - 2, 5), bottom);
	require(nightmare_recall_depth(player, bottom) <= bottom);
	require(nightmare_recall_depth(player, 99) <= bottom);
	player->dungeon = dungeon;
	ok;
}

static int test_a_centre_with_no_vault_gives_up(void *state) {
	const char *error = NULL;
	int depth = player->depth;

	/* The precondition: nothing is allowed this deep */
	player->depth = z_info->max_depth + 20;
	null(random_vault(player->depth, "Greater vault"));
	null(random_vault(player->depth, "Greater vault (new)"));

	/* And the level builder says so instead of crashing */
	null(hard_centre_gen(player, z_info->dungeon_hgt, z_info->dungeon_wid,
		&error));
	notnull(error);
	player->depth = depth;
	ok;
}

/*
 * Down and up the dungeon in strides, out to the surface every tenth level.
 * Levels here are not kept, so whatever chunks are stored after the first
 * lap are all there should be after the last.
 */
static int test_a_long_run_stores_nothing_behind(void *state) {
	uint16_t stored = 0;
	int i;

	for (i = 0; i < 150; i++) {
		arrive((i % 10 == 9) ? 0 : 1 + (i * 37) % (z_info->max_depth - 1));
		notnull(cave);
		if (i == 29) stored = chunk_list_max;
	}
	eq(chunk_list_max, stored);
	ok;
}

const char *suite_name = "game/generations";
struct test tests[] = {
	{ "every-depth-makes-a-level", test_every_depth_makes_a_level },
	{ "no-way-down-goes-below-the-bottom",
	  test_no_way_down_goes_below_the_bottom },
	{ "a-centre-with-no-vault-gives-up", test_a_centre_with_no_vault_gives_up },
	{ "a-long-run-stores-nothing-behind",
	  test_a_long_run_stores_nothing_behind },
	{ NULL, NULL }
};
