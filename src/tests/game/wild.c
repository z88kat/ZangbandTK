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
#include "player-util.h"
#include "mon-util.h"
#include "savefile.h"
#include "dun-type.h"
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
 * The town can be got out of, and the way out is a gate.
 *
 * Floods outwards from the player across everything that can be walked on or
 * opened, and requires that it escapes the town's rectangle. A town whose only
 * exit is the staircase would pass every other test here and still be a trap.
 *
 * Closed doors count as passable because a gate is a door: the player opens it
 * and walks through. What must not exist is a way out that is neither.
 */
static int test_town_can_be_walked_out_of(void *state) {
	struct loc org = loc(wild_town_origin(wild).x - player->wild_offset.x,
						 wild_town_origin(wild).y - player->wild_offset.y);
	int w = cave->width, h = cave->height;
	bool *seen = mem_zalloc((size_t) w * h * sizeof(bool));
	struct loc *queue = mem_zalloc((size_t) w * h * sizeof(struct loc));
	int head = 0, tail = 0;
	bool escaped = false, through_a_door = false;

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

			if (square_iscloseddoor(cave, next)) {
				through_a_door = true;
			} else if (!square_ispassable(cave, next)) {
				continue;
			}

			seen[next.y * w + next.x] = true;
			queue[tail++] = next;
		}
	}

	mem_free(queue);
	mem_free(seen);

	require(escaped);
	require(through_a_door);

	ok;
}

/*
 * The gates are two tiles wide, and they are the only way through the wall.
 *
 * Before this, the way out was wherever the starburst clearing happened to
 * reach the edge of its rectangle -- gaps several tiles across, in no
 * particular place, that anything could wander in through.
 */
static int test_the_town_gates_are_narrow(void *state) {
	struct loc org = loc(wild_town_origin(wild).x - player->wild_offset.x,
						 wild_town_origin(wild).y - player->wild_offset.y);
	int wid = z_info->town_wid, hgt = z_info->town_hgt;
	int i, doors = 0, holes = 0;

	/* The boundary of the town's interior, all four sides. */
	for (i = 1; i < wid - 1; i++) {
		struct loc top = loc(org.x + i, org.y + 1);
		struct loc bottom = loc(org.x + i, org.y + hgt - 2);

		if (!square_in_bounds_fully(cave, top)) continue;

		if (square_iscloseddoor(cave, top)) doors++;
		else if (square_ispassable(cave, top) && !square_isshop(cave, top))
			holes++;

		if (!square_in_bounds_fully(cave, bottom)) continue;
		if (square_iscloseddoor(cave, bottom)) doors++;
		else if (square_ispassable(cave, bottom) && !square_isshop(cave, bottom))
			holes++;
	}
	for (i = 1; i < hgt - 1; i++) {
		struct loc left = loc(org.x + 1, org.y + i);
		struct loc right = loc(org.x + wid - 2, org.y + i);

		if (square_in_bounds_fully(cave, left)) {
			if (square_iscloseddoor(cave, left)) doors++;
			else if (square_ispassable(cave, left) && !square_isshop(cave, left))
				holes++;
		}
		if (square_in_bounds_fully(cave, right)) {
			if (square_iscloseddoor(cave, right)) doors++;
			else if (square_ispassable(cave, right) && !square_isshop(cave, right))
				holes++;
		}
	}

	/* Gated, not merely holed. */
	require(doors >= 2);

	/* Two tiles a side, four sides, and nothing gets through anywhere else. */
	require(doors <= 8);
	eq(holes, 0);

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

	for (y = wild->towns[0].block.y - reach; y <= wild->towns[0].block.y + reach; y++)
		for (x = wild->towns[0].block.x - reach; x <= wild->towns[0].block.x + reach; x++) {
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
 * Teleporting on the surface does not rewind the player's position.
 *
 * The world position used to be updated only by walking, so any other
 * displacement -- a phase door, a trapdoor's landing, being thrown by a monster
 * -- left it stale, and the next rebuild snapped the player back to wherever
 * they had last walked.
 */
static int test_displacement_keeps_the_world_position(void *state) {
	struct loc from = player->grid;
	struct loc to = loc(from.x + 9, from.y + 5);

	while (!square_isempty(cave, to) || square_isdamaging(cave, to)) {
		to.x++;
		require(to.x < cave->width - 1);
	}

	/* Move the player the way an effect would, not the way walking does. */
	monster_swap(player->grid, to);
	player_handle_post_move(player, false, true);

	eq(player->wild_grid.x, player->wild_offset.x + player->grid.x);
	eq(player->wild_grid.y, player->wild_offset.y + player->grid.y);

	/* Put them back. */
	monster_swap(player->grid, from);
	player_handle_post_move(player, false, true);

	ok;
}

/*
 * What the player has mapped survives the window scrolling.
 *
 * The knowledge chunk is rebuilt with the surface, so without carrying it over
 * the map was wiped roughly every forty steps -- and since the surface does not
 * memorise itself under daylight, nothing put it back.
 */
static int test_the_map_survives_a_scroll(void *state) {
	struct chunk *known;
	struct loc offset = player->wild_offset;
	struct loc probe;
	int feat, moved = 24;

	/* Learn a grid the player would not otherwise know. */
	probe = loc(player->grid.x + 20, player->grid.y);
	require(square_in_bounds_fully(cave, probe));
	square_memorize(cave, probe);
	feat = square(player->cave, probe)->feat;
	require(feat != FEAT_NONE);

	/* Scroll the window along, as walking to its edge would. */
	known = player->cave;
	player->cave = cave_new(cave->height, cave->width);
	wild_carry_knowledge(known, offset, player->cave,
						 loc(offset.x + moved, offset.y));
	cave_free(known);

	/* The same world grid is still known, at its new place in the window. */
	eq(square(player->cave, loc(probe.x - moved, probe.y))->feat, feat);

	ok;
}

/*
 * Scrolling the window does not move the player.
 *
 * The fault this defends against was reported from play: walk west out of town,
 * the map jumps, a strip of country you have never seen appears, and the town is
 * suddenly nowhere to be found. The cause was sanitize_player_loc(), which asks
 * square_isarrivable() -- and that wants FLOOR or stairs. Trees and water are
 * passable and are not floor, so a player standing among trees when the window
 * scrolled counted as having arrived somewhere illegal and was flung to a random
 * grid anywhere in the window. In a world this wooded, most of the time.
 */
static int test_scrolling_does_not_move_the_player(void *state) {
	struct loc was, probe = loc(-1, -1), scan;

	/* Find somewhere passable that is deliberately not floor: a tree. */
	for (scan.y = 1; scan.y < cave->height - 1 && probe.x < 0; scan.y++)
		for (scan.x = 1; scan.x < cave->width - 1; scan.x++) {
			if (square_feat(cave, scan)->fidx != FEAT_TREE) continue;
			if (square_monster(cave, scan)) continue;
			probe = scan;
			break;
		}
	require(probe.x >= 0);
	require(!square_isfloor(cave, probe));
	require(square_ispassable(cave, probe));

	/* Stand there, and note where in the world that is. */
	monster_swap(player->grid, probe);
	player_handle_post_move(player, false, true);
	was = player->wild_grid;

	/*
	 * Scroll the window through the real path, not a hand-rolled imitation of
	 * it -- the fault was in which helper prepare_next_level() reached for, and
	 * a test that called the right one itself would not have caught it.
	 */
	player->upkeep->generate_level = true;
	player->upkeep->scroll_world = true;
	prepare_next_level(player);
	player->upkeep->generate_level = false;
	player->upkeep->scroll_world = false;

	/* Still in the same place in the world. */
	eq(player->wild_grid.x, was.x);
	eq(player->wild_grid.y, was.y);

	ok;
}

/*
 * The land reserved for a town is not the town (WLD-25).
 *
 * A block's `place` mark covers the town plus a block of margin on every side,
 * so a road has somewhere to leave from -- 35 blocks against the town's own 15.
 * Treating that as "town" made the overhead map paint a slab of masonry more
 * than twice the size of the place, with the player standing in it while
 * plainly out in the fields, and silenced the monsters over all of it.
 */
static int test_reserved_land_is_not_the_town(void *state) {
	int x, y, town = 0, reserved = 0;

	for (y = 0; y < wild->blocks; y++)
		for (x = 0; x < wild->blocks; x++) {
			int idx = wild_town_at(wild, x, y);

			if (idx >= 0) town++;
			if (wild_block_at(wild, x, y)->place) reserved++;

			/* Only a block a town actually stands on draws as a town... */
			if (idx < 0 && wild_block_at(wild, x, y)->place) {
				require(wild_block_feat(wild, x, y) != FEAT_PERM);

				/* ...or falls silent. */
				require(wild_danger(wild, x, y) > 0);
			}
		}

	require(town > 0);

	/*
	 * Every town reserves a block of margin on each side, so the land spoken
	 * for is always larger than the towns standing on it.
	 */
	require(reserved > town);

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

	town_x = wild->towns[0].block.x;
	town_y = wild->towns[0].block.y;

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
	eq(wild->towns[0].block.x, town_x);
	eq(wild->towns[0].block.y, town_y);

	file_delete("Test-wild");

	ok;
}






/*
 * The world map records where the player has been, and only that (WLD-25).
 *
 * Knowledge for the map is per block, which is all a map drawing one character
 * per block can show. A new character has seen the country around the town and
 * nothing else; the map fills in behind them as they travel.
 */
static int test_the_world_map_remembers_travel(void *state) {
	int size = z_info->wild_block_size;
	struct loc home = loc(player->wild_grid.x / size, player->wild_grid.y / size);
	int seen = 0, total = wild->blocks * wild->blocks, i;

	/* Where they are standing is known, and so are its neighbours. */
	require(wild_seen(wild, home.x, home.y));
	require(wild_seen(wild, home.x + 1, home.y));
	require(wild_seen(wild, home.x, home.y + 1));

	/* Far away is not. */
	require(!wild_seen(wild, home.x + 20, home.y + 20));

	for (i = 0; i < total; i++)
		if (wild->map[i].info & WILD_INFO_SEEN) seen++;

	/* A handful of blocks, not a revealed world. */
	require(seen > 0);
	require(seen < 40);

	/* Walking marks more of it. */
	{
		struct loc there = loc(player->wild_grid.x + size * 3,
							   player->wild_grid.y);

		wild_mark_seen(wild, there);
		require(wild_seen(wild, there.x / size, there.y / size));
	}

	/* Every seen block has something to draw. */
	for (i = 0; i < total; i++)
		if (wild->map[i].info & WILD_INFO_SEEN) {
			int feat = wild_block_feat(wild, i % wild->blocks, i / wild->blocks);

			require(feat > FEAT_NONE);
			require(feat < FEAT_MAX);
		}

	ok;
}



/**
 * The town on the ground holds exactly the trades the world says it holds.
 *
 * struct wild_town::stores is only a promise until the generator keeps it, and
 * the two halves are written in different files.  This is the test that ties
 * them together: every shop door standing in the starting village is one the
 * village was given, and every trade it was given has a door.
 */
static int test_the_town_holds_the_trades_it_was_given(void *state) {
	struct loc org = loc(wild_town_origin(wild).x - player->wild_offset.x,
						 wild_town_origin(wild).y - player->wild_offset.y);
	uint16_t promised = wild->towns[0].stores, found = 0;
	struct loc grid;
	int n;

	for (grid.y = org.y; grid.y < org.y + z_info->town_hgt; grid.y++)
		for (grid.x = org.x; grid.x < org.x + z_info->town_wid; grid.x++) {
			int shop;

			if (!square_in_bounds_fully(cave, grid)) continue;

			shop = square_shopnum(cave, grid);
			if (shop >= 0 && shop < 16) found |= 1u << shop;
		}

	require(promised != 0);
	require(found == promised);

	/* And the starting village is the one that is fixed rather than drawn. */
	require(promised & (1u << WILD_STORE_GENERAL));
	require(promised & (1u << WILD_STORE_BOOK));
	require(promised & (1u << WILD_STORE_ALCHEMY));
	require(promised & (1u << WILD_STORE_HOME));

	/* Nothing it was not given: no armoury, no weaponsmith, no black market. */
	for (n = 0; n < 16; n++)
		if (!(promised & (1u << n)))
			require(!(found & (1u << n)));

	ok;
}

/**
 * A town larger than the starting village is built across the whole of itself.
 *
 * get_lot_bounds() clamped every lot to z_info->town_wid/hgt, which describe the
 * starting village and nothing else.  The layout loop retries until all its
 * shops land, so a larger town still got its full complement -- crammed into the
 * western strip the clamp allowed, with everything beyond it left as bare floor.
 * Walking in from the east found an empty field inside the walls.
 *
 * So presence is not the property to test.  Reach is: something the town was
 * built out of has to stand beyond where the old clamp could put it.
 */
static int test_a_large_town_is_built(void *state) {
	int reach_x = 0, reach_y = 0, seed;

	for (seed = 0; seed < 8; seed++) {
		struct chunk *town = town_gen_wild(player, 8191 + seed * 977,
										   132, 34, 0xffff, 0);
		uint16_t found = 0;
		struct loc grid;

		require(town);
		require(town->width == 132 && town->height == 34);

		for (grid.y = 1; grid.y < town->height - 1; grid.y++)
			for (grid.x = 1; grid.x < town->width - 1; grid.x++) {
				int shop = square_shopnum(town, grid);

				if (shop >= 0 && shop < 16) found |= 1u << shop;

				/*
				 * Shop doors alone.  The rock a ruin is built from cannot be
				 * told from the rock the clearing was cut out of, so it would
				 * not measure what is being asked.
				 */
				if (shop >= 0) {
					reach_x = MAX(reach_x, grid.x);
					reach_y = MAX(reach_y, grid.y);
				}
			}

		/* Every trade it was given still stands in it. */
		require(found != 0);

		cave_free(town);
	}

	/*
	 * Beyond the old clamp, which could never reach past town_wid - 3 or
	 * town_hgt - 3.  Measured rather than guessed: over these eight seeds the
	 * fixed code reaches x=116, y=24 in a 132x34 town, and the clamped code
	 * stops dead at x=61, y=17.  The thresholds sit between the two.
	 */
	require(reach_x > z_info->town_wid + 4);
	require(reach_y > z_info->town_hgt - 2);

	ok;
}

/**
 * A town away from home is drawn onto the surface that covers it.
 *
 * The call that draws towns into the live window used to be wrapped in a test
 * against the starting village's rectangle, so once the player walked far
 * enough that the window no longer covered home, no town was drawn at all --
 * including the one being walked into.  Approaching another town made it
 * disappear as the window scrolled.
 */
static int test_a_distant_town_is_drawn(void *state) {
	int idx;

	require(wild_town_count(wild) > 1);

	for (idx = 1; idx < wild_town_count(wild); idx++) {
		struct loc org = wild_town_origin_of(wild, idx);
		struct loc centre = loc(org.x + wild->towns[idx].wid / 2,
								org.y + wild->towns[idx].hgt / 2);
		struct loc offset;
		struct chunk *c;
		struct loc grid;
		int shops = 0, stairs = 0;

		/* A window centred on that town, with home nowhere near it. */
		c = wild_surface(wild, player, centre, &offset);
		require(c);

		for (grid.y = 0; grid.y < c->height; grid.y++)
			for (grid.x = 0; grid.x < c->width; grid.x++)
				if (square_isshop(c, grid)) shops++;
				else if (square_isdownstairs(c, grid)) stairs++;


		/* The town is there, with its trades in it. */
		require(shops > 0);

		/*
		 * And it kept its staircase.  wild_town_wall() moves the staircase
		 * inside when the north wall lands on it, which puts it in the north
		 * gate's path; a gate that paved over open ground took it with it.
		 */
		require(stairs > 0);

		cave_free(c);
	}

	ok;
}

/**
 * What the player knows of the surface survives a trip to the dungeon (WLD-25).
 *
 * The surface is rebuilt from the world seed every time it is returned to, and
 * the parallel chunk holding what has been seen is rebuilt empty with it.
 * generate_level() carried that knowledge across a window scroll in a local,
 * which cannot survive a dungeon level in between -- so walking into town,
 * going down the stairs and coming back up found the town unexplored.  wild.c
 * holds it instead.
 *
 * This exercises the holding, which is the part that was missing.  The two
 * calls in generate_level() that hand it over and take it back are not
 * reachable from this harness.
 */
static int test_the_map_survives_the_dungeon(void *state) {
	struct loc offset = player->wild_offset;
	struct chunk *known, *back;
	struct loc taken = loc(-1, -1);
	struct loc probe;
	int feat;

	/* Learn a grid the player would not otherwise know. */
	probe = loc(player->grid.x + 20, player->grid.y);
	require(square_in_bounds_fully(cave, probe));
	square_memorize(cave, probe);
	feat = square(player->cave, probe)->feat;
	require(feat != FEAT_NONE);

	/* Going below hands the knowledge over rather than dropping it. */
	known = cave_new(cave->height, cave->width);
	wild_carry_knowledge(player->cave, offset, known, offset);
	require(square(known, probe)->feat == feat);
	wild_keep_knowledge(known, offset);

	/* Coming back up takes it, at the offset it was left at. */
	back = wild_take_knowledge(&taken);
	notnull(back);
	eq(taken.x, offset.x);
	eq(taken.y, offset.y);
	eq(square(back, probe)->feat, feat);

	/* And it is handed over once, not held onto. */
	require(wild_take_knowledge(NULL) == NULL);

	cave_free(back);

	ok;
}

/**
 * Walking east or west does not shift the world up or down.
 *
 * A rebuild is triggered by either axis nearing the window's edge, and used to
 * re-anchor both.  Since the window aligns to whole blocks, re-anchoring an
 * axis the player had merely drifted along moved them by up to a block within
 * the chunk -- so a character walking due west appeared to drop a dozen tiles
 * down the screen mid-stride, having never moved north or south.
 */
static int test_walking_sideways_keeps_the_row(void *state) {
	int size = z_info->wild_block_size;
	int span = wild_view_blocks() * size;
	struct loc here = player->wild_grid;
	struct loc first, second;
	struct chunk *a, *b;
	int drift = size + 4;

	/*
	 * Start well inside the world, so that neither axis is against an edge and
	 * clamping is not what keeps the offset still.
	 */
	here.x = span * 3 + size * 5 + 7;
	here.y = span * 3 + size * 5 + 3;

	a = wild_surface(wild, player, here, &first);
	notnull(a);
	cave_free(a);

	/*
	 * Walk west until the window must follow, going round things on the way --
	 * which is what makes a character drift north and south while heading in a
	 * single direction.  The drift is more than a block, because the window
	 * aligns to whole blocks and a smaller one would be absorbed by that and
	 * prove nothing.
	 */
	here.x = first.x + 1;
	here.y -= drift;

	b = wild_surface(wild, player, here, &second);
	notnull(b);
	cave_free(b);

	/* The window follows them west... */
	require(second.x < first.x);

	/* ...and does not move at all in the axis they did not walk out of. */
	eq(second.y, first.y);

	ok;
}

/**
 * The window reports how far it moved, so the display can follow it (WLD-25).
 *
 * The panel is addressed in coordinates within the chunk, and a rebuild
 * replaces the chunk under it.  Shifting the panel by exactly this keeps the
 * same country in the same place on screen; getting it wrong is the difference
 * between scrolling the world and appearing to jump across it.
 */
static int test_the_window_reports_its_travel(void *state) {
	int size = z_info->wild_block_size;
	int span = wild_view_blocks() * size;
	struct loc here = loc(span * 3 + size * 5 + 7, span * 3 + size * 5 + 3);
	struct loc first, second, moved;
	struct chunk *a, *b;

	a = wild_surface(wild, player, here, &first);
	notnull(a);
	cave_free(a);

	/* Far enough that both axes have to move. */
	here.x -= span;
	here.y -= span;

	b = wild_surface(wild, player, here, &second);
	notnull(b);
	cave_free(b);

	require(second.x != first.x);
	require(second.y != first.y);

	moved = wild_scroll_delta();
	eq(moved.x, first.x - second.x);
	eq(moved.y, first.y - second.y);

	ok;
}



/**
 * What the player knows of the surface survives being saved from below (WLD-25).
 *
 * Standing on the surface, this is player->cave and wr_dungeon() writes it.
 * Down in the dungeon the surface has been taken down and only wild.c still
 * holds what was learned of it -- so a character who saved below and came back
 * up found the town unexplored.  That is the same fault as losing the map across
 * a dungeon trip, one layer further out: through the savefile rather than
 * through generate_level().
 */
static int test_the_map_survives_a_save_from_below(void *state) {
	struct loc probe = loc(player->grid.x + 12, player->grid.y + 4);
	struct loc offset = player->wild_offset;
	struct loc taken = loc(-1, -1);
	struct chunk *back;
	int feat;

	require(square_in_bounds_fully(cave, probe));
	square_memorize(cave, probe);
	feat = square(player->cave, probe)->feat;
	require(feat != FEAT_NONE);

	/* Going below hands the knowledge to wild.c, which is where a save finds it. */
	{
		struct chunk *known = cave_new(cave->height, cave->width);

		wild_carry_knowledge(player->cave, offset, known, offset);
		require(square(known, probe)->feat == feat);
		wild_keep_knowledge(known, offset);
	}

	require(savefile_save("Test-know"));

	play_again = true;
	wipe_mon_list(cave, player);
	cleanup_angband();
	chunk_list_max = 0;
	init_angband();
	play_again = false;

	require(savefile_load("Test-know", false));

	/* It came back, at the offset it was left at, with the grid still known. */
	back = wild_take_knowledge(&taken);
	notnull(back);
	eq(taken.x, offset.x);
	eq(taken.y, offset.y);
	require(square_in_bounds_fully(back, probe));
	eq(square(back, probe)->feat, feat);

	cave_free(back);
	file_delete("Test-know");

	ok;
}


/**
 * A wall you are standing next to is visible, even from inside a wood.
 *
 * Angband lights a wall only if the grid between it and the player carries
 * light onto its face, which assumes anything blocking sight is a wall nobody
 * can stand in.  ZangbandTK's trees are PASSABLE without LOS, so a character
 * standing in a wood beside a town wall was told the grid they were standing in
 * did not let light through: the wall was never lit, never seen and never
 * remembered, and appeared only when they walked into it.  Only the stretches
 * with grass in front of them lit normally, which is what made it puzzling.
 */
static int test_a_wall_is_seen_from_inside_a_wood(void *state) {
	struct loc mid = loc(cave->width / 2, cave->height / 2);
	struct loc wall = loc(mid.x, mid.y + 1);
	struct loc away = loc(mid.x, mid.y - 40);
	struct loc grid;
	int dx, dy;

	require(square_in_bounds_fully(cave, wall));
	require(square_in_bounds_fully(cave, away));

	/* A clearing with a wall across its south side, and trees in front. */
	for (dy = -3; dy <= 3; dy++)
		for (dx = -3; dx <= 3; dx++) {
			grid = loc(mid.x + dx, mid.y + dy);
			square_set_feat(cave, grid, (dy == 1) ? FEAT_PERM : FEAT_TREE);
		}

	/* Out of sight of it to begin with, so the walk up is what reveals it. */
	square_set_feat(cave, away, FEAT_GRASS);
	player->grid = away;
	square_forget(cave, wall);
	sqinfo_off(square(cave, wall)->info, SQUARE_SEEN);
	cave_illuminate(cave, true, false);
	update_view(cave, player);
	require(!square_isseen(cave, wall));

	/* Now stand in the trees, right against the wall. */
	player->grid = mid;
	cave_illuminate(cave, true, false);
	update_view(cave, player);

	/* Daylight reaches it, it is seen, and walking away will not unlearn it. */
	require(square_light(cave, wall) > 0);
	require(square_isseen(cave, wall));
	require(square_isknown(cave, wall));

	/*
	 * And at a distance, not only when touching it.  The first fix for this
	 * was a special case for the player's own grid, which left a wall two
	 * grids off behind trees still invisible while the same wall behind grass
	 * was plain to see -- the same inconsistency, one grid further out.  The
	 * view calculation already puts such a wall in the field of view; it was
	 * only the light that was missing.
	 */
	{
		int away;

		for (away = 2; away <= 4; away++) {
			struct loc far_wall = loc(mid.x, mid.y + away);
			int a, b;

			/* Trees everywhere, and the wall further out through them. */
			for (a = -5; a <= 5; a++)
				for (b = -5; b <= 5; b++)
					square_set_feat(cave, loc(mid.x + a, mid.y + b), FEAT_TREE);

			square_set_feat(cave, far_wall, FEAT_PERM);
			square_forget(cave, far_wall);
			sqinfo_off(square(cave, far_wall)->info, SQUARE_SEEN);

			player->grid = mid;
			cave_illuminate(cave, true, false);
			update_view(cave, player);

			if (!square_isseen(cave, far_wall))
				printf("a wall %d grids off through trees is not seen\n", away);

			require(square_light(cave, far_wall) > 0);
			require(square_isseen(cave, far_wall));
		}
	}

	ok;
}


/**
 * Every dungeon in the game data opens somewhere in the world (WLD-14).
 *
 * The depth ladder must have no gap in it.  A world missing the dungeon that
 * covers depths 40 to 75 is a world a character cannot get past depth 40 in, so
 * rarity decides the order the mouths are sited in, never whether a dungeon is
 * present at all.
 */
static int test_every_dungeon_opens_somewhere(void *state) {
	bool seen[DUN_TYPE_MAX] = { false };
	struct dun_type *type;
	int i, deepest = 0;

	require(dun_type_count() > 0);
	eq(wild_dungeon_count(wild), dun_type_count());

	for (i = 0; i < wild_dungeon_count(wild); i++) {
		struct wild_dungeon *mouth = wild_dungeon_by_index(wild, i);
		struct wild_block *block;
		int j;

		notnull(dun_type_by_index(mouth->type));

		/* One mouth each, and no two in the same place. */
		require(mouth->type < DUN_TYPE_MAX);
		require(!seen[mouth->type]);
		seen[mouth->type] = true;

		for (j = 0; j < i; j++)
			require(!loc_eq(mouth->grid, wild_dungeon_by_index(wild, j)->grid));

		/* Not in the sea, and not in a lake. */
		block = wild_block_at(wild, mouth->block.x, mouth->block.y);
		notnull(block);
		require(block->terrain != WILD_TERRAIN_OCEAN);
		require(!(block->info & WILD_INFO_WATER));

		/* The mouth is where the map says the dungeon is. */
		eq(wild_dungeon_at(wild, mouth->grid), i);
		require(wild_dungeon_in_block(wild, mouth->block.x, mouth->block.y));
	}

	/*
	 * And between them they reach the bottom, by an unbroken chain: no depth
	 * from the first level down is out of reach of some dungeon.
	 */
	for (type = dun_types; type; type = type->next)
		if (type->max_depth > deepest) deepest = type->max_depth;

	for (i = 1; i <= deepest; i++) {
		bool covered = false;

		for (type = dun_types; type; type = type->next)
			if (i >= type->min_depth && i <= type->max_depth) covered = true;

		require(covered);
	}

	ok;
}

/**
 * A dungeon ends at its own bottom, not at the bottom of the game (WLD-14).
 */
static int test_a_dungeon_has_a_bottom(void *state) {
	struct dun_type *type;

	for (type = dun_types; type; type = type->next) {
		player->dungeon = type->index + 1;

		/* At its deepest level, there is no way further down. */
		player->depth = type->max_depth;
		require(dungeon_get_next_level(player, player->depth, 1)
				<= type->max_depth);

		/* Part way down, there is. */
		if (type->max_depth > type->min_depth) {
			player->depth = type->min_depth;
			require(dungeon_get_next_level(player, player->depth, 1)
					> type->min_depth);
		}

		/* And going up from the top of it comes out onto the surface. */
		player->depth = type->min_depth;
		eq(dungeon_get_next_level(player, player->depth, -1), 0);
	}

	player->dungeon = 0;
	player->depth = 0;

	ok;
}

/**
 * Each dungeon remembers its own depth, for recall (WLD-14).
 *
 * Word of recall has to return the player to where they were in the dungeon
 * they were last in.  Sending them to the deepest level they have reached
 * anywhere could drop them into a dungeon on the far side of the world, or
 * below the bottom of the one they are standing on.
 */
static int test_each_dungeon_remembers_its_own_depth(void *state) {
	struct wild_dungeon *first = wild_dungeon_by_index(wild, 0);
	struct wild_dungeon *second = wild_dungeon_by_index(wild, 1);
	struct dun_type *ta, *tb;

	notnull(first);
	notnull(second);
	ta = dun_type_by_index(first->type);
	tb = dun_type_by_index(second->type);
	notnull(ta);
	notnull(tb);

	/* Go down the first one a way. */
	player->dungeon = first->type + 1;
	player->depth = ta->min_depth + 1;
	player_note_dungeon_depth(player);
	eq(first->max_depth, ta->min_depth + 1);

	/* The second knows nothing of it. */
	eq(second->max_depth, 0);
	player->dungeon = second->type + 1;
	eq(player_dungeon_recall_depth(player), 0);

	/* And the first still remembers when we come back to it. */
	player->dungeon = first->type + 1;
	eq(player_dungeon_recall_depth(player), ta->min_depth + 1);

	/* Going no deeper does not lower what it remembers. */
	player->depth = ta->min_depth;
	player_note_dungeon_depth(player);
	eq(first->max_depth, ta->min_depth + 1);

	first->max_depth = 0;
	player->dungeon = 0;
	player->depth = 0;

	ok;
}

/**
 * The world puts a way down within reach of the character who starts in it.
 *
 * The town staircase leads into the shallowest dungeon there is, so a new
 * character has somewhere to go from the first turn.  What must not happen is
 * that the shallowest dungeon starts below the depth a first-level character
 * can survive.
 */
static int test_the_first_dungeon_is_reachable(void *state) {
	struct dun_type *type, *shallowest = NULL;

	for (type = dun_types; type; type = type->next)
		if (!shallowest || type->min_depth < shallowest->min_depth)
			shallowest = type;

	notnull(shallowest);
	eq(shallowest->min_depth, 1);

	ok;
}


/** Which of a theme's four axes an object type is counted under. */
static int theme_axis_of(int tval) {
	switch (tval) {
		case TV_CHEST: case TV_CROWN: case TV_AMULET: case TV_RING:
		case TV_GOLD: case TV_DRAG_ARMOR: return 0;			/* treasure */
		case TV_SHOT: case TV_ARROW: case TV_BOLT: case TV_BOW:
		case TV_HAFTED: case TV_POLEARM: case TV_SWORD: case TV_BOOTS:
		case TV_GLOVES: case TV_HELM: case TV_SHIELD: case TV_CLOAK:
		case TV_SOFT_ARMOR: case TV_HARD_ARMOR: return 1;	/* combat */
		case TV_STAFF: case TV_WAND: case TV_ROD: case TV_SCROLL:
		case TV_POTION: case TV_MAGIC_BOOK: case TV_PRAYER_BOOK:
		case TV_NATURE_BOOK: case TV_SHADOW_BOOK: case TV_OTHER_BOOK:
			return 2;										/* magic */
		case TV_DIGGING: case TV_LIGHT: case TV_FLASK: case TV_FOOD:
		case TV_MUSHROOM: return 3;							/* tools */
		default: return -1;
	}
}

/** Generate objects at depth 40 in the named dungeon; tally them by axis. */
static int theme_sample(const char *dungeon, int *axis, int rolls) {
	struct chunk *c = cave_new(20, 20);
	struct dun_type *ty = dungeon ? dun_type_by_name(dungeon) : NULL;
	int made = 0, i;

	c->depth = 40;
	player->dungeon = ty ? ty->index + 1 : 0;

	for (i = 0; i < 4; i++) axis[i] = 0;

	for (i = 0; i < rolls; i++) {
		struct object *obj = make_object(c, 40, false, false, false, NULL, 0);
		int a;

		if (!obj) continue;
		made++;
		a = theme_axis_of(obj->tval);
		if (a >= 0) axis[a]++;
		object_delete(c, NULL, &obj);
	}

	player->dungeon = 0;
	cave_free(c);

	return made;
}

/**
 * A dungeon's theme shifts what it yields (CNT-12).
 *
 * This is what WLD-14 needs to be perceptible: thirteen dungeons that differ
 * only in their floor are thirteen of the same dungeon.  Zangband weighted the
 * base allocation by object type; 4.2's allocation is a precomputed cumulative
 * table that cannot be reweighted, so the same distribution is reached by
 * turning down objects that do not suit -- which is arithmetically the same
 * thing.
 *
 * Measured over 4000 rolls at depth 40: against a baseline of treasure 19%,
 * combat 22%, magic 50%, tools 8%, the Grove of the Unicorn gives magic 79%
 * and combat 7%, and Rebma gives treasure 32%.
 */
static int test_a_dungeon_theme_shifts_what_it_yields(void *state) {
	int rolls = 4000;
	int plain[4], magical[4], rich[4], warlike[4];
	int made;

	made = theme_sample(NULL, plain, rolls);
	require(made > rolls / 2);

	/* The Grove is a magic dungeon: more magic, and much less to fight with. */
	made = theme_sample("The Grove of the Unicorn", magical, rolls);
	require(made > rolls / 2);
	require(magical[2] > plain[2]);
	require(magical[1] * 2 < plain[1]);

	/* Rebma is a treasure dungeon. */
	made = theme_sample("Rebma", rich, rolls);
	require(made > rolls / 2);
	require(rich[0] > plain[0]);

	/* Garnath is a combat dungeon. */
	made = theme_sample("Garnath", warlike, rolls);
	require(made > rolls / 2);
	require(warlike[1] > plain[1]);
	require(warlike[0] < plain[0]);

	ok;
}

/**
 * A theme changes what a level yields, not how much (CNT-12).
 *
 * Turning objects down has to leave the count alone, or a themed dungeon would
 * quietly be a poorer one.  make_object() keeps the first thing it was offered
 * and falls back to it when every attempt is refused.
 */
static int test_a_theme_does_not_reduce_what_is_found(void *state) {
	int rolls = 2000;
	int plain[4], picky[4];
	int made_plain, made_picky;

	made_plain = theme_sample(NULL, plain, rolls);

	/* Tir-na Nog'th has the most lopsided theme in the file. */
	made_picky = theme_sample("Tir-na Nog'th", picky, rolls);

	require(made_plain > rolls / 2);

	/* Within a few per cent of each other, rather than merely "some". */
	require(made_picky * 100 > made_plain * 95);

	ok;
}

/**
 * Every kind of object belongs to one of the theme's four axes (CNT-12).
 *
 * The mapping is a switch over object types, so a type added later would fall
 * through to the default and be weighted by an average nobody chose.  This is
 * the test that says so at the time rather than leaving it to be noticed.
 */
static int test_the_theme_mapping_is_total(void *state) {
	/* Four distinct weights, none of them equal to their own average. */
	struct obj_theme probe = { 8, 24, 56, 72 };
	int average = (8 + 24 + 56 + 72) / 4;
	int tval;

	for (tval = 1; tval < TV_MAX; tval++) {
		int w = obj_theme_weight(&probe, tval);

		/*
		 * Every type must come out as one of the four, or as the sum the
		 * dragon scale mail case deliberately gives.  Landing on the average
		 * means it fell through the switch.
		 */
		if (w == average) {
			printf("object type %d (%s) is not in the theme mapping\n",
				tval, tval_find_name(tval));
		}
		require(w != average);
	}

	ok;
}


/** The share of monsters met at this depth that the dungeon is home to. */
static int dwell_sample(struct dun_type *ty, int depth, int rolls) {
	int at_home = 0, got = 0, i;
	bool was_wild = player->in_wild;
	int was_depth = player->depth;

	/*
	 * Stand the player in the dungeon, not merely name it: the habitat applies
	 * to a character who is underground, so that it cannot leak into the open
	 * country they walk home across.
	 */
	player->dungeon = ty ? ty->index + 1 : 0;
	player->in_wild = false;
	player->depth = depth;

	for (i = 0; i < rolls; i++) {
		struct monster_race *r = get_mon_num(depth, depth);

		if (!r) continue;
		got++;
		if (ty && dun_type_dwells(ty, r)) at_home++;
	}

	player->dungeon = 0;
	player->in_wild = was_wild;
	player->depth = was_depth;

	return got ? (100 * at_home) / got : -1;
}

/**
 * A dungeon is mostly inhabited by the things that live in it (CNT-05).
 *
 * Zangband carried a habitat flag for each of its dungeons on every one of its
 * nine hundred monsters.  Expressed here through 4.2's own monster bases and
 * flags instead, which says the same thing in a dozen lines per dungeon and
 * covers the monsters Angband brought as well as the ones Zangband did.
 *
 * Measured over 3000 rolls at depth 40: the Caverns of Kolvir give trolls 20%,
 * giants 11%; Tir-na Nog'th gives vortices 17%, wraiths 11%; against a base
 * distribution whose commonest kinds are people at 9% and dragons at 7%.
 */
static int test_a_dungeon_has_its_own_inhabitants(void *state) {
	struct dun_type *kolvir = dun_type_by_name("The Caverns of Kolvir");
	struct dun_type *tir = dun_type_by_name("Tir-na Nog'th");
	int home_kolvir, home_tir, cross;

	notnull(kolvir);
	notnull(tir);

	/* In its own dungeon, most of what is met belongs there. */
	home_kolvir = dwell_sample(kolvir, 40, 2000);
	home_tir = dwell_sample(tir, 40, 2000);
	require(home_kolvir > 40);
	require(home_tir > 40);

	/*
	 * And the two are genuinely different places: measured against Kolvir's
	 * list, what Tir-na Nog'th turns up is largely foreign.  Trolls and giants
	 * are not what walks a city in the sky.
	 */
	player->dungeon = tir->index + 1;
	player->in_wild = false;
	player->depth = 40;
	{
		int i, foreign = 0, got = 0;

		for (i = 0; i < 2000; i++) {
			struct monster_race *r = get_mon_num(40, 40);

			if (!r) continue;
			got++;
			if (!dun_type_dwells(kolvir, r)) foreign++;
		}
		cross = got ? (100 * foreign) / got : -1;
	}
	player->dungeon = 0;
	player->in_wild = true;
	player->depth = 0;

	require(cross > 70);

	ok;
}

/**
 * A dungeon can always be populated (CNT-05).
 *
 * Walks every dungeon from its shallowest level to its deepest and requires
 * that each can produce a monster.
 *
 * Note what this does and does not show.  Zangband zeroed the probability of
 * every monster without the dungeon's habitat flag; ZangbandTK scales it down
 * instead, so that a dungeon whose own kinds are thin at some depth still has
 * something to fall back on.  Setting the stranger share to zero and running
 * this test does *not* fail, so the hard filter would in fact work against the
 * data as it stands -- the alloc table offers every monster up to the current
 * depth, which is a wide enough pool that a dungeon is never actually empty.
 * The soft filter is kept for two reasons that are not this one: it survives
 * data changes that would thin a habitat out, and an occasional stranger a long
 * way from home is better flavour than a dungeon that is hermetically sealed.
 */
static int test_a_dungeon_can_always_be_populated(void *state) {
	int idx;

	for (idx = 0; idx < dun_type_count(); idx++) {
		struct dun_type *ty = dun_type_by_index(idx);
		int depth;

		notnull(ty);

		for (depth = ty->min_depth; depth <= ty->max_depth; depth++) {
			int i;

			player->dungeon = idx + 1;
			player->in_wild = false;
			player->depth = depth;

			/*
			 * Several rolls per level: one success could be luck, and it is
			 * the systematic emptiness that would matter.
			 */
			for (i = 0; i < 8; i++) {
				struct monster_race *r = get_mon_num(depth, depth);

				if (!r) {
					printf("%s has nothing to put on level %d\n",
						ty->name, depth);
				}
				notnull(r);
			}

			player->dungeon = 0;
			player->in_wild = true;
			player->depth = 0;
		}
	}

	ok;
}

/**
 * Every dungeon claims a share of the bestiary that means something (CNT-05).
 *
 * A habitat that matches most of the game is not a habitat.  This caught a real
 * mistake, and the way it caught it is worth recording: Garnath was given the
 * EVIL flag, which is 564 of the 1013 monsters.  Its commonest inhabitants came
 * out as dragons and townspeople -- exactly what any other dungeon gives -- and
 * measuring what share of the bestiary it claimed did *not* find the fault,
 * because within Garnath's own depths EVIL plus its bases came to about half,
 * which is not obviously wrong.
 *
 * What does find it is the breadth of the individual flags claimed.  ANIMAL is
 * 288 monsters and a forest may fairly say it is full of animals; EVIL is over
 * half of everything and says nothing at all.
 */
static int test_every_dungeon_claims_a_workable_share(void *state) {
	int idx, total = 0, i;

	for (i = 1; i < z_info->r_max; i++)
		if (r_info[i].name) total++;

	require(total > 100);

	for (idx = 0; idx < dun_type_count(); idx++) {
		struct dun_type *ty = dun_type_by_index(idx);
		int own = 0, eligible = 0, share, flag;

		notnull(ty);
		require(ty->has_dwellers);

		/* No single flag may stand for a great part of the bestiary. */
		for (flag = 1; flag < RF_MAX; flag++) {
			int matched = 0;

			if (!rf_has(ty->dweller_flags, flag)) continue;

			for (i = 1; i < z_info->r_max; i++)
				if (r_info[i].name && rf_has(r_info[i].flags, flag))
					matched++;

			if (matched * 100 > total * 40)
				printf("%s claims a flag matching %d of %d monsters\n",
					ty->name, matched, total);

			require(matched * 100 <= total * 40);
		}

		/* And it must be home to some of what it will meet, but not all. */
		for (i = 1; i < z_info->r_max; i++) {
			struct monster_race *r = &r_info[i];

			if (!r->name || r->level <= 0) continue;
			if (r->level < ty->min_depth || r->level > ty->max_depth) continue;

			eligible++;
			if (dun_type_dwells(ty, r)) own++;
		}

		require(eligible > 0);
		share = (100 * own) / eligible;

		if (share < 5 || share > 60)
			printf("%s is home to %d%% of the %d monsters at its depths\n",
				ty->name, share, eligible);

		/*
		 * Measured range with the data as it stands: 8% (Faiella-Bionin) to
		 * 46% (Arden, which is a forest and may fairly claim the animals).
		 */
		require(share >= 5);
		require(share <= 60);
	}

	ok;
}


/**
 * Every dungeon can be entered at its own mouth, and left again (WLD-14).
 *
 * The depth clamp treated "above the top of this dungeon" as one case when it
 * is two.  Going up it means out, to the surface.  Going down it means the
 * mouth -- and clamping that to the surface made eleven of the thirteen
 * dungeons impossible to enter: stepping onto the mouth of the Abyss asked for
 * depth 1, which is above its top of 90, and got depth 0 back, so the player
 * spent a move and stayed where they were.
 *
 * The earlier test for this checked only the first dungeon in the file, which
 * starts at depth 1 and was therefore the single case that worked.  This walks
 * all of them.
 */
static int test_every_dungeon_can_be_entered_and_left(void *state) {
	int idx;

	require(dun_type_count() > 1);

	for (idx = 0; idx < dun_type_count(); idx++) {
		struct dun_type *ty = dun_type_by_index(idx);
		int in, out, deeper, floor_of_it;

		notnull(ty);
		player->dungeon = idx + 1;

		/* Stepping onto its mouth from the surface reaches its top level. */
		in = dungeon_get_next_level(player, 0, 1);
		if (in != ty->min_depth)
			printf("%s (%d-%d) is entered at depth %d\n", ty->name,
				ty->min_depth, ty->max_depth, in);
		eq(in, ty->min_depth);

		/* Going up from its top level reaches the surface, not its top again. */
		out = dungeon_get_next_level(player, ty->min_depth, -1);
		eq(out, 0);

		/* Going down inside it descends, as far as its bottom and no further. */
		if (ty->max_depth > ty->min_depth) {
			deeper = dungeon_get_next_level(player, ty->min_depth, 1);
			require(deeper > ty->min_depth);
			require(deeper <= ty->max_depth);
		}

		floor_of_it = dungeon_get_next_level(player, ty->max_depth, 1);
		eq(floor_of_it, ty->max_depth);

		player->dungeon = 0;
	}

	ok;
}

/**
 * A dungeon's inhabitants stay in their dungeon (CNT-05).
 *
 * player->dungeon is not cleared when the player comes back up -- word of recall
 * needs it -- so it says where they were, not where they are.  The habitat test
 * originally keyed off the requested monster level alone, and wild_populate()
 * asks at the block's danger level, which is above zero.  So a character who
 * had been down the Courts of Chaos came up to find the fields full of demons.
 */
static int test_a_habitat_does_not_leak_into_the_country(void *state) {
	struct dun_type *courts = dun_type_by_name("The Courts of Chaos");
	int at_home_below, at_home_above, i;
	int got;

	notnull(courts);

	/* Down in the Courts, most of what is met belongs to the Courts. */
	player->dungeon = courts->index + 1;
	player->in_wild = false;
	player->depth = 80;

	at_home_below = 0;
	got = 0;
	for (i = 0; i < 1500; i++) {
		struct monster_race *r = get_mon_num(80, 80);

		if (!r) continue;
		got++;
		if (dun_type_dwells(courts, r)) at_home_below++;
	}
	require(got > 500);
	at_home_below = (100 * at_home_below) / got;

	/*
	 * Back on the surface, having been there: the country is populated at its
	 * own danger level, and owes the Courts nothing.
	 */
	player->in_wild = true;
	player->depth = 0;

	at_home_above = 0;
	got = 0;
	for (i = 0; i < 1500; i++) {
		struct monster_race *r = get_mon_num(80, 80);

		if (!r) continue;
		got++;
		if (dun_type_dwells(courts, r)) at_home_above++;
	}
	require(got > 500);
	at_home_above = (100 * at_home_above) / got;

	player->dungeon = 0;

	if (at_home_above * 2 > at_home_below)
		printf("the Courts' own kinds are %d%% of what is met underground "
			   "and %d%% of what is met on the surface\n",
			at_home_below, at_home_above);

	/* Far less of it above ground than below. */
	require(at_home_above * 2 < at_home_below);

	ok;
}

/**
 * The window survives a save, so the first scroll after loading does not jump.
 *
 * wild_window and wild_scroll are worked out when a window is built.  A
 * character loaded on the surface has their offset restored from the savefile
 * without wild_surface() ever running, so neither was set: the first rebuild
 * re-anchored both axes and reported no movement to the display, reintroducing
 * both of the bugs they exist to prevent -- on the first scroll after every
 * load.
 */
static int test_the_window_survives_a_save(void *state) {
	int size = z_info->wild_block_size;
	int span = wild_view_blocks() * size;
	struct loc here, first, second;
	struct chunk *a, *b;

	/* Stand well inside the world and build a window there. */
	here = loc(span * 4 + size * 3 + 5, span * 4 + size * 3 + 5);
	a = wild_surface(wild, player, here, &first);
	notnull(a);
	cave_free(a);

	player->wild_grid = here;
	player->wild_offset = first;
	player->in_wild = true;

	require(savefile_save("Test-window"));

	play_again = true;
	wipe_mon_list(cave, player);
	cleanup_angband();
	chunk_list_max = 0;
	init_angband();
	play_again = false;

	require(savefile_load("Test-window", false));

	/*
	 * Now walk west far enough that the window must follow.  The axis that did
	 * not need to move must stay put -- which it can only do if the loaded
	 * offset was adopted.
	 */
	here = player->wild_grid;
	here.x = player->wild_offset.x + 1;

	b = wild_surface(wild, player, here, &second);
	notnull(b);
	cave_free(b);

	require(second.x < first.x);
	eq(second.y, first.y);

	/* And the display is told how far it moved, rather than being told nothing. */
	eq(wild_scroll_delta().x, first.x - second.x);
	require(wild_scroll_delta().x != 0);

	file_delete("Test-window");

	ok;
}

/**
 * The village band and the town constants agree (WLD-11).
 *
 * wild_town_bands[0] is 66x22 and so are world:town-wid and world:town-hgt in
 * constants.txt.  That is not a coincidence -- the starting village *is*
 * Angband's town, at Angband's size -- but the two are written down separately,
 * so a change to one would leave the other behind.  Deriving the band from the
 * constants would be worse: it would let a large town-wid invert the size ladder
 * silently.  So they are checked against each other instead.
 */
static int test_the_village_is_angbands_town_size(void *state) {
	int i;

	require(wild_town_count(wild) > 0);
	eq(wild->towns[0].band, 0);
	eq(wild->towns[0].wid, z_info->town_wid);
	eq(wild->towns[0].hgt, z_info->town_hgt);

	/* And every larger band really is larger, in both directions. */
	for (i = 1; i < wild_town_count(wild); i++) {
		struct wild_town *town = &wild->towns[i];

		if (town->band == 0) {
			eq(town->wid, z_info->town_wid);
			eq(town->hgt, z_info->town_hgt);
		} else {
			require(town->wid > z_info->town_wid);
			require(town->hgt > z_info->town_hgt);
		}
	}

	ok;
}



/**
 * The road on the ground is continuous across block boundaries (WLD-08).
 *
 * The block map says which blocks carry a road; the drawing turns that into
 * grids, joining the middle of a block to the middle of each neighbouring block
 * that carries one.  Those are two different things, and a road that is
 * connected on the map but broken on the ground is no use for walking along --
 * which is the whole point of it.  The stub drew every road block as an
 * east-west line, which would have left a road that turns north as a column of
 * disconnected dashes.
 */
static int test_the_road_is_continuous_on_the_ground(void *state) {
	int size = z_info->wild_block_size;
	int seams[2] = { 0, 0 }, broken = 0;
	int axis;

	/*
	 * Both directions.  East-west alone proves nothing: the stub drew every
	 * road block as a full east-west line, so it passed that check while
	 * leaving a road that turns north as a column of disconnected dashes.
	 */
	for (axis = 0; axis < 2; axis++) {
		int bx, by;

		for (by = 1; by < wild->blocks - 2 && seams[axis] < 20; by++)
			for (bx = 1; bx < wild->blocks - 2 && seams[axis] < 20; bx++) {
				int nx = bx + (axis ? 0 : 1), ny = by + (axis ? 1 : 0);
				struct loc offset;
				struct chunk *c;
				int k;
				bool gap = false;

				if (!wild_road_at(wild, bx, by)) continue;
				if (!wild_road_at(wild, nx, ny)) continue;

				/* A town or a dungeon mouth is drawn over the road on purpose. */
				if (wild_block_at(wild, bx, by)->place) continue;
				if (wild_block_at(wild, nx, ny)->place) continue;

				c = wild_surface(wild, player,
								 loc(bx * size + size, by * size + size),
								 &offset);
				notnull(c);

				/* Walk from one block's middle to the next one's. */
				for (k = 0; k <= size; k++) {
					struct loc g = axis
						? loc(bx * size + size / 2 - offset.x,
							  by * size + size / 2 + k - offset.y)
						: loc(bx * size + size / 2 + k - offset.x,
							  by * size + size / 2 - offset.y);

					if (!square_in_bounds_fully(c, g) ||
						square(c, g)->feat != FEAT_ROAD) {
						gap = true;
						break;
					}
				}

				cave_free(c);
				seams[axis]++;

				if (gap) {
					broken++;
					if (broken <= 3)
						printf("the road breaks between blocks %d,%d and "
							   "%d,%d (%s)\n", bx, by, nx, ny,
							axis ? "north-south" : "east-west");
				}
			}
	}

	/* There has to be some road of each kind to have checked, and none broken. */
	require(seams[0] > 3);
	require(seams[1] > 3);
	eq(broken, 0);

	ok;
}


/**
 * Every town and every dungeon can be walked to along roads (WLD-08, WLD-14).
 *
 * This is the property the world has to have for its size to be usable.  A
 * dungeon nobody can find is a dungeon nobody uses -- and measurement said
 * that was the case: before the mouths were given roads of their own, six of
 * the thirteen happened to sit on one and the rest were between eleven and
 * sixty-two blocks away, which is up to a thousand grids of open country to
 * search with nothing to follow.
 *
 * Siting cannot fix that.  A dungeon stands in the kind of country it belongs
 * in, and the deep ones belong a long way from any town.  So the road goes to
 * them instead.
 */
static int test_roads_reach_every_place(void *state) {
	int blocks = wild->blocks;
	bool *seen = mem_zalloc(blocks * blocks * sizeof(*seen));
	int *queue = mem_zalloc(blocks * blocks * sizeof(*queue));
	int head = 0, tail = 0, i;
	int start;

	require(wild_town_count(wild) > 1);
	require(wild_dungeon_count(wild) > 0);

	/* Flood out from the village the character starts in, across roads only. */
	start = wild->towns[0].block.y * blocks + wild->towns[0].block.x;
	require(wild_road_at(wild, wild->towns[0].block.x, wild->towns[0].block.y));
	seen[start] = true;
	queue[tail++] = start;

	while (head < tail) {
		int node = queue[head++];
		int bx = node % blocks, by = node / blocks;
		static const int dx[4] = { -1, 1, 0, 0 };
		static const int dy[4] = { 0, 0, -1, 1 };

		for (i = 0; i < 4; i++) {
			int nx = bx + dx[i], ny = by + dy[i], next;

			if (!wild_road_at(wild, nx, ny)) continue;
			next = ny * blocks + nx;
			if (seen[next]) continue;
			seen[next] = true;
			queue[tail++] = next;
		}
	}

	for (i = 0; i < wild_town_count(wild); i++) {
		struct wild_town *town = &wild->towns[i];

		if (!seen[town->block.y * blocks + town->block.x])
			printf("town %d is not on the road network\n", i);
		require(seen[town->block.y * blocks + town->block.x]);
	}

	for (i = 0; i < wild_dungeon_count(wild); i++) {
		struct wild_dungeon *mouth = wild_dungeon_by_index(wild, i);
		struct dun_type *ty = dun_type_by_index(mouth->type);

		if (!seen[mouth->block.y * blocks + mouth->block.x])
			printf("%s is not on the road network\n",
				ty ? ty->name : "a dungeon");
		require(seen[mouth->block.y * blocks + mouth->block.x]);
	}

	mem_free(queue);
	mem_free(seen);

	ok;
}


/** Count the monsters standing in town zero, by what kind of thing they are. */
static void folk_tally(int *people, int *beasts, int *others) {
	struct loc org = loc(wild_town_origin(wild).x - player->wild_offset.x,
						 wild_town_origin(wild).y - player->wild_offset.y);
	struct loc grid;

	*people = 0;
	*beasts = 0;
	*others = 0;

	for (grid.y = org.y; grid.y < org.y + wild->towns[0].hgt; grid.y++)
		for (grid.x = org.x; grid.x < org.x + wild->towns[0].wid; grid.x++) {
			struct monster *mon;
			const char *base;

			if (!square_in_bounds_fully(cave, grid)) continue;

			mon = square_monster(cave, grid);
			if (!mon || !mon->race || !mon->race->base) continue;

			base = mon->race->base->name;

			if (streq(base, "townsfolk") || streq(base, "person") ||
				streq(base, "humanoid"))
				++*people;
			else if (rf_has(mon->race->flags, RF_ANIMAL))
				++*beasts;
			else
				++*others;
		}
}

/**
 * Who is put in a town's streets follows who lives there (WLD-11).
 *
 * Tested by changing what town zero is and filling it again, because town zero
 * is the only town reliably inside the live window and it is villagers by fiat.
 *
 * Note what is *not* tested: that a villager town contains no animals at all.
 * It does contain some, and correctly -- Angband's filthy street urchin carries
 * "friends:50:2d1:Scrawny cat" and "friends:50:2d1:Scruffy little dog", and
 * named companions are placed by name rather than drawn from the allocator, so
 * no filter reaches them.  Urchins with a stray cat and a scruffy dog is the
 * game's own joke and belongs in a village.  Measured either way, a villager
 * town comes out with about seventy people and somewhere between nought and
 * thirteen animals whether the filter is applied or not, so purity would have
 * been a test that agreed with a filter that did nothing.
 */
static int test_a_towns_streets_follow_its_inhabitants(void *state) {
	int was = wild->towns[0].folk;
	int p0, b0, o0, p1, b1, o1, i;

	eq(was, WILD_FOLK_VILLAGER);

	/* Villagers: filling the streets adds people. */
	folk_tally(&p0, &b0, &o0);
	for (i = 0; i < 8; i++)
		wild_town_people(wild, player, cave, player->wild_offset);
	folk_tally(&p1, &b1, &o1);

	require(p1 - p0 > 10);
	require(p1 - p0 > b1 - b0);

	/* Beasts: filling them adds animals, and hardly any people. */
	wild->towns[0].folk = WILD_FOLK_BEAST;
	folk_tally(&p0, &b0, &o0);
	for (i = 0; i < 8; i++)
		wild_town_people(wild, player, cave, player->wild_offset);
	folk_tally(&p1, &b1, &o1);

	if (b1 - b0 <= p1 - p0)
		printf("a beast town gained %d animals and %d people\n",
			b1 - b0, p1 - p0);

	require(b1 - b0 > 10);
	require(b1 - b0 > (p1 - p0) * 2);

	/* Abandoned: filling them adds nobody. */
	wild->towns[0].folk = WILD_FOLK_ABANDONED;
	folk_tally(&p0, &b0, &o0);
	for (i = 0; i < 8; i++)
		wild_town_people(wild, player, cave, player->wild_offset);
	folk_tally(&p1, &b1, &o1);

	eq(p1, p0);
	eq(b1, b0);
	eq(o1, o0);

	wild->towns[0].folk = was;

	ok;
}

/**
 * The status line names the place, not the level (WLD-11).
 *
 * Depth zero is the whole world here rather than a town, so the "Town" Angband
 * shows there was wrong nearly everywhere it appeared: it said the same thing a
 * thousand grids out in open country as it did in the market square.  Standing
 * in a place it now names its size, which is also the useful thing to know
 * before walking in -- a village keeps four trades and a great city all eight.
 */
static int test_the_status_line_names_the_place(void *state) {
	struct loc world = loc(player->grid.x + player->wild_offset.x,
						   player->grid.y + player->wild_offset.y);
	int here = wild_town_here(wild, world);
	int i;

	/* The character starts in the village, so that is what it should say. */
	require(here >= 0);
	eq(here, 0);
	require(streq(wild_band_name(wild->towns[here].band), "village"));

	/* A grid well outside any town is in no town. */
	{
		struct loc away = loc(world.x, world.y);
		bool found = false;

		for (i = 200; i < 900 && !found; i += 50) {
			away.y = world.y + i;
			if (away.y >= wild_world_grids()) break;
			if (wild_town_here(wild, away) < 0) found = true;
		}

		require(found);
	}

	/* Every band has a word, and they are all different. */
	for (i = 0; i < 4; i++) {
		int j;

		require(strlen(wild_band_name(i)) > 0);

		/* Short enough for the field the status line puts it in. */
		require(strlen(wild_band_name(i)) < 13);

		for (j = 0; j < i; j++)
			require(!streq(wild_band_name(i), wild_band_name(j)));
	}

	/*
	 * And the answer is grid-precise, not block-precise: the block test counts
	 * ground outside the wall, which is not the same question.
	 */
	{
		struct loc org = wild_town_origin_of(wild, 0);
		struct loc corner = loc(org.x - 1, org.y - 1);

		if (corner.x >= 0 && corner.y >= 0)
			require(wild_town_here(wild, corner) < 0);
	}

	ok;
}


/** Is this one of the names town.txt lists for country that has fallen? */
static bool name_is_lawless(const char *name) {
	int i;

	for (i = 0; i < town_names.lawless_count; i++)
		if (streq(name, town_names.lawless[i])) return true;

	return false;
}

/**
 * Towns have names, and no two in a world share one (WLD-11).
 *
 * Zangband named its towns and this did not, which was a real gap against the
 * reference rather than something Zangband left undone.  Its own scheme was a
 * generated elvish name with a size suffix -- "-ville", " Dun", "-ton",
 * "-ford" -- which is a name generator for Middle-earth; DEC-30 points the
 * other way, so the names are curated in town.txt instead.
 *
 * What a place is called also says something about it: a town that has fallen or
 * stands empty takes its name from the lawless list.
 */
static int test_towns_have_names(void *state) {
	int seed;

	require(town_names.settled_count > 0);
	require(town_names.lawless_count > 0);

	/* Enough names to go round, which the parser also insists on. */
	require(town_names.settled_count + town_names.lawless_count >=
			(int) z_info->wild_towns);

	for (seed = 1; seed <= 12; seed++) {
		struct wilderness *w = wild_new(129, seed * 7717);
		int i, j, fallen = 0, fallen_named = 0;

		wild_generate(w);
		require(w->town_count > 1);

		for (i = 0; i < w->town_count; i++) {
			struct wild_town *town = &w->towns[i];
			bool has_fallen = (town->folk == WILD_FOLK_MONSTER ||
							   town->folk == WILD_FOLK_ABANDONED);

			/* Every town is called something. */
			if (!town->name)
				printf("town %d of world %d has no name\n", i, seed);
			notnull(town->name);

			/* And nothing else in the world is called that. */
			for (j = 0; j < i; j++) {
				if (streq(town->name, w->towns[j].name))
					printf("world %d has two towns called %s\n", seed,
						town->name);
				require(!streq(town->name, w->towns[j].name));
			}

			/*
			 * A fallen town takes a lawless name while any is free.  Counted
			 * rather than required of each: with six lawless names and a world
			 * that happens to have seven fallen towns, the seventh borrows.
			 */
			if (has_fallen) {
				fallen++;
				if (name_is_lawless(town->name)) fallen_named++;
			}
		}

		if (fallen > 0 && fallen <= town_names.lawless_count) {
			if (fallen_named != fallen)
				printf("world %d: %d fallen towns, %d with lawless names\n",
					seed, fallen, fallen_named);
			eq(fallen_named, fallen);
		}

		wild_free(w);
	}

	ok;
}

/**
 * A world always calls its towns the same thing (WLD-11).
 *
 * The names are not saved: the world regenerates from its seed, so they have to
 * come back identical or a character would come home to somewhere else.
 */
static int test_town_names_come_back_the_same(void *state) {
	char *first[WILD_TOWNS_MAX];
	struct wilderness *w;
	int count, i;

	w = wild_new(129, 90210);
	wild_generate(w);
	count = w->town_count;
	require(count > 1);
	for (i = 0; i < count; i++)
		first[i] = string_make(w->towns[i].name);
	wild_free(w);

	/* The same seed, built again from nothing. */
	w = wild_new(129, 90210);
	wild_generate(w);
	eq(w->town_count, count);
	for (i = 0; i < count; i++) {
		if (!streq(first[i], w->towns[i].name))
			printf("town %d was %s and is now %s\n", i, first[i],
				w->towns[i].name);
		require(streq(first[i], w->towns[i].name));
	}
	wild_free(w);

	for (i = 0; i < count; i++)
		string_free(first[i]);

	ok;
}


/**
 * A magetower stands where a magetower would be built (WLD-15, WLD-16).
 *
 * Scored on the country and the size of the place, as the trades are: it is the
 * work of people with the leisure and the order to build one, so it wants
 * population and law both.  Never in a village, since a tower nobody can reach
 * is no use, and never in a town that has fallen -- nobody is running a teleport
 * network out of somewhere held by monsters.
 */
static int test_a_magetower_stands_where_it_should(void *state) {
	int seed, with = 0, towns = 0;

	for (seed = 1; seed <= 10; seed++) {
		struct wilderness *w = wild_new(129, seed * 7717);
		int i;

		wild_generate(w);

		/*
		 * The starting village always has one, whatever its size and country
		 * say: every journey begins at home, and a network you cannot leave
		 * from has one node fewer than it needs.  So it is exempt from the
		 * rules below rather than a counter-example to them.
		 */
		require(w->towns[0].services & (1u << WILD_SERVICE_MAGETOWER));

		for (i = 1; i < w->town_count; i++) {
			struct wild_town *town = &w->towns[i];
			bool has = (town->services & (1u << WILD_SERVICE_MAGETOWER)) != 0;

			towns++;
			if (has) with++;

			/* Never in a village other than the starting one. */
			if (has && town->band < 1)
				printf("a village keeps a magetower\n");
			if (has) require(town->band >= 1);

			/* Nor anywhere that has fallen. */
			if (has && (town->folk == WILD_FOLK_MONSTER ||
						town->folk == WILD_FOLK_ABANDONED))
				printf("a %s town keeps a magetower\n",
					wild_folk_name(town->folk));
			if (has) require(town->folk != WILD_FOLK_MONSTER);
			if (has) require(town->folk != WILD_FOLK_ABANDONED);
		}

		wild_free(w);
	}

	/*
	 * Most of them.  Anything above a village keeps one, so what is left out is
	 * the villages and the towns that have fallen -- measured at eleven of
	 * twelve on one world, the twelfth being an abandoned town.
	 */
	require(with > 0);
	require(with * 2 > towns);

	ok;
}

/**
 * The magetower carries you to places you already know (WLD-16c).
 *
 * Two different bars, deliberately.  A town has to have been stood in -- seeing
 * it across a field is not being there, and the first crossing of the world
 * should stay worth making.  A dungeon mouth only has to have been seen, since
 * it is a staircase in a field with nothing to be inside of.
 */
static int test_the_tower_offers_only_known_places(void *state) {
	struct wild_place places[WILD_TOWNS_MAX + WILD_DUNGEONS_MAX];
	struct loc org = wild_town_origin_of(wild, 0);
	/*
	 * Asked from the middle of the starting village rather than from wherever
	 * the player happens to be: earlier tests in this suite move them about,
	 * and this is a question about the world, not about them.
	 */
	struct loc from = loc(org.x + wild->towns[0].wid / 2,
						  org.y + wild->towns[0].hgt / 2);
	int before, after, i;

	/* Forget everything, so the starting state is known. */
	for (i = 0; i < wild_town_count(wild); i++)
		wild->towns[i].visited = 0;

	before = wild_travel_places(wild, from, places,
								(int) N_ELEMENTS(places));

	/* Nothing is offered until somewhere has been visited. */
	for (i = 0; i < before; i++)
		require(streq(places[i].what, "dungeon"));

	/* Mark a town other than this one as visited; it appears. */
	{
		int other = -1;

		for (i = 0; i < wild_town_count(wild); i++)
			if (i != wild_town_here(wild, from)) { other = i; break; }

		require(other >= 0);
		wild->towns[other].visited = 1;

		after = wild_travel_places(wild, from, places,
								   (int) N_ELEMENTS(places));
		eq(after, before + 1);
	}

	/* The town the player is standing in is never offered. */
	{
		int here = wild_town_here(wild, from);

		require(here >= 0);
		wild->towns[here].visited = 1;

		after = wild_travel_places(wild, from, places,
								   (int) N_ELEMENTS(places));

		for (i = 0; i < after; i++)
			require(!streq(places[i].what, "dungeon") ||
					!loc_eq(places[i].grid, from));

		/* Marking where we already are adds nothing to travel to. */
		eq(after, before + 1);
	}

	ok;
}

/**
 * The fare rises with the distance (WLD-16c).
 */
static int test_the_fare_rises_with_the_distance(void *state) {
	int size = z_info->wild_block_size;
	struct loc from = loc(size * 40, size * 40);
	int32_t near_fare = wild_travel_cost(wild, from, loc(size * 45, size * 40));
	int32_t far_fare = wild_travel_cost(wild, from, loc(size * 90, size * 40));

	/* Nothing is free... */
	require(near_fare > 0);

	/* ...and the long way costs more, in proportion. */
	require(far_fare > near_fare * 5);

	/* A step within one block still costs the minimum rather than nothing. */
	require(wild_travel_cost(wild, from, loc(from.x + 2, from.y)) > 0);

	ok;
}

/**
 * Walking into a town is what makes it a destination (WLD-16c).
 */
static int test_walking_into_a_town_is_remembered(void *state) {
	struct loc org = wild_town_origin_of(wild, 0);
	struct loc middle = loc(org.x + wild->towns[0].wid / 2,
							org.y + wild->towns[0].hgt / 2);
	int i;

	for (i = 0; i < wild_town_count(wild); i++)
		wild->towns[i].visited = 0;

	/* Somewhere out in the country marks nothing. */
	wild_note_visit(wild, loc(org.x, org.y - 40));
	eq(wild->towns[0].visited, 0);

	/* Standing in the town marks it. */
	wild_note_visit(wild, middle);
	eq(wild->towns[0].visited, 1);

	/*
	 * And leave it set.  The starting village counting as visited is a fact
	 * about the world rather than something this test owns, and a later test
	 * checks it -- clearing the flags and walking away broke that one.
	 */
	for (i = 0; i < wild_town_count(wild); i++)
		if (i == 0) wild->towns[i].visited = 1;

	ok;
}


/**
 * No road goes nowhere (WLD-08).
 *
 * Every end of the road network is a town or a dungeon mouth.  A road that
 * simply stops in open country is a road somebody built to nowhere, and the
 * player walking it has no way to know it was never going to arrive.
 */
static int test_no_road_goes_nowhere(void *state) {
	int seed;

	for (seed = 1; seed <= 6; seed++) {
		struct wilderness *w = wild_new(129, seed * 7717);
		int x, y, roads = 0, ends = 0;

		wild_generate(w);

		for (y = 0; y < w->blocks; y++)
			for (x = 0; x < w->blocks; x++) {
				static const int dx[4] = { -1, 1, 0, 0 };
				static const int dy[4] = { 0, 0, -1, 1 };
				int i, neighbours = 0;

				if (!wild_road_at(w, x, y)) continue;
				roads++;

				for (i = 0; i < 4; i++)
					if (wild_road_at(w, x + dx[i], y + dy[i])) neighbours++;

				if (neighbours > 1) continue;

				ends++;

				if (wild_town_at(w, x, y) < 0 &&
					!wild_dungeon_in_block(w, x, y))
					printf("world %d: a road ends at block %d,%d, which is "
						   "neither a town nor a dungeon\n", seed, x, y);

				require(wild_town_at(w, x, y) >= 0 ||
						wild_dungeon_in_block(w, x, y));
			}

		require(roads > 100);
		require(ends > 0);
		wild_free(w);
	}

	ok;
}

/**
 * The road runs up to a gate, not to a blank wall (WLD-08).
 *
 * Reported from play: walking a road a long way and coming to a dead end.  It
 * was not a dead end at block level -- every road ends at a town -- but on the
 * ground the road stopped at the wall and the gate could be twenty-five grids
 * along it, which amounts to the same thing when you are standing there.
 *
 * Measured before the fix: about half the towns of a world had their road arrive
 * within two grids of a gate, and the rest between twelve and twenty-six.  Two
 * changes together: the gate is cut where the road arrives rather than at the
 * middle of the side, and the last stretch of road is drawn from each gate out
 * along the wall until it meets the road that came for it.
 */
static int test_the_road_runs_up_to_a_gate(void *state) {
	int idx, checked = 0, worst = 0;

	require(wild_town_count(wild) > 1);

	for (idx = 0; idx < wild_town_count(wild); idx++) {
		struct loc org = wild_town_origin_of(wild, idx);
		struct loc centre = loc(org.x + wild->towns[idx].wid / 2,
								org.y + wild->towns[idx].hgt / 2);
		struct loc offset;
		struct chunk *c = wild_surface(wild, player, centre, &offset);
		struct loc gates[32];
		struct loc g;
		int w = wild->towns[idx].wid, h = wild->towns[idx].hgt;
		int gate_count = 0, best = 9999;

		notnull(c);

		/* Every gate, once. */
		for (g.y = org.y; g.y < org.y + h; g.y++)
			for (g.x = org.x; g.x < org.x + w; g.x++) {
				struct loc at = loc(g.x - offset.x, g.y - offset.y);

				if (!square_in_bounds_fully(c, at)) continue;
				if (!square_iscloseddoor(c, at) && !square_isopendoor(c, at))
					continue;
				if (gate_count < (int) N_ELEMENTS(gates)) gates[gate_count++] = g;
			}

		/*
		 * The nearest gate to where the road network arrives -- which is what
		 * the traveller has to walk.  Measured on the ring outside the town's
		 * ground, since the town's own outer ring keeps whatever the road
		 * drawing left there and would flatter the answer.
		 */
		for (g.y = org.y - 1; g.y <= org.y + h; g.y++)
			for (g.x = org.x - 1; g.x <= org.x + w; g.x++) {
				struct loc out = loc(g.x - offset.x, g.y - offset.y);
				bool edge = (g.x == org.x - 1 || g.x == org.x + w ||
							 g.y == org.y - 1 || g.y == org.y + h);
				int k;

				if (!edge) continue;
				if (!square_in_bounds_fully(c, out)) continue;
				if (square(c, out)->feat != FEAT_ROAD) continue;

				for (k = 0; k < gate_count; k++) {
					int d = distance(g, gates[k]);

					if (d < best) best = d;
				}
			}

		cave_free(c);

		/* A town no road reaches at all is a separate matter, tested above. */
		if (best == 9999 || !gate_count) continue;

		checked++;
		if (best > worst) worst = best;

		if (best > 4)
			printf("%s: the road leaves you %d grids from the nearest gate\n",
				wild->towns[idx].name ? wild->towns[idx].name : "a town", best);

		/*
		 * Within a few grids.  Measured before the gates were cut where the
		 * roads arrive: about half the towns of a world were within two and the
		 * rest between twelve and twenty-six, which is the block size and
		 * multiples of it.
		 */
		require(best <= 4);
	}

	require(checked > 1);

	ok;
}

/**
 * The village a character began in is always somewhere they have been (WLD-16c).
 *
 * Reported from play: standing in a magetower with two towns behind them and
 * neither offered.  The visited flag was only ever set by taking a step, and
 * this character's steps into their own village were taken in an earlier version
 * that had no flag to set -- so the savefile faithfully recorded that they had
 * been nowhere.
 *
 * Every character begins on the starting village's staircase, so this is a fact
 * about the game rather than something to be recorded and possibly missed.  Set
 * on loading as well as on generating a surface, because loading a character who
 * is standing on the surface does not generate one: the level comes back from
 * the savefile.
 */
static int test_the_starting_village_is_always_known(void *state) {
	struct wild_place places[WILD_TOWNS_MAX + WILD_DUNGEONS_MAX];
	struct loc org = wild_town_origin_of(wild, 1);
	struct loc from;
	int n, i;
	bool offered = false;

	require(wild_town_count(wild) > 1);

	/* It counts as visited without anybody having walked anywhere. */
	eq(wild->towns[0].visited, 1);

	/* And so it is offered, asked from somewhere that is not it. */
	from = loc(org.x + wild->towns[1].wid / 2, org.y + wild->towns[1].hgt / 2);
	n = wild_travel_places(wild, from, places, (int) N_ELEMENTS(places));

	for (i = 0; i < n; i++)
		if (wild->towns[0].name && places[i].name &&
			streq(places[i].name, wild->towns[0].name))
			offered = true;

	if (!offered)
		printf("the starting village is not offered from elsewhere\n");
	require(offered);

	ok;
}

/**
 * The surface says whether it is day or night (WLD-24).
 *
 * Angband has a day and a night and never says which it is, because its surface
 * is one town-sized level you can see all of regardless.  Here daylight is what
 * reveals the country: measured on a real savefile at night, of 20736 grids in
 * the window 3895 were glowing and 17 were seen.  That is correct, and
 * indistinguishable from a broken map if nothing on screen says what hour it is.
 *
 * The status line only prints it above ground, so this checks both the hour and
 * the condition, since a claim about the display is what the report was about.
 */
static int test_the_surface_knows_the_hour(void *state) {
	int32_t was = turn;
	int day = 10L * z_info->day_length;
	bool lit, dark;

	require(player->in_wild);

	/* Noon, and midnight. */
	turn = day * 3 + day / 4;
	lit = is_daytime();

	turn = day * 3 + (3 * day) / 4;
	dark = is_daytime();

	turn = was;

	/* The two halves of the day differ, which is what there is to report. */
	require(lit);
	require(!dark);

	ok;
}

/**
 * A town's services are built into it, and are the ones it holds (WLD-16c).
 *
 * struct wild_town::services is only a promise until the generator keeps it, and
 * the two are written in different files -- the same gap the shops had, and
 * caught the same way: build the town and look for the doors.
 */
static int test_a_towns_services_are_built(void *state) {
	int idx, checked = 0;

	for (idx = 0; idx < wild_town_count(wild) && checked < 6; idx++) {
		struct wild_town *town = &wild->towns[idx];
		struct chunk *c;
		struct loc g;
		uint16_t found = 0;

		if (!town->services) continue;

		c = town_gen_wild(player,
						  wild_block_seed(wild, town->block.x, town->block.y),
						  town->wid, town->hgt, town->stores, town->services);
		notnull(c);

		for (g.y = 0; g.y < c->height; g.y++)
			for (g.x = 0; g.x < c->width; g.x++) {
				int at = wild_service_at(c, g);

				if (at >= 0) found |= 1u << at;
			}

		/* Everything it was given stands in it... */
		eq(found & town->services, town->services);

		/* ...and nothing it was not. */
		eq(found & ~town->services, 0);

		cave_free(c);
		checked++;
	}

	require(checked > 1);

	ok;
}

/**
 * The services a town holds follow its size, and say so plainly (WLD-16c).
 */
static int test_services_follow_the_size(void *state) {
	int seed;

	for (seed = 1; seed <= 10; seed++) {
		struct wilderness *w = wild_new(129, seed * 7717);
		int i;

		wild_generate(w);

		for (i = 1; i < w->town_count; i++) {
			struct wild_town *town = &w->towns[i];
			bool fallen = (town->folk == WILD_FOLK_MONSTER ||
						   town->folk == WILD_FOLK_ABANDONED);

			/* Nothing at all in a village, or anywhere that has fallen. */
			if (town->band < 1 || fallen) {
				eq(town->services, 0);
				continue;
			}

			/* A town: a way out, a bed, and somebody to mend you. */
			require(town->services & (1u << WILD_SERVICE_MAGETOWER));
			require(town->services & (1u << WILD_SERVICE_HEALER));
			require(town->services & (1u << WILD_SERVICE_INN));

			/* A city adds the magesmith and the recharger; a town has neither. */
			if (town->band >= 2) {
				require(town->services & (1u << WILD_SERVICE_ENCHANT));
				require(town->services & (1u << WILD_SERVICE_RECHARGE));
			} else {
				require(!(town->services & (1u << WILD_SERVICE_ENCHANT)));
				require(!(town->services & (1u << WILD_SERVICE_RECHARGE)));
			}
		}

		wild_free(w);
	}

	ok;
}

/**
 * A road is wide enough to see, and a turn reads as a turn (WLD-08).
 *
 * Reported from play: a long walk down a road that "ends at the beach".  It did
 * not end -- it turned ninety degrees south in the very block the player was
 * standing in, and ran on across a causeway to a dungeon.  But a road one grid
 * wide puts a corner at a single square of floor at right angles to the way you
 * are going, and there is no reason for anybody to look at it.
 *
 * So a road is three grids wide.  This checks the width, and checks a corner has
 * width in both directions rather than only along the way the player came.
 */
static int test_a_road_is_wide_enough_to_see(void *state) {
	int size = z_info->wild_block_size;
	int bx, by, checked = 0, corners = 0;

	/*
	 * The whole map, not the first dozen blocks: a corner is what the second
	 * half of this test is about, and twelve blocks of a straight run would
	 * pass it while proving nothing.
	 */
	for (by = 1; by < wild->blocks - 1; by++)
		for (bx = 1; bx < wild->blocks - 1; bx++) {
			struct loc offset;
			struct chunk *c;
			int cx, cy, k, across;
			bool turn;

			if (!wild_road_at(wild, bx, by)) continue;
			if (wild_block_at(wild, bx, by)->place) continue;

			/* Only a straight east-west run, or a corner: skip junctions. */
			turn = (wild_road_at(wild, bx - 1, by) &&
					wild_road_at(wild, bx, by + 1) &&
					!wild_road_at(wild, bx + 1, by));

			if (!turn && !(wild_road_at(wild, bx - 1, by) &&
						   wild_road_at(wild, bx + 1, by)))
				continue;

			/* Enough straight runs; keep looking only for corners after that. */
			if (!turn && checked >= 12) continue;

			c = wild_surface(wild, player,
							 loc(bx * size + size / 2, by * size + size / 2),
							 &offset);
			notnull(c);

			cx = bx * size + size / 2 - offset.x;
			cy = by * size + size / 2 - offset.y;

			/* Three grids across, measured through the middle. */
			across = 0;
			for (k = -3; k <= 3; k++) {
				struct loc g = loc(cx, cy + k);

				if (square_in_bounds_fully(c, g) &&
					square(c, g)->feat == FEAT_ROAD)
					across++;
			}

			if (across < 3)
				printf("the road at block %d,%d is only %d grids wide\n",
					bx, by, across);
			require(across >= 3);

			/* At a corner, the leg going away is as wide as the one coming in. */
			if (turn) {
				int down = 0;

				for (k = -1; k <= 1; k++) {
					struct loc g = loc(cx + k, cy + 3);

					if (square_in_bounds_fully(c, g) &&
						square(c, g)->feat == FEAT_ROAD)
						down++;
				}

				if (down < 3)
					printf("the turn at block %d,%d is only %d grids wide\n",
						bx, by, down);
				require(down >= 3);
				corners++;
			}

			cave_free(c);
			checked++;
		}

	require(checked > 3);

	/* And at least one of them was a corner, or the corner half proved nothing. */
	if (!corners) printf("no corner was found to check\n");
	require(corners > 0);

	ok;
}


/**
 * Every service a town holds is standing when the town is built (WLD-16c).
 *
 * This one found a real fault and is kept at a size that would find it again.
 * Services are placed on lots off the streets, before the shops; the ruin pass
 * that follows skips a lot that already has a building on it -- but it asked
 * feat_is_shop(), and a magetower is not a shop.  So the town's data promised a
 * magetower, the generator built one, and a ruin was built on top of it.  About
 * one service in ten went missing that way, and in a village, where the ruins
 * have the run of the place, one in six.
 *
 * Chasing it took four wrong turns -- more lots, an earlier pass, a systematic
 * sweep instead of random guesses, all of which moved the number around without
 * fixing anything -- because the assumption was that placement was failing to
 * find room.  It never failed once: instrumenting the failure to find a lot
 * printed nothing at all.  The building was going up and being demolished.
 *
 * Every band, because the bands differ in how much room the ruins are left with
 * and the village is the worst case, not the largest city.
 */
static int test_every_service_held_is_built(void *state) {
	static const struct {
		const char *name;
		int wid, hgt;
		uint16_t shops, services;
	} bands[] = {
		{ "village",    66,  22, 0x8d, 0x01 },
		{ "town",       88,  22, 0x9f, 0x07 },
		{ "city",       110, 26, 0xff, 0x1f },
		{ "great city", 132, 34, 0xff, 0x1f },
	};
	size_t b;
	int missing = 0;

	for (b = 0; b < N_ELEMENTS(bands); b++) {
		int seed;

		for (seed = 0; seed < 20; seed++) {
			struct chunk *c = town_gen_wild(player, 991 + seed * 37,
											bands[b].wid, bands[b].hgt,
											bands[b].shops, bands[b].services);
			struct loc g;
			uint16_t found = 0;
			int s;

			notnull(c);

			for (g.y = 0; g.y < c->height; g.y++)
				for (g.x = 0; g.x < c->width; g.x++) {
					int at = wild_service_at(c, g);
					if (at >= 0) found |= 1u << at;
				}

			for (s = 0; s < WILD_SERVICE_MAX; s++)
				if ((bands[b].services & (1u << s)) && !(found & (1u << s))) {
					printf("%s seed %d has no %s built\n", bands[b].name,
						   seed, wild_service_name(s));
					missing++;
				}

			cave_free(c);
		}
	}

	eq(missing, 0);

	ok;
}

const char *suite_name = "game/wild";
struct test tests[] = {
	{ "start-is-on-the-surface", test_start_is_on_the_surface },
	{ "world-position-matches-the-window", test_world_position_matches_the_window },
	{ "town-can-be-walked-out-of", test_town_can_be_walked_out_of },
	{ "the-town-gates-are-narrow", test_the_town_gates_are_narrow },
	{ "the-surface-is-bounded", test_the_surface_is_bounded },
	{ "only-the-town-is-known", test_only_the_town_is_known },
	{ "the-town-has-people", test_the_town_has_people },
	{ "a-towns-streets-follow-its-inhabitants", test_a_towns_streets_follow_its_inhabitants },
	{ "the-status-line-names-the-place", test_the_status_line_names_the_place },
	{ "the-surface-knows-the-hour", test_the_surface_knows_the_hour },
	{ "the-town-holds-the-trades-it-was-given", test_the_town_holds_the_trades_it_was_given },
	{ "a-large-town-is-built", test_a_large_town_is_built },
	{ "a-distant-town-is-drawn", test_a_distant_town_is_drawn },
	{ "the-doorstep-is-survivable", test_the_doorstep_is_survivable },
	{ "the-wilderness-is-inhabited", test_the_wilderness_is_inhabited },
	{ "what-you-drop-is-remembered", test_what_you_drop_is_remembered },
	{ "what-you-drop-is-forgotten", test_what_you_drop_is_forgotten },
	{ "a-wounded-unique-is-remembered", test_a_wounded_unique_is_remembered },
	{ "the-townspeople-stay-in-town", test_the_townspeople_stay_in_town },
	{ "an-untouched-unique-is-not-remembered", test_an_untouched_unique_is_not_remembered },
	{ "displacement-keeps-the-world-position", test_displacement_keeps_the_world_position },
	{ "the-map-survives-a-scroll", test_the_map_survives_a_scroll },
	{ "the-map-survives-the-dungeon", test_the_map_survives_the_dungeon },
	{ "scrolling-does-not-move-the-player", test_scrolling_does_not_move_the_player },
	{ "walking-sideways-keeps-the-row", test_walking_sideways_keeps_the_row },
	{ "the-window-reports-its-travel", test_the_window_reports_its_travel },
	{ "the-world-map-remembers-travel", test_the_world_map_remembers_travel },
	{ "reserved-land-is-not-the-town", test_reserved_land_is_not_the_town },
	{ "the-village-is-angbands-town-size", test_the_village_is_angbands_town_size },
	{ "world-position-survives-a-save", test_world_position_survives_a_save },
	{ "the-window-survives-a-save", test_the_window_survives_a_save },
	{ "the-map-survives-a-save-from-below", test_the_map_survives_a_save_from_below },
	{ "a-wall-is-seen-from-inside-a-wood", test_a_wall_is_seen_from_inside_a_wood },
	{ "every-dungeon-opens-somewhere", test_every_dungeon_opens_somewhere },
	{ "a-dungeon-has-a-bottom", test_a_dungeon_has_a_bottom },
	{ "each-dungeon-remembers-its-own-depth", test_each_dungeon_remembers_its_own_depth },
	{ "the-first-dungeon-is-reachable", test_the_first_dungeon_is_reachable },
	{ "a-dungeon-theme-shifts-what-it-yields", test_a_dungeon_theme_shifts_what_it_yields },
	{ "a-theme-does-not-reduce-what-is-found", test_a_theme_does_not_reduce_what_is_found },
	{ "the-theme-mapping-is-total", test_the_theme_mapping_is_total },
	{ "a-dungeon-has-its-own-inhabitants", test_a_dungeon_has_its_own_inhabitants },
	{ "a-habitat-does-not-leak-into-the-country", test_a_habitat_does_not_leak_into_the_country },
	{ "a-dungeon-can-always-be-populated", test_a_dungeon_can_always_be_populated },
	{ "every-dungeon-claims-a-workable-share", test_every_dungeon_claims_a_workable_share },
	{ "every-dungeon-can-be-entered-and-left", test_every_dungeon_can_be_entered_and_left },
	{ "the-road-is-continuous-on-the-ground", test_the_road_is_continuous_on_the_ground },
	{ "a-road-is-wide-enough-to-see", test_a_road_is_wide_enough_to_see },
	{ "roads-reach-every-place", test_roads_reach_every_place },
	{ "towns-have-names", test_towns_have_names },
	{ "town-names-come-back-the-same", test_town_names_come_back_the_same },
	{ "a-magetower-stands-where-it-should", test_a_magetower_stands_where_it_should },
	{ "a-towns-services-are-built", test_a_towns_services_are_built },
	{ "services-follow-the-size", test_services_follow_the_size },
	{ "the-tower-offers-only-known-places", test_the_tower_offers_only_known_places },
	{ "the-fare-rises-with-the-distance", test_the_fare_rises_with_the_distance },
	{ "walking-into-a-town-is-remembered", test_walking_into_a_town_is_remembered },
	{ "the-starting-village-is-always-known", test_the_starting_village_is_always_known },
	{ "no-road-goes-nowhere", test_no_road_goes_nowhere },
	{ "the-road-runs-up-to-a-gate", test_the_road_runs_up_to_a_gate },
	{ "every-service-held-is-built", test_every_service_held_is_built },
	{ NULL, NULL }
};
