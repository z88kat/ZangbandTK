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
#include "obj-make.h"
#include "obj-pile.h"
#include "obj-tval.h"
#include "obj-util.h"
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
 * The surface always carries the impassable ring every Angband level has.
 *
 * A great deal of code relies on that ring without saying so -- monster group
 * placement, object drops and others step outwards from a grid on the
 * assumption that nothing is ever standing on the outermost one. The surface
 * broke that assumption when the ring was drawn only at the edge of the world,
 * and walked off the chunk about one run in thirty.
 *
 * Away from the world's edge the ring is a fiction, so the window must scroll
 * while the player is still further from it than they can see. That margin is
 * checked here too, since the ring being invisible is the whole reason it is
 * allowed to be a fiction.
 */
static int test_the_surface_is_bounded(void *state) {
	int span = cave->width;
	int i, edges = 0;

	for (i = 0; i < span; i++) {
		if (square_feat(cave, loc(i, 0))->fidx == FEAT_WORLD_EDGE) edges++;
		require(!square_ispassable(cave, loc(i, 0)));
		require(!square_ispassable(cave, loc(i, span - 1)));
		require(!square_ispassable(cave, loc(0, i)));
		require(!square_ispassable(cave, loc(span - 1, i)));
	}

	/* And it is sea, not masonry: the world ends in water. */
	eq(edges, span);

	/* The player is rebuilt out of sight of it. */
	require(wild_world_grids() > span);

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
 * The doorstep is survivable.
 *
 * Danger in the wilderness comes from law and nothing else, so where the town
 * stands decides what a first-level character meets on walking out of it.
 * Measured before the town was placed on that basis, the answer was monsters of
 * dungeon depth 20 to 53, three blocks from the gate.
 *
 * The bar here is deliberately loose -- the point is that the country around
 * the town is quiet, not that it is empty, and beyond it the danger should
 * climb.  Measured across eight worlds after the fix, the mean came out between
 * 1 and 6.
 */
static int test_the_doorstep_is_survivable(void *state) {
	int reach = 6;
	int sum = 0, count = 0, x, y;

	for (y = wild->town_block.y - reach; y <= wild->town_block.y + reach; y++)
		for (x = wild->town_block.x - reach; x <= wild->town_block.x + reach; x++) {
			if (!wild_in_bounds(wild, x, y)) continue;
			sum += wild_danger(wild, x, y);
			count++;
		}

	require(count > 0);
	require(sum / count <= 12);

	ok;
}

/*
 * There is something alive out there (CNT-05).
 *
 * Rolled per grid from the land's own fertility, so the count varies -- what
 * must not happen is an empty world, which is what it was before this.
 */
static int test_the_wilderness_is_inhabited(void *state) {
	struct loc org = loc(wild_town_origin(wild).x - player->wild_offset.x,
						 wild_town_origin(wild).y - player->wild_offset.y);
	struct loc grid;
	int wild_monsters = 0, round;

	/*
	 * Populated three times over.  A single window can legitimately come up
	 * empty -- the roll is per grid and barren country is meant to be bare --
	 * so asserting on one would be asserting on the dice.  Three rounds tests
	 * that the path works without pinning the density, which is a tuning
	 * matter and belongs in constants.txt.
	 */
	for (round = 0; round < 3; round++)
		wild_populate(wild, player, cave, player->wild_offset);

	for (grid.y = 0; grid.y < cave->height; grid.y++)
		for (grid.x = 0; grid.x < cave->width; grid.x++) {
			if (grid.x >= org.x && grid.x < org.x + z_info->town_wid &&
				grid.y >= org.y && grid.y < org.y + z_info->town_hgt)
				continue;
			if (square_monster(cave, grid)) wild_monsters++;
		}

	require(wild_monsters > 0);

	ok;
}

/*
 * What you drop stays where you dropped it -- for a while (WLD-04, WLD-04a).
 *
 * The surface is torn down and rebuilt whenever the window scrolls, so anything
 * left on it has to be taken into the world's memory and put back.  It also has
 * to be forgotten eventually, or the world becomes a museum of everything the
 * player ever discarded.
 */
static int test_what_you_drop_is_remembered(void *state) {
	struct loc drop = loc(player->grid.x + 4, player->grid.y);
	struct object *obj;
	bool dummy = true;
	int before;

	/* Somewhere the player can actually put something down. */
	while (!square_isobjectholding(cave, drop) ||
		   square_object(cave, drop)) {
		drop.x++;
		require(drop.x < cave->width - 1);
	}

	obj = object_new();
	object_prep(obj, lookup_kind(TV_FOOD, lookup_sval(TV_FOOD, "Ration of Food")),
				0, RANDOMISE);
	obj->number = 1;
	require(floor_carry(cave, drop, obj, &dummy));
	list_object(cave, obj);

	before = wild_relic_count(wild);

	/* Tear the surface down, as scrolling the window would. */
	wild_harvest(wild, player, cave, player->wild_offset);
	require(wild_relic_count(wild) > before);

	/* And build it again, right away, so nothing has had time to go. */
	wild_restore(wild, player, cave, player->wild_offset);
	eq(wild_relic_count(wild), before);
	notnull(square_object(cave, drop));

	ok;
}

/*
 * And it does not stay forever (WLD-04a).
 *
 * Harvested with the clock wound a long way back, so that the decay has had
 * every chance to act.  The odds halve with each half-life, so after a great
 * many of them the survival chance is nil rather than merely small -- which is
 * what makes this testable without depending on the dice.
 */
static int test_what_you_drop_is_forgotten(void *state) {
	struct loc drop = loc(player->grid.x + 4, player->grid.y);
	struct wild_relic *relic;
	struct object *obj;
	bool dummy = true;

	while (!square_isobjectholding(cave, drop) ||
		   square_object(cave, drop)) {
		drop.x++;
		require(drop.x < cave->width - 1);
	}

	obj = object_new();
	object_prep(obj, lookup_kind(TV_FOOD, lookup_sval(TV_FOOD, "Ration of Food")),
				0, RANDOMISE);
	obj->number = 1;
	require(floor_carry(cave, drop, obj, &dummy));
	list_object(cave, obj);

	wild_harvest(wild, player, cave, player->wild_offset);
	require(wild_relic_count(wild) > 0);

	/* Age everything in the world's memory by a very long time. */
	for (relic = wild->relics; relic; relic = relic->next)
		relic->turn -= 10L * z_info->day_length * 1000L;

	wild_restore(wild, player, cave, player->wild_offset);
	eq(wild_relic_count(wild), 0);
	null(square_object(cave, drop));

	ok;
}

/*
 * A unique you wounded and left is still out there, and is better for the rest
 * (WLD-04b).
 *
 * Ordinary monsters are re-rolled with the country, which reads as their having
 * recovered and wandered off. A named monster cannot be treated that way: if it
 * could, every unique in the wilderness would be a fresh one at full health,
 * and nothing the player did to it would ever count.
 */
static int test_a_wounded_unique_is_remembered(void *state) {
	struct monster_race *race = NULL;
	struct monster_group_info info = { 0, 0 };
	struct loc grid = player->grid;
	struct monster *mon;
	int i;

	/* Any unique shallow enough to be placeable will do. */
	for (i = 1; i < z_info->r_max; i++) {
		if (!r_info[i].name) continue;
		if (!rf_has(r_info[i].flags, RF_UNIQUE)) continue;
		if (r_info[i].cur_num >= r_info[i].max_num) continue;
		race = &r_info[i];
		break;
	}
	require(race != NULL);

	/* Somewhere near the player that will hold it. */
	do {
		grid.x++;
		require(grid.x < cave->width - 1);
	} while (!square_isempty(cave, grid) || square_isdamaging(cave, grid));

	require(place_new_monster(cave, grid, race, false, false, info,
							  ORIGIN_DROP));
	mon = square_monster(cave, grid);
	notnull(mon);
	mon->hp = 1;

	/* Walk away: the window scrolls, the surface goes. */
	wild_harvest(wild, player, cave, player->wild_offset);
	require(wild_unique_count(wild) > 0);

	/*
	 * Clear it off the surface the way tearing the level down would, so the
	 * race's counter is free for it to be placed again.
	 */
	delete_monster(cave, grid);

	/* Come back a long time later. */
	{
		struct wild_unique *seen;

		for (seen = wild->uniques; seen; seen = seen->next)
			seen->turn -= 10L * z_info->day_length;
	}

	wild_restore(wild, player, cave, player->wild_offset);
	eq(wild_unique_count(wild), 0);

	/* It is somewhere nearby, and it has had time to heal. */
	{
		bool found = false;
		struct loc g;

		for (g.y = grid.y - 8; g.y <= grid.y + 8 && !found; g.y++)
			for (g.x = grid.x - 8; g.x <= grid.x + 8 && !found; g.x++) {
				if (!square_in_bounds_fully(cave, g)) continue;
				mon = square_monster(cave, g);
				if (mon && mon->race == race) {
					found = true;
					require(mon->hp > 1);
				}
			}

		require(found);
	}

	ok;
}

/*
 * An untouched unique is not remembered, and the townspeople stay in town.
 *
 * Both of these presented as the same complaint in play: Farmer Maggot
 * everywhere, all the time, impossible to shake off. He is a depth-zero monster
 * -- one of the town's own -- and get_mon_num() will place a monster shallower
 * than the level it is asked for, so he was a legitimate pick for open country.
 * Remembering him then pinned him in place, since a remembered unique is put
 * back near where it was left and he had been following the player.
 */
static int test_the_townspeople_stay_in_town(void *state) {
	struct loc org = loc(wild_town_origin(wild).x - player->wild_offset.x,
						 wild_town_origin(wild).y - player->wild_offset.y);
	struct loc grid;
	int strays = 0, seen = 0;

	/*
	 * Judged outside the town and its outskirts rather than outside the town
	 * rectangle. Residents are placed with their groups, as they are in
	 * vanilla, and a group placed near the edge can scatter a little way past
	 * it -- which is a townsperson on the edge of town, not a townsperson in
	 * the wilds. The margin still leaves the great majority of the surface
	 * under test.
	 */
	for (grid.y = 0; grid.y < cave->height; grid.y++)
		for (grid.x = 0; grid.x < cave->width; grid.x++) {
			struct monster *mon;
			int margin = 12;

			if (grid.x >= org.x - margin &&
				grid.x < org.x + z_info->town_wid + margin &&
				grid.y >= org.y - margin &&
				grid.y < org.y + z_info->town_hgt + margin)
				continue;

			mon = square_monster(cave, grid);
			if (!mon || !mon->race) continue;

			seen++;
			if (mon->race->level == 0) strays++;
		}

	require(seen > 0);
	eq(strays, 0);

	ok;
}

/* An unwounded unique is left to the country to re-roll, not pinned in place. */
static int test_an_untouched_unique_is_not_remembered(void *state) {
	struct monster_race *race = NULL;
	struct monster_group_info info = { 0, 0 };
	struct loc grid = player->grid;
	int before, i;

	for (i = 1; i < z_info->r_max; i++) {
		if (!r_info[i].name) continue;
		if (!rf_has(r_info[i].flags, RF_UNIQUE)) continue;
		if (r_info[i].cur_num >= r_info[i].max_num) continue;
		race = &r_info[i];
		break;
	}
	require(race != NULL);

	do {
		grid.x++;
		require(grid.x < cave->width - 1);
	} while (!square_isempty(cave, grid) || square_isdamaging(cave, grid));

	require(place_new_monster(cave, grid, race, false, false, info,
							  ORIGIN_DROP));

	before = wild_unique_count(wild);
	wild_harvest(wild, player, cave, player->wild_offset);
	eq(wild_unique_count(wild), before);

	delete_monster(cave, grid);

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
	{ "the-surface-is-bounded", test_the_surface_is_bounded },
	{ "only-the-town-is-known", test_only_the_town_is_known },
	{ "the-town-has-people", test_the_town_has_people },
	{ "the-doorstep-is-survivable", test_the_doorstep_is_survivable },
	{ "the-wilderness-is-inhabited", test_the_wilderness_is_inhabited },
	{ "what-you-drop-is-remembered", test_what_you_drop_is_remembered },
	{ "what-you-drop-is-forgotten", test_what_you_drop_is_forgotten },
	{ "a-wounded-unique-is-remembered", test_a_wounded_unique_is_remembered },
	{ "the-townspeople-stay-in-town", test_the_townspeople_stay_in_town },
	{ "an-untouched-unique-is-not-remembered", test_an_untouched_unique_is_not_remembered },
	{ "world-position-survives-a-save", test_world_position_survives_a_save },
	{ NULL, NULL }
};
