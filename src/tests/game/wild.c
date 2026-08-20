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
										   132, 34, 0xffff);
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

const char *suite_name = "game/wild";
struct test tests[] = {
	{ "start-is-on-the-surface", test_start_is_on_the_surface },
	{ "world-position-matches-the-window", test_world_position_matches_the_window },
	{ "town-can-be-walked-out-of", test_town_can_be_walked_out_of },
	{ "the-town-gates-are-narrow", test_the_town_gates_are_narrow },
	{ "the-surface-is-bounded", test_the_surface_is_bounded },
	{ "only-the-town-is-known", test_only_the_town_is_known },
	{ "the-town-has-people", test_the_town_has_people },
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
	{ "world-position-survives-a-save", test_world_position_survives_a_save },
	{ "the-map-survives-a-save-from-below", test_the_map_survives_a_save_from_below },
	{ NULL, NULL }
};
