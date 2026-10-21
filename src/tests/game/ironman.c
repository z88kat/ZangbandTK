/* game/ironman.c
 *
 * Two of Zangband's ironman options (DEC-163): `birth_no_stores`, its
 * `ironman_shops`, and `birth_small_levels`, its `ironman_small_levels`, and
 * the Golem's stun immunity, which closed stores are one of three ways to lose.
 *
 * The doors: every shop in the town, the home with them, says it is locked and
 * is not entered, and with the option off the same doors open.  The levels:
 * every depth of the world and of every dungeon is made inside Zangband's small
 * range, none of them is one of the two level types that can't be made small,
 * and without the option the levels are the size they always were.  The
 * monster base shrinks with the level, as Zangband's does.  And a Golem can be
 * stunned under any of the three settings Zangband takes that away for.
 */

#include "unit-test.h"
#include "test-utils.h"

#include "cave.h"
#include "cmds.h"
#include "dun-type.h"
#include "game-event.h"
#include "game-world.h"
#include "generate.h"
#include "init.h"
#include "mon-make.h"
#include "mon-util.h"
#include "option.h"
#include "player-birth.h"
#include "player-calcs.h"
#include "player-timed.h"
#include "player-util.h"
#include "savefile.h"
#include "z-file.h"

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

/* The save case's file, deleted here so that a failing case can't leave it */
static char savename[64];

int teardown_tests(void *state) {
	if (savename[0]) file_delete(savename);
	if (cave) wipe_mon_list(cave, player);
	cleanup_angband();
	return 0;
}

/** Make the level at a depth, as taking a staircase there does. */
static void arrive(int dungeon, int depth) {
	player->dungeon = dungeon;
	player->depth = depth;
	prepare_next_level(player);
	on_new_level();
}

/* ------------------------------------------------------------------ doors */

static int entered;
static int locked;

static void count_entry(game_event_type type, game_event_data *data,
		void *user) {
	entered++;
}

static void count_locked(game_event_type type, game_event_data *data,
		void *user) {
	if (data->message.msg && streq(data->message.msg, "The doors are locked."))
		locked++;
}

/** Step onto a shop door, as walking there does; true if the shop opened. */
static bool step_onto(struct loc grid) {
	entered = 0;
	if (!loc_eq(player->grid, grid)) monster_swap(player->grid, grid);
	/* The game removes this after each visit, so it goes on each time */
	event_add_handler(EVENT_ENTER_STORE, count_entry, NULL);
	player_handle_post_move(player, false, false);
	event_remove_handler(EVENT_ENTER_STORE, count_entry, NULL);
	return entered > 0;
}

/** Hold still on a shop door, the other way in; true if the shop opened. */
static bool hold_on(struct loc grid) {
	entered = 0;
	if (!loc_eq(player->grid, grid)) monster_swap(player->grid, grid);
	event_add_handler(EVENT_ENTER_STORE, count_entry, NULL);
	do_cmd_hold(NULL);
	event_remove_handler(EVENT_ENTER_STORE, count_entry, NULL);
	return entered > 0;
}

static int test_every_door_is_locked(void *state) {
	struct loc grid;
	int shops = 0, homes = 0;

	arrive(0, 0);
	locked = 0;
	event_add_handler(EVENT_MESSAGE, count_locked, NULL);

	for (grid.y = 0; grid.y < cave->height; grid.y++) {
		for (grid.x = 0; grid.x < cave->width; grid.x++) {
			if (!square_isshop(cave, grid)) continue;
			if (square_monster(cave, grid)) continue;

			shops++;
			if (square(cave, grid)->feat == FEAT_HOME) homes++;

			player->opts.opt[OPT_birth_no_stores] = false;
			if (!step_onto(grid) || !hold_on(grid)) {
				event_remove_handler(EVENT_MESSAGE, count_locked, NULL);
				require(false);
			}

			player->opts.opt[OPT_birth_no_stores] = true;
			if (step_onto(grid) || hold_on(grid)) {
				event_remove_handler(EVENT_MESSAGE, count_locked, NULL);
				player->opts.opt[OPT_birth_no_stores] = false;
				require(false);
			}
		}
	}

	event_remove_handler(EVENT_MESSAGE, count_locked, NULL);
	player->opts.opt[OPT_birth_no_stores] = false;

	printf("%d shop doors, %d of them a home, all locked\n", shops, homes);
	require(shops > 0);
	require(homes > 0);
	/* Once for walking on, once for holding still */
	eq(locked, 2 * shops);
	ok;
}

/* ----------------------------------------------------------------- levels */

static char profile[40];

static void note_profile(game_event_type type, game_event_data *data,
		void *user) {
	my_strcpy(profile, data->string ? data->string : "", sizeof(profile));
}

/** The builders that take Zangband's draw, rather than a size of their own. */
static bool takes_the_draw(const char *name) {
	return streq(name, "classic") || streq(name, "modified")
		|| streq(name, "moria") || streq(name, "lair");
}

/** Make a level and check it is small; false, having said why, if not. */
static bool small_at(int dungeon, int depth) {
	arrive(dungeon, depth);

	if (streq(profile, "gauntlet") || streq(profile, "hard centre")) {
		printf("dungeon %d depth %d: a %s level\n", dungeon, depth, profile);
		return false;
	}
	if (cave->height > 55 || cave->width > 187) {
		printf("dungeon %d depth %d: %s is %d by %d\n", dungeon, depth,
			profile, cave->height, cave->width);
		return false;
	}
	if (takes_the_draw(profile) && (cave->height < 22 || cave->width < 66
			|| cave->height % 11 || cave->width % 11)) {
		printf("dungeon %d depth %d: %s is %d by %d, not Zangband's draw\n",
			dungeon, depth, profile, cave->height, cave->width);
		return false;
	}
	return true;
}

static int test_every_level_is_small(void *state) {
	int dungeon, depth, made = 0;
	bool good = true;

	player->opts.opt[OPT_birth_small_levels] = true;
	event_add_handler(EVENT_GEN_LEVEL_START, note_profile, NULL);

	/* The world's own dungeon, quest levels and all */
	for (depth = 1; good && depth < z_info->max_depth; depth++, made++)
		good = small_at(0, depth);

	/* And every named one, the Abyss's hard centres included */
	for (dungeon = 1; good && dungeon <= dun_type_count(); dungeon++) {
		const struct dun_type *type = dun_type_by_index(dungeon - 1);

		for (depth = MAX(type->min_depth, 1);
				good && depth <= type->max_depth; depth++, made++)
			good = small_at(dungeon, depth);
	}

	event_remove_handler(EVENT_GEN_LEVEL_START, note_profile, NULL);
	player->opts.opt[OPT_birth_small_levels] = false;
	printf("%d levels made, all small\n", made);
	require(good);
	ok;
}

/*
 * The draw covers its range: over many levels the smallest a builder makes is
 * one screen and the largest is one block short of a full level each way.
 */
static int test_the_draw_spans_zangbands_range(void *state) {
	int i, min_h = 1000, max_h = 0, min_w = 1000, max_w = 0;

	player->opts.opt[OPT_birth_small_levels] = true;
	for (i = 0; i < 2000; i++) {
		int h = 0, w = 0;

		require(small_level_size(player, &h, &w));
		min_h = MIN(min_h, h);
		max_h = MAX(max_h, h);
		min_w = MIN(min_w, w);
		max_w = MAX(max_w, w);
	}
	player->opts.opt[OPT_birth_small_levels] = false;

	eq(min_h, 22);
	eq(max_h, 55);
	eq(min_w, 66);
	eq(max_w, 187);
	ok;
}

static int test_without_the_option_nothing_changes(void *state) {
	int i, h = -1, w = -1, full = 0, centres = 0;

	player->opts.opt[OPT_birth_small_levels] = false;
	require(!small_level_size(player, &h, &w));
	eq(h, -1);
	eq(w, -1);

	/* Classic levels are always full size, and most levels are classic */
	event_add_handler(EVENT_GEN_LEVEL_START, note_profile, NULL);
	for (i = 0; i < 60; i++) {
		arrive(0, 30);
		if (cave->height == z_info->dungeon_hgt
				&& cave->width == z_info->dungeon_wid) full++;
	}

	/* And the Abyss is still all hard centres */
	for (i = 0; i < 3; i++) {
		const struct dun_type *type = NULL;
		int dungeon;

		for (dungeon = 1; dungeon <= dun_type_count(); dungeon++) {
			type = dun_type_by_index(dungeon - 1);
			if (type->profile && streq(type->profile, "hard centre")) break;
		}
		require(dungeon <= dun_type_count());
		arrive(dungeon, type->min_depth);
		if (streq(profile, "hard centre")) centres++;
	}
	event_remove_handler(EVENT_GEN_LEVEL_START, note_profile, NULL);

	require(full > 0);
	eq(centres, 3);
	ok;
}

static int test_the_monster_base_scales_with_area(void *state) {
	int base = z_info->level_monster_min;

	/* Off, the base is the base at any size */
	player->opts.opt[OPT_birth_small_levels] = false;
	eq(level_monster_base(player, 22, 66), base);

	/* On, Zangband's sum: 14 * 22 / 66 = 4, 4 * 66 / 198 = 1, plus one */
	player->opts.opt[OPT_birth_small_levels] = true;
	eq(level_monster_base(player, 22, 66), 2);
	eq(level_monster_base(player, 55, 187), 11);
	eq(level_monster_base(player, z_info->dungeon_hgt, z_info->dungeon_wid),
		base);

	/* And it never comes out above the base */
	eq(level_monster_base(player, z_info->dungeon_hgt, 187), base);
	player->opts.opt[OPT_birth_small_levels] = false;
	ok;
}

/* -------------------------------------------------------------- the Golem */

/** Be the race of that name, with everything it gives worked out again. */
static bool become(const char *name) {
	const struct player_race *r;

	for (r = races; r; r = r->next)
		if (streq(r->name, name)) break;
	if (!r) return false;
	player->race = r;
	player->upkeep->update |= PU_BONUS;
	update_stuff(player);
	return true;
}

/** Try to stun the player; true if it landed. */
static bool stunned(void) {
	bool landed;

	player_clear_timed(player, TMD_STUN, false, false);
	(void) player_inc_timed(player, TMD_STUN, 20, false, false, true);
	landed = player->timed[TMD_STUN] > 0;
	player_clear_timed(player, TMD_STUN, false, false);
	return landed;
}

/*
 * Zangband's set_stun() spares a Golem unless the game is ironman_shops,
 * ironman_downward or ironman_nightmare (effects.c:1876).  Each of the three
 * alone takes it away, and with none of them the Golem is as it was.
 */
static int test_a_golem_is_stunned_in_a_hard_game(void *state) {
	static const int hard[] = {
		OPT_birth_nightmare, OPT_birth_force_descend, OPT_birth_no_stores
	};
	const char *kept = player->race->name;
	size_t i;

	require(become("Golem"));
	require(player_of_has(player, OF_PROT_STUN));
	require(!stunned());

	for (i = 0; i < N_ELEMENTS(hard); i++) {
		bool protected, landed;

		player->opts.opt[hard[i]] = true;
		player->upkeep->update |= PU_BONUS;
		update_stuff(player);
		protected = player_of_has(player, OF_PROT_STUN);
		landed = stunned();
		player->opts.opt[hard[i]] = false;
		player->upkeep->update |= PU_BONUS;
		update_stuff(player);

		if (protected || !landed) {
			printf("%s: protected %d, stunned %d\n", option_name(hard[i]),
				protected, landed);
			require(become(kept));
			require(false);
		}
	}

	/* And with the options off again, it is as it was */
	require(player_of_has(player, OF_PROT_STUN));
	require(!stunned());
	require(become(kept));
	ok;
}

/* ---------------------------------------------------------- the savefile */

/*
 * Options are saved by name, so these two need nothing of their own, but a
 * birth option that didn't come back would be a different game on reload.
 * Each is set the opposite way to the other, so a swap shows, and both are
 * flipped in memory before the load, so a load that skipped them shows too.
 * Last, because the reset takes everything the other cases set up with it.
 */
static int test_the_options_survive_a_save(void *state) {
	int on;

	test_savefile_name(savename, sizeof(savename), "Ironman");
	for (on = 0; on < 2; on++) {
		player->opts.opt[OPT_birth_no_stores] = on;
		player->opts.opt[OPT_birth_small_levels] = !on;
		require(savefile_save(savename));

		play_again = true;
		if (cave) wipe_mon_list(cave, player);
		cleanup_angband();
		chunk_list_max = 0;
		init_angband();
		play_again = false;

		if (player) {
			player->opts.opt[OPT_birth_no_stores] = !on;
			player->opts.opt[OPT_birth_small_levels] = on;
		}
		require(savefile_load(savename, false));
		eq(player->opts.opt[OPT_birth_no_stores], on);
		eq(player->opts.opt[OPT_birth_small_levels], !on);
	}
	ok;
}

const char *suite_name = "game/ironman";
struct test tests[] = {
	{ "every-door-is-locked", test_every_door_is_locked },
	{ "every-level-is-small", test_every_level_is_small },
	{ "the-draw-spans-zangbands-range", test_the_draw_spans_zangbands_range },
	{ "without-the-option-nothing-changes",
	  test_without_the_option_nothing_changes },
	{ "the-monster-base-scales-with-area",
	  test_the_monster_base_scales_with_area },
	{ "a-golem-is-stunned-in-a-hard-game",
	  test_a_golem_is_stunned_in_a_hard_game },
	{ "the-options-survive-a-save", test_the_options_survive_a_save },
	{ NULL, NULL }
};
