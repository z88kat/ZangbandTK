/* cave/light — what bright terrain gives the squares around it
 *
 * Lava is the game's one bright terrain.  A square of it glows, as
 * square_set_feat() makes all bright terrain do, has a light of two more of its
 * own, and gives one to each of the eight squares round it -- a wall only
 * where the face that is lit is one the player could see, which an open room
 * never asks about.
 *
 * For years it gave that to four of them.  calc_lighting() set every square's
 * starting light and added the lava's in the same pass over the level, so the
 * squares that came after the lava in the scan -- to its right and the row
 * below -- set their own starting value over what they had just been given.
 * Nothing about that looks wrong on the screen unless you stand and count, so
 * the room here is dark and the player is at the far end of it, and the tests
 * count.
 */
#include "unit-test.h"
#include "test-utils.h"
#include "cave.h"
#include "init.h"
#include "player.h"
#include "player-birth.h"

/* A dark room of floor inside a ring of permanent wall. */
static struct chunk *create_empty_cave(int height, int width) {
	struct chunk *c = cave_new(height, width);
	struct loc grid;

	grid.y = 0;
	for (grid.x = 0; grid.x < width; ++grid.x) {
		square_set_feat(c, grid, FEAT_PERM);
	}
	for (grid.y = 1; grid.y < height - 1; ++grid.y) {
		grid.x = 0;
		square_set_feat(c, grid, FEAT_PERM);
		for (grid.x = 1; grid.x < width - 1; ++grid.x) {
			square_set_feat(c, grid, FEAT_FLOOR);
		}
		grid.x = width - 1;
		square_set_feat(c, grid, FEAT_PERM);
	}
	grid.y = height - 1;
	for (grid.x = 0; grid.x < width; ++grid.x) {
		square_set_feat(c, grid, FEAT_PERM);
	}
	return c;
}

int setup_tests(void **state) {
	set_file_paths();
	if (!init_angband()) {
		*state = NULL;
		return 1;
	}
	if (!player_make_simple(NULL, NULL, "Tester")) {
		cleanup_angband();
		return 1;
	}
	*state = NULL;
	return 0;
}

int teardown_tests(void *state) {
	cleanup_angband();
	return 0;
}

/*
 * The player stands here, ten and more squares from any lava below: well out
 * of reach of their own light, and well inside how far they can see.
 */
static const struct loc far_end = { 3, 5 };

static int test_lava_lights_all_eight(void *state) {
	struct chunk *c = create_empty_cave(11, 21);
	struct loc lava = loc(15, 5);
	int before[9], dir;

	player->grid = far_end;
	update_view(c, player);
	for (dir = 0; dir < 9; dir++) {
		before[dir] = square_light(c, loc_sum(lava, ddgrid_ddd[dir]));
	}

	square_set_feat(c, lava, FEAT_LAVA);
	update_view(c, player);

	/* One more for each square round it, and three for the lava itself. */
	for (dir = 0; dir < 8; dir++) {
		eq(square_light(c, loc_sum(lava, ddgrid_ddd[dir])),
			before[dir] + 1);
	}
	eq(square_light(c, lava), before[8] + 3);

	cave_free(c);
	ok;
}

static int test_two_lavas_light_each_other_alike(void *state) {
	struct chunk *c = create_empty_cave(11, 21);
	struct loc west = loc(14, 5), east = loc(15, 5);

	player->grid = far_end;
	square_set_feat(c, west, FEAT_LAVA);
	square_set_feat(c, east, FEAT_LAVA);
	update_view(c, player);

	/* Each its own three and one from the other, whichever comes first. */
	eq(square_light(c, west), 4);
	eq(square_light(c, east), 4);

	/* The squares above and below the pair are next to both. */
	eq(square_light(c, loc(14, 4)), 2);
	eq(square_light(c, loc(15, 4)), 2);
	eq(square_light(c, loc(14, 6)), 2);
	eq(square_light(c, loc(15, 6)), 2);

	/* And the squares at either end to one only. */
	eq(square_light(c, loc(13, 5)), 1);
	eq(square_light(c, loc(16, 5)), 1);

	cave_free(c);
	ok;
}

const char *suite_name = "cave/light";
struct test tests[] = {
	{ "lava lights all eight squares round it", test_lava_lights_all_eight },
	{ "two lavas light each other alike",
		test_two_lavas_light_each_other_alike },
	{ NULL, NULL }
};
