/* command/wizard
 *
 * The debug commands that take a square (ZangbandTK).
 *
 * Teleport-to and push-objects get their square from an argument or from a
 * front end's point hook, and the hook's answer is whatever the front end
 * says.  Neither command looked at it: off the level, the first square
 * lookup aborted the game, and on a passable square of the level's edge
 * teleport-to looped for ever looking for a landing inside it.  The look
 * command never targets those squares, so it took a script to find them.
 */

#include "unit-test.h"
#include "test-utils.h"

#include "cave.h"
#include "cmd-core.h"
#include "cmds.h"
#include "game-world.h"
#include "generate.h"
#include "init.h"
#include "mon-make.h"
#include "player-birth.h"

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

/** Run a debug command with its square already given. */
static void wiz_at(void (*fn)(struct command *), struct loc grid) {
	struct command cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd_set_arg_point(&cmd, "point", grid);
	fn(&cmd);
}

/** A square the character could stand on, away from where they are. */
static bool landing(struct loc *grid) {
	int x, y;

	for (y = 1; y < cave->height - 1; y++) {
		for (x = 1; x < cave->width - 1; x++) {
			struct loc g = loc(x, y);

			if (loc_eq(g, player->grid)) continue;
			if (!square_isempty(cave, g)) continue;
			if (square_isshop(cave, g) || square_isdamaging(cave, g))
				continue;
			*grid = g;
			return true;
		}
	}
	return false;
}

/* The teleport still works where it should. */
static int test_teleport_inside(void *state) {
	struct loc grid;

	require(landing(&grid));
	wiz_at(do_cmd_wiz_teleport_to, grid);
	require(loc_eq(player->grid, grid));
	ok;
}

/* Off the level: refused, where it used to abort. */
static int test_teleport_off_level(void *state) {
	struct loc start = player->grid;

	wiz_at(do_cmd_wiz_teleport_to, loc(cave->width + 10, 5));
	require(loc_eq(player->grid, start));
	wiz_at(do_cmd_wiz_teleport_to, loc(5, cave->height));
	require(loc_eq(player->grid, start));
	wiz_at(do_cmd_wiz_teleport_to, loc(-1, -1));
	require(loc_eq(player->grid, start));
	ok;
}

/*
 * On the edge, made passable so that only the bounds check stands in the
 * way: refused, where it used to loop.
 */
static int test_teleport_edge(void *state) {
	struct loc start = player->grid;
	struct loc edge = loc(cave->width - 1, cave->height / 2);
	int feat = square(cave, edge)->feat;

	square_set_feat(cave, edge, FEAT_FLOOR);
	wiz_at(do_cmd_wiz_teleport_to, edge);
	square_set_feat(cave, edge, feat);
	require(loc_eq(player->grid, start));
	ok;
}

/* Off the level: refused, where it used to abort. */
static int test_push_off_level(void *state) {
	wiz_at(do_cmd_wiz_push_object, loc(cave->width, cave->height));
	wiz_at(do_cmd_wiz_push_object, loc(-1, 3));
	ok;
}

const char *suite_name = "command/wizard";
struct test tests[] = {
	{ "teleport inside", test_teleport_inside },
	{ "teleport off level", test_teleport_off_level },
	{ "teleport edge", test_teleport_edge },
	{ "push off level", test_push_off_level },
	{ NULL, NULL }
};
