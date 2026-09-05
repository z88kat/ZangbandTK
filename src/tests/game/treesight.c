/* game/treesight
 *
 * Looking at a monster must not make you forget the trees (WLD).
 *
 * `path_analyse()` in mon-util.c walks the projection path from the player to
 * every monster in view and forgets any grid along it that the player's *memory*
 * says blocks line of sight.  In Angband 4.2 that inference cannot fail: nothing
 * there lets a projection through while blocking sight, so a sight-blocker found
 * on a projection path is by definition misremembered and the memory should go.
 *
 * A tree is the exception, and the only feature in this game that is one --
 * `PROJECT` and `PASSABLE` without `LOS`, which is the whole of "you can push
 * through, but you cannot see far".  A bolt crosses it and sight does not, so
 * `los()` and `project_path()` disagree about it: a monster can be plainly in
 * view around a tree while the projection path to it goes straight through one.
 * Every such tree was forgotten, every turn.
 *
 * It also compounds, which is why it looked like a flicker rather than a steady
 * fault.  A forgotten grid remembers as `FEAT_NONE`, `FEAT_NONE` carries no
 * `LOS` flag, so `square_allowslos(player->cave, ...)` stays false for it and the
 * same grid is re-forgotten on every later pass.  One tree could hold a line of
 * country open until the player walked down it.
 *
 * Reported from play as a graphical fault -- squares going unexplored and coming
 * back as the character moved, in every tileset.  It was never graphical.  It was
 * in the ASCII build too, as a blank among the grass, and had been for months.
 *
 * === Why this has a suite to itself ===
 *
 * The test has to stand the character in woodland, remember the whole map, and
 * put monsters round them.  Done inside `game/wild` that is three pieces of
 * shared state to put back, and putting them back is where it went wrong: the
 * first version wiped the town's residents, the second left the character
 * standing somewhere else, and the third crashed one run in twelve restoring a
 * knowledge array against a level that had moved under it.
 *
 * A suite of its own gets a level of its own and owes nothing to anybody.  That
 * is cheaper than three restores and does not go stale when a later test starts
 * caring about something new.
 */

#include "unit-test.h"
#include "test-utils.h"

#include "cave.h"
#include "game-world.h"
#include "generate.h"
#include "init.h"
#include "mon-make.h"
#include "mon-util.h"
#include "monster.h"
#include "player-birth.h"
#include "player-calcs.h"
#include "player-util.h"
#include "z-util.h"

static void println(const char *str) {
	printf("%s\n", str);
}

int setup_tests(void **state) {
	plog_aux = println;
	set_file_paths();
	if (!init_angband()) return 1;
	(void) test_seed_rng_reported(suite_name);
	if (!player_make_simple(NULL, "Ranger", "Tester")) return 1;
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

/** Stand the character where there are trees to lose. */
static bool stand_in_woodland(void) {
	struct loc scan;

	for (scan.y = 2; scan.y < cave->height - 2; scan.y++)
		for (scan.x = 2; scan.x < cave->width - 2; scan.x++) {
			int d, near = 0;

			if (!square_isempty(cave, scan)) continue;
			for (d = 0; d < 8; d++)
				if (square_feat(cave, loc_sum(scan, ddgrid_ddd[d]))->fidx
						== FEAT_TREE) near++;
			if (near < 4) continue;

			monster_swap(player->grid, scan);
			player_handle_post_move(player, false, true);
			return true;
		}

	return false;
}

/**
 * A tree the player remembers is still remembered after the monsters update.
 *
 * Written as a property rather than a staged geometry, because reproducing it by
 * hand means arranging a player, a tree and a monster such that `los()` reaches
 * the monster and `project_path()` does not miss the tree -- fiddly to pin and
 * fragile to keep pinned.  The rule underneath is simpler and is the one that
 * matters: *updating monsters must not lose terrain the player correctly
 * remembers.*
 */
static int test_a_remembered_tree_survives_a_monster(void *state) {
	struct loc scan;
	int trees = 0, forgotten = 0, placed = 0, radius;

	require(stand_in_woodland());

	/* A character who has walked the country and remembers all of it. */
	for (scan.y = 0; scan.y < cave->height; scan.y++)
		for (scan.x = 0; scan.x < cave->width; scan.x++)
			square_memorize(cave, scan);

	/*
	 * Monsters round about, so the paths to them cross the wood.
	 *
	 * Searched outward in rings rather than taken from a fixed ring at one
	 * distance.  A tree is passable and is not *floor*, so `square_isempty()`
	 * refuses it -- and the whole point of this test is to stand somewhere
	 * with trees on every side.  The fixed version found nowhere to put a
	 * monster on about one seed in a hundred, which is exactly the kind of
	 * flake that gets rerun rather than read.  It was caught by the sanitizer
	 * pass, which runs its own seed.
	 */
	for (radius = 2; radius <= 8 && placed < 6; radius++) {
		int dx, dy;

		for (dy = -radius; dy <= radius && placed < 6; dy++)
			for (dx = -radius; dx <= radius && placed < 6; dx++) {
				struct monster_group_info info = { 0, 0 };
				struct monster_race *race;
				struct loc grid = loc(player->grid.x + dx,
									  player->grid.y + dy);

				/* Only the ring; the inside has been tried already. */
				if (ABS(dx) != radius && ABS(dy) != radius) continue;
				if (!square_in_bounds_fully(cave, grid)) continue;
				if (!square_isempty(cave, grid)) continue;

				race = get_mon_num(1, 1);
				if (!race) continue;
				if (place_new_monster(cave, grid, race, false, false, info,
									  ORIGIN_DROP)) placed++;
			}
	}
	require(placed > 0);

	/*
	 * What the game does every turn the player moves.  The view has to be
	 * brought up to date first: `path_analyse()` only runs for a monster
	 * whose grid is flagged in view, so without this the test walks straight
	 * past the thing it is testing -- which the first version of it did, and
	 * passed against a deliberately broken build.
	 */
	player->upkeep->update |= (PU_UPDATE_VIEW | PU_MONSTERS);
	update_stuff(player);
	update_monsters(true);

	for (scan.y = 0; scan.y < cave->height; scan.y++)
		for (scan.x = 0; scan.x < cave->width; scan.x++) {
			if (square_feat(cave, scan)->fidx != FEAT_TREE) continue;
			trees++;
			if (!square_isknown(cave, scan)) forgotten++;
		}

	/* A wooded world, or this proves nothing. */
	require(trees > 20);
	eq(forgotten, 0);

	ok;
}

const char *suite_name = "game/treesight";
struct test tests[] = {
	{ "a-remembered-tree-survives-a-monster",
	  test_a_remembered_tree_survives_a_monster },
	{ NULL, NULL }
};
