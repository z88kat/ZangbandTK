/* game/wild.c
 *
 * The town on the wilderness surface (WLD-12, WLD-24).
 *
 * These need a fully initialised game -- terrain, stores, a generated
 * character -- so they live here rather than with the world-map tests in
 * cave/wild, which run without game data.
 *
 * The property being defended is the one the design turns on: the town and the
 * country around it are one map, and the way between them is walking.  It is
 * easy to break by accident, because Angband's town is a starburst clearing
 * inside a permanent wall and every part of that wants to keep the player in.
 */

#include "unit-test.h"
#include "unit-test-data.h"
#include "test-utils.h"

#include "cave.h"
#include "game-event.h"
#include "game-world.h"
#include "generate.h"
#include "init.h"
#include "mon-make.h"
#include "player.h"
#include "player-birth.h"
#include "savefile.h"
#include "wild.h"
#include "z-util.h"

static void println(const char *str) {
	printf("%s\n", str);
}

int setup_tests(void **state) {
	plog_aux = println;

	set_file_paths();
	init_angband();
#ifdef UNIX
	create_needed_dirs();
#endif

	require(player_make_simple(NULL, NULL, "Wanderer"));
	prepare_next_level(player);

	return 0;
}

int teardown_tests(void *state) {
	wipe_mon_list(cave, player);
	cleanup_angband();
	return 0;
}

/* A new character stands on the surface of the world, in the town. */
static int test_start_is_on_the_surface(void *state) {
	notnull(cave);
	require(wild_is_surface(cave));
	require(player->in_wild);
	eq(player->depth, 0);

	/* The surface is the whole window, not one town-sized level. */
	eq(cave->width, wild_view_blocks() * z_info->wild_block_size);
	eq(cave->height, cave->width);

	/* And the character starts on the town's staircase, as in Angband. */
	require(square_isdownstairs(cave, player->grid));

	ok;
}

/* The player's world position and their position on the window agree. */
static int test_world_position_matches_the_window(void *state) {
	eq(player->wild_grid.x, player->wild_offset.x + player->grid.x);
	eq(player->wild_grid.y, player->wild_offset.y + player->grid.y);

	ok;
}

/*
 * The town can be walked out of.
 *
 * Floods outwards from the player across everything that can be walked on, and
 * requires that it escapes the town's rectangle.  A town whose only exit is the
 * staircase would pass every other test here and still be a trap.
 */
static int test_town_can_be_walked_out_of(void *state) {
	struct loc org = loc(wild_town_origin(wild).x - player->wild_offset.x,
						 wild_town_origin(wild).y - player->wild_offset.y);
	int w = cave->width, h = cave->height;
	bool *seen = mem_zalloc((size_t) w * h * sizeof(bool));
	struct loc *queue = mem_zalloc((size_t) w * h * sizeof(struct loc));
	int head = 0, tail = 0;
	bool escaped = false;

	queue[tail++] = player->grid;
	seen[player->grid.y * w + player->grid.x] = true;

	while (head < tail && !escaped) {
		struct loc grid = queue[head++];
		int dir;

		/* Outside the town's rectangle, with open country ahead. */
		if (grid.x < org.x || grid.x >= org.x + z_info->town_wid ||
			grid.y < org.y || grid.y >= org.y + z_info->town_hgt) {
			escaped = true;
			break;
		}

		for (dir = 0; dir < 8; dir++) {
			struct loc next = loc(grid.x + ddx_ddd[dir], grid.y + ddy_ddd[dir]);

			if (!square_in_bounds_fully(cave, next)) continue;
			if (seen[next.y * w + next.x]) continue;
			if (!square_ispassable(cave, next)) continue;

			seen[next.y * w + next.x] = true;
			queue[tail++] = next;
		}
	}

	mem_free(queue);
	mem_free(seen);

	require(escaped);

	ok;
}

/*
 * The surface has the boundary Angband expects, but only at the world's edge.
 *
 * A great deal of level code steps one grid outwards without checking, so the
 * edge of the world needs a permanent wall.  Everywhere else the window scrolls
 * before the player can reach its border, and a wall there would be a wall
 * across open country.
 */
static int test_boundary_only_at_the_world_edge(void *state) {
	int span = cave->width;
	int world = wild_world_grids();
	int i, walls = 0;

	for (i = 0; i < span; i++)
		if (square_isperm(cave, loc(i, 0)))
			walls++;

	if (player->wild_offset.y == 0) {
		eq(walls, span);
	} else {
		require(walls < span);
	}

	require(world > span);

	ok;
}

/*
 * The town is known from the start.  The world is not.
 *
 * Angband knows the whole of depth zero from the first turn, which is right
 * when depth zero is nothing but a town.  On a surface it would hand the player
 * the coastline, the forests and the mountains for free -- everything the
 * overworld exists to be walked to find out.
 */
static int test_only_the_town_is_known(void *state) {
	struct loc org = loc(wild_town_origin(wild).x - player->wild_offset.x,
						 wild_town_origin(wild).y - player->wild_offset.y);
	struct loc grid;
	int unknown_outside = 0, outside = 0, shops = 0;

	/* Every grid of the town is known: you have lived in it. */
	for (grid.y = org.y; grid.y < org.y + z_info->town_hgt; grid.y++)
		for (grid.x = org.x; grid.x < org.x + z_info->town_wid; grid.x++) {
			if (!square_in_bounds_fully(cave, grid)) continue;

			require(square_isknown(cave, grid));
			if (square_isshop(cave, grid)) shops++;
		}

	/* And it has shops in it. */
	require(shops > 0);

	/* The country beyond it is mostly not. */
	for (grid.y = 0; grid.y < cave->height; grid.y++)
		for (grid.x = 0; grid.x < cave->width; grid.x++) {
			if (grid.x >= org.x && grid.x < org.x + z_info->town_wid &&
				grid.y >= org.y && grid.y < org.y + z_info->town_hgt)
				continue;

			outside++;
			if (!square_isknown(cave, grid))
				unknown_outside++;
		}

	require(outside > 0);

	/*
	 * Not "none of it": the player can see out of the town, and what they can
	 * see is fairly theirs.  What must not happen is the whole world arriving
	 * known, so the bar is that the great majority of it has not.
	 */
	require(unknown_outside > (outside * 9) / 10);

	ok;
}

/*
 * The town has people in it.
 *
 * town_gen() places its residents itself, and the surface does not call it --
 * it takes the town's terrain and draws that in.  The beggars and the scruffy
 * dogs were lost that way once already, quietly, and were only found missing by
 * walking the streets and noticing they were empty.
 */
static int test_the_town_has_people(void *state) {
	struct loc org = loc(wild_town_origin(wild).x - player->wild_offset.x,
						 wild_town_origin(wild).y - player->wild_offset.y);
	struct loc grid;
	int residents = 0;

	for (grid.y = org.y; grid.y < org.y + z_info->town_hgt; grid.y++)
		for (grid.x = org.x; grid.x < org.x + z_info->town_wid; grid.x++) {
			if (!square_in_bounds_fully(cave, grid)) continue;
			if (square_monster(cave, grid)) residents++;
		}

	require(residents > 0);

	ok;
}

/*
 * The world survives a save and a load.
 *
 * The world map is not written to the savefile -- it regenerates from the seed
 * -- so what has to come back is the player's position in it.  Losing that
 * would put a character who saved three days' walk from home back on the town
 * staircase, and would look like a generation fault rather than a savefile one.
 *
 * Runs last: it tears the game down and builds it back up, which the tests
 * above would not survive.
 */
static int test_world_position_survives_a_save(void *state) {
	struct loc grid = player->wild_grid;
	struct loc offset = player->wild_offset;
	int town_x, town_y;

	/* Walk a little first, so the position saved is not the starting one. */
	player->wild_grid.x = grid.x + 3;
	player->wild_grid.y = grid.y + 2;

	require(savefile_save("Test-wild"));

	town_x = wild->town_block.x;
	town_y = wild->town_block.y;

	play_again = true;
	wipe_mon_list(cave, player);
	cleanup_angband();
	chunk_list_max = 0;
	init_angband();
	play_again = false;

	require(savefile_load("Test-wild", false));

	require(player->in_wild);
	eq(player->wild_grid.x, grid.x + 3);
	eq(player->wild_grid.y, grid.y + 2);
	eq(player->wild_offset.x, offset.x);
	eq(player->wild_offset.y, offset.y);

	/* The world came back the same, rather than being generated afresh. */
	notnull(wild);
	eq(wild->town_block.x, town_x);
	eq(wild->town_block.y, town_y);

	file_delete("Test-wild");

	ok;
}



const char *suite_name = "game/wild";
struct test tests[] = {
	{ "start-is-on-the-surface", test_start_is_on_the_surface },
	{ "world-position-matches-the-window", test_world_position_matches_the_window },
	{ "town-can-be-walked-out-of", test_town_can_be_walked_out_of },
	{ "boundary-only-at-the-world-edge", test_boundary_only_at_the_world_edge },
	{ "only-the-town-is-known", test_only_the_town_is_known },
	{ "the-town-has-people", test_the_town_has_people },
	{ "world-position-survives-a-save", test_world_position_survives_a_save },
	{ NULL, NULL }
};
