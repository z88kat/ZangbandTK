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
#include "effects.h"
#include "generate.h"
#include "init.h"
#include "mon-lore.h"
#include "obj-gear.h"
#include "mon-make.h"
#include "obj-desc.h"
#include "obj-make.h"
#include "obj-pile.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "player.h"
#include "player-attack.h"
#include "player-birth.h"
#include "player-timed.h"
#include "player-quest.h"
#include "player-util.h"
#include "mon-util.h"
#include "savefile.h"
#include "store.h"
#include "dun-type.h"
#include "wild.h"
#include "z-util.h"

/**
 * Find somewhere near the player that will hold a monster.
 *
 * Three tests need this and all three used to walk east from the player until
 * they found a square or hit the edge of the map, which fails whenever the
 * character happens to be standing with a wall, a lake or the map edge to its
 * right.  That made them fail perhaps one run in five -- the kind of flake that
 * gets rerun rather than read.  Searching outward in rings finds a square if
 * one exists anywhere nearby, which on an open wilderness map it always does.
 */
static bool find_open_grid_near(struct loc from, struct loc *out) {
	int radius;

	for (radius = 1; radius <= 8; radius++) {
		int dy, dx;

		for (dy = -radius; dy <= radius; dy++)
			for (dx = -radius; dx <= radius; dx++) {
				struct loc try;

				/* Only the ring itself; the inside was covered already. */
				if (ABS(dy) != radius && ABS(dx) != radius) continue;

				try = loc(from.x + dx, from.y + dy);

				if (!square_in_bounds_fully(cave, try)) continue;
				if (!square_isempty(cave, try)) continue;
				if (square_isdamaging(cave, try)) continue;

				*out = try;
				return true;
			}
	}

	return false;
}

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
	require(find_open_grid_near(player->grid, &grid));

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

	require(find_open_grid_near(player->grid, &grid));

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
	 * Then stand at the window's vertical centre and settle there.
	 *
	 * Without this the test was world-dependent and failed about one run in
	 * five, for a correct reason: the window aligns to whole blocks, so where
	 * the character sits inside it depends on the world, and an axis scrolls
	 * when they come within a margin of its edge.  Starting close to the north
	 * edge, the northward drift below legitimately moved the y axis, and the
	 * test read a working scroll as a broken one.  The centre is the one place
	 * a drift of a block and a bit is guaranteed not to reach an edge.
	 */
	here.y = first.y + span / 2;
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

	/*
	 * A grid well outside any town is in no town.
	 *
	 * Searched in all four directions rather than only south, which is what
	 * this used to do: a start village near the southern edge ran off the world
	 * before it found open country and the test failed, perhaps one run in ten.
	 */
	{
		bool found = false;

		for (i = 200; i < 900 && !found; i += 50) {
			int d;
			static const int dy[] = { 1, -1, 0, 0 };
			static const int dx[] = { 0, 0, 1, -1 };

			for (d = 0; d < 4 && !found; d++) {
				struct loc away = loc(world.x + dx[d] * i, world.y + dy[d] * i);

				if (away.y < 0 || away.y >= wild_world_grids()) continue;
				if (away.x < 0 || away.x >= wild_world_grids()) continue;

				if (wild_town_here(wild, away) < 0) found = true;
			}
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

			/*
			 * Two grids across, measured through the middle.  Two rather than
			 * one because a one-grid road cannot be seen to turn; two rather
			 * than the three it was first built at because three reads as a
			 * motorway.  The scan is wider than the road so that finding it
			 * does not depend on which side of the centre line the lanes
			 * happen to fall on.
			 */
			across = 0;
			for (k = -3; k <= 3; k++) {
				struct loc g = loc(cx, cy + k);

				if (square_in_bounds_fully(c, g) &&
					square(c, g)->feat == FEAT_ROAD)
					across++;
			}

			if (across < 2)
				printf("the road at block %d,%d is only %d grids wide\n",
					bx, by, across);
			require(across >= 2);

			/* At a corner, the leg going away is as wide as the one coming in. */
			if (turn) {
				int down = 0;

				for (k = -2; k <= 2; k++) {
					struct loc g = loc(cx + k, cy + 3);

					if (square_in_bounds_fully(c, g) &&
						square(c, g)->feat == FEAT_ROAD)
						down++;
				}

				if (down < 2)
					printf("the turn at block %d,%d is only %d grids wide\n",
						bx, by, down);
				require(down >= 2);
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
 * The quality ladder is a ladder, and it is climbed rather than handed out
 * (WLD-15, WLD-16a).
 *
 * The mechanism only pays off if the tiers are actually rare at the top: an
 * arcane weaponsmith is worth walking to find, and one in every town is
 * wallpaper.  So this measures the shape of the distribution over enough worlds
 * to be a distribution, and fails if it flattens out at either end -- all plain
 * (the ladder does nothing) or top-heavy (the ladder means nothing).
 */
static int test_the_quality_ladder_is_a_ladder(void *state) {
	int counts[8] = { 0 };
	int seed, total = 0, top;

	for (seed = 1; seed <= 40; seed++) {
		struct wilderness *w = wild_new(129, seed * 4409);
		int i;

		wild_generate(w);

		for (i = 0; i < wild_town_count(w); i++) {
			int n;

			for (n = 0; n < (int) z_info->store_max && n < 16; n++) {
				int tier;

				if (!(w->towns[i].stores & (1u << n))) continue;

				tier = wild_store_quality(w, i, n);
				require(tier >= 0 && tier <= quality_tier_count);
				counts[tier]++;
				total++;

				/* Home is plain, by WLD-12 and by having no stock to vary. */
				if (i == 0) eq(tier, 0);
			}
		}

		wild_free(w);
	}

	require(total > 0);
	for (top = 0; top <= quality_tier_count; top++)
		printf("QUALITY tier %d: %4d of %4d (%d%%)\n", top, counts[top], total,
			   counts[top] * 100 / total);

	/* Every rung is reached, or a rung is data nothing selects. */
	for (top = 0; top <= quality_tier_count; top++)
		require(counts[top] > 0);

	/* Plain is the common case... */
	require(counts[0] * 2 > total);

	/* ...and the top of the ladder is somewhere you travel to. */
	require(counts[quality_tier_count] * 10 < total);

	ok;
}


/**
 * Walking into a blessed beast heals you and sends it bounding away (CNT-20).
 *
 * And once only.  A full heal for nothing is worth having; a full heal for
 * nothing that can be had again by following the beast and touching it a second
 * time is a character who never buys a potion again, so the beast remembers.
 */
static int test_a_blessed_beast_bounds_away(void *state) {
	struct monster_race *race = NULL;
	struct monster *mon;
	struct loc grid = player->grid, first;
	struct monster_group_info info = { 0, 0 };
	int i, hurt;

	for (i = 1; i < z_info->r_max; i++)
		if (r_info[i].name && rf_has(r_info[i].flags, RF_BLESSING)) {
			race = &r_info[i];
			break;
		}
	notnull(race);

	/* Somewhere beside the player that will hold it. */
	require(find_open_grid_near(player->grid, &grid));

	require(place_new_monster(cave, grid, race, false, false, info,
							  ORIGIN_DROP));
	mon = square_monster(cave, grid);
	notnull(mon);
	first = mon->grid;

	/*
	 * Work out the character's bonuses before swinging at anything.
	 *
	 * py_attack() divides the turn's energy by state.num_blows, and in a suite
	 * that has never called calc_bonuses() that is zero.  On x86 an integer
	 * division by zero raises SIGFPE and kills the process; on this Mac's ARM it
	 * quietly yields zero, so the test passed locally and killed the whole suite
	 * on Linux and on msys2 with "Suite died".  A real game always has bonuses
	 * calculated by this point -- the gap was in the setup here, not the game.
	 */
	player->upkeep->update |= PU_BONUS;
	update_stuff(player);
	require(player->state.num_blows > 0);

	/* A character in a bad way. */
	player->chp = 1;
	hurt = player->mhp - player->chp;
	require(hurt > 0);

	/*
	 * And a turn to spend.  py_attack() swings only while there is energy left
	 * for another blow, so a character with none never reaches the monster --
	 * which is what happened once the division above was fixed: the test had
	 * been passing because num_blows of zero made the energy per blow zero too,
	 * and zero energy is always enough for a blow that costs nothing.
	 */
	player->energy = z_info->move_energy;

	py_attack(player, grid);

	/* Healed... */
	eq(player->chp, player->mhp);

	/* ...and gone from where it stood, by a good way. */
	notnull(mon->race);
	require(distance(mon->grid, first) >= 5);

	/* It is not dead.  It has simply left. */
	require(mon->hp > 0);

	/* A second touch gives nothing. */
	player->chp = 1;
	first = mon->grid;

	/* Stand next to it again rather than chase it across the level. */
	player->grid = loc(first.x - 1, first.y);
	if (!square_isempty(cave, player->grid)) {
		player->grid = loc(first.x + 1, first.y);
	}
	player->energy = z_info->move_energy;
	py_attack(player, first);

	eq(player->chp, 1);

	/* But it still bounds away from the hand. */
	require(distance(mon->grid, first) >= 5);

	/*
	 * And it always does.  The teleport effect picks the grid whose distance
	 * best approximates what is asked and then varies it by up to a quarter
	 * either way, so "at least five" is not something asking for five would
	 * deliver -- it lands short about half the time.  Asking for ten and
	 * measuring the worst of thirty bounds is what settled the number.
	 */
	{
		int touch, worst = 999, measured = 0;

		for (touch = 0; touch < 30; touch++) {
			struct loc was = mon->grid;
			int d;

			/*
			 * Touched from where the player stands, without walking round to be
			 * beside it: how far the beast bounds does not depend on which side
			 * the hand came from, and needing an empty grid next to it made this
			 * depend on where the last bound happened to land.  Measured that
			 * way it skipped between two and thirty of the touches from one run
			 * to the next, and on a run where it skipped all of them it reported
			 * the sentinel it started from and passed having measured nothing.
			 * The adjacent case is the touch above; this is the distance.
			 */
			player->energy = z_info->move_energy;
			py_attack(player, was);

			d = distance(mon->grid, was);
			if (d < worst) worst = d;
			measured++;
		}

		printf("DEER worst of %d bounds: %d grids\n", measured, worst);

		/* It has to have actually bounded, or this measures nothing. */
		eq(measured, 30);
		require(worst >= 5);
	}

	ok;
}

/**
 * A world seed keeps producing the same world (WLD-03).
 *
 * The world is never written to a savefile; it regenerates from the seed, and a
 * character's knowledge of it is stored by *name* -- which towns they have
 * visited, which dungeons they have found.  So if generation changes, an
 * existing character wakes up in a rearranged world and their knowledge is
 * silently dropped on the floor, because the names no longer match anything.
 *
 * That is not hypothetical.  Adding the magic fractal for WLD-15 put one more
 * draw in the middle of the shared world-seed stream, which shifted every draw
 * after it: rivers, lakes, towns, their names, their sizes, the dungeons and the
 * roads. Every existing character's world was quietly replaced. It surfaced as a
 * magetower that had gone empty -- the savefile recorded a visit to a great city
 * called Helgram, the regenerated world had no such place, and the visit was
 * discarded on load, so the mages would carry the character nowhere.
 *
 * This pins one seed's world so that never happens quietly again.  If a change
 * to generation breaks this test, the test is doing its job: either the change
 * belongs on a stream of its own, the way magic now is, or it really does mean
 * every existing character gets a new world and somebody should say so out loud.
 */
static int test_a_seed_keeps_its_world(void *state) {
	static const struct { const char *name; int x, y, band; } expect[] = {
		{ "Weirmonken", 36,  66, 0 },
		{ "Kashfa",     55,  83, 0 },
		{ "Sawall",     54,  71, 0 },
		{ "Eregnor",   105,  15, 1 },
		{ "Deiga",      51,   5, 1 },
		{ "Ghenesh",    11, 123, 3 },
		{ "Helgram",    24,  65, 3 },
		{ "Avalon",     69,   8, 0 },
		{ "Lorraine",   28, 118, 2 },
		{ "Chantris",   92,  13, 0 },
		{ "Amblerash",  12,  59, 3 },
		{ "Begma",      66, 103, 0 },
	};
	struct wilderness *w = wild_new(129, 44428972);
	int i;

	wild_generate(w);

	eq(wild_town_count(w), (int) N_ELEMENTS(expect));

	for (i = 0; i < wild_town_count(w); i++) {
		notnull(w->towns[i].name);

		if (!streq(w->towns[i].name, expect[i].name) ||
			w->towns[i].block.x != expect[i].x ||
			w->towns[i].block.y != expect[i].y ||
			w->towns[i].band != expect[i].band)
			printf("town %d is %s at %d,%d band %d; was %s at %d,%d band %d\n",
				   i, w->towns[i].name, w->towns[i].block.x,
				   w->towns[i].block.y, w->towns[i].band, expect[i].name,
				   expect[i].x, expect[i].y, expect[i].band);

		require(streq(w->towns[i].name, expect[i].name));
		eq(w->towns[i].block.x, expect[i].x);
		eq(w->towns[i].block.y, expect[i].y);
		eq(w->towns[i].band, expect[i].band);
	}

	/* The dungeons move with everything else, so count them too. */
	eq(w->dungeon_count, 13);

	wild_free(w);

	ok;
}

/**
 * A road out of a gate goes somewhere (WLD-08).
 *
 * Reported from play: "the road goes out of the town but just ends."  It did.
 * The approach paved three grids out of every gate and *then* went looking for
 * the network to join, so every gate the network did not reach kept a three-grid
 * stub pointing into open country -- 147 of 508 gates over six worlds, better
 * than one in four.  A town has four gates and the roads commonly reach one or
 * two of them, so this was never rare.
 *
 * Worse than having no road: a road is a promise that it goes somewhere, and
 * this one was made at four gates in every town and kept at three.
 *
 * Two things are checked, because fixing the first by paving nothing anywhere
 * would satisfy it and leave the towns unreachable.
 */
static int test_a_road_out_of_a_gate_goes_somewhere(void *state) {
	int seed, towns = 0, reached = 0, stubs = 0;

	for (seed = 1; seed <= 4; seed++) {
		struct wilderness *w = wild_new(129, seed * 7717);
		int i;

		wild_generate(w);

		for (i = 0; i < wild_town_count(w); i++) {
			struct loc offset, org = wild_town_origin_of(w, i);
			struct chunk *c;
			struct loc g;
			bool any_road = false;

			c = wild_surface(w, player,
							 loc(org.x + w->towns[i].wid / 2,
								 org.y + w->towns[i].hgt / 2), &offset);
			notnull(c);
			towns++;

			/*
			 * Every road grid outside the town wall must have another road grid
			 * beside it: a lone paved square, or a run of three that touches
			 * nothing further, is the stub this test exists for.
			 */
			/*
			 * Outside the wall, and away from the window's own edge.
			 *
			 * Inside the wall a lone paved grid is the gateway itself, which is
			 * a road tile with the town's own streets either side of it and is
			 * not a stub.  At the window's edge a road's continuation lies
			 * outside the chunk, so it looks orphaned and is not.  Both were
			 * counted by the first version of this test, which is why it
			 * reported between 8 and 28 faults on the same worlds from one run
			 * to the next -- the window aligns to whole blocks and where it
			 * lands depends on where the previous test left it, so the same
			 * road is clipped differently each time.  The margin is wider than
			 * the road for that reason.
			 */
			for (g.y = 6; g.y < c->height - 6; g.y++)
				for (g.x = 6; g.x < c->width - 6; g.x++) {
					int dir, neighbours = 0;
					struct loc rel = loc(g.x - (org.x - offset.x),
										 g.y - (org.y - offset.y));

					if (rel.x >= 0 && rel.x < w->towns[i].wid &&
						rel.y >= 0 && rel.y < w->towns[i].hgt)
						continue;

					if (square(c, g)->feat != FEAT_ROAD) continue;
					any_road = true;

					for (dir = 0; dir < 8; dir++) {
						struct loc n = loc_sum(g, ddgrid_ddd[dir]);

						if (!square_in_bounds_fully(c, n)) continue;
						if (square(c, n)->feat == FEAT_ROAD) neighbours++;
					}

					if (!neighbours) stubs++;
				}

			if (any_road) reached++;
			cave_free(c);
		}

		wild_free(w);
	}

	printf("GATES %d towns, %d have a road at all, %d orphaned road grids\n",
		   towns, reached, stubs);

	/*
	 * Zero, and it took two fixes to earn that.
	 *
	 * The gate stubs this test was written for are gone by construction: the
	 * approach paves nothing until it has found the road, so it cannot leave a
	 * spur that goes nowhere.  Before that, 147 of 508 gates left one.
	 *
	 * That left nought to three stranded grids across forty-eight towns, which
	 * this test bounded rather than asserted while their cause was unknown.  It
	 * is known now: a road is routed *to* a town, so its last stretch runs over
	 * ground the town is then drawn on top of, and the wall and gate can strand
	 * a grid of paving just outside them.  wild_sweep_stranded_road() takes it
	 * up.  See that function for why it is taken up rather than joined up.
	 */
	require(towns > 0);
	eq(stubs, 0);

	/*
	 * And the towns are still on the network, which is the point of it: paving
	 * nothing anywhere would satisfy the line above perfectly.
	 */
	require(reached * 4 > towns * 3);

	ok;
}

/**
 * Quest-giving is carried by a building, and only where there is one (WLD-16d).
 *
 * The point of the requirement is that it is a *property*, not a building type:
 * whichever door has it hands out work, and moving it is a line of code rather
 * than a new building, a new door and a new terrain.  What this defends is that
 * it is attached to a service the town actually has -- a town promising work
 * behind a door it does not possess is a town nobody can take work from.
 */
static int test_work_is_offered_where_there_is_a_door(void *state) {
	int seed, giving = 0, towns = 0;

	for (seed = 1; seed <= 8; seed++) {
		struct wilderness *w = wild_new(129, seed * 4409);
		int i;

		wild_generate(w);

		for (i = 0; i < wild_town_count(w); i++) {
			struct wild_town *tn = &w->towns[i];
			int s;

			towns++;

			for (s = 0; s < WILD_SERVICE_MAX; s++) {
				if (!wild_gives_quests(w, i, s)) continue;

				/* The door it is behind has to be one the town has. */
				require(tn->services & (1u << s));
				giving++;
			}

			/*
			 * A town held by monsters or standing empty keeps no services at
			 * all, so there is no door for work to be behind.  A town the
			 * animals have taken back does keep them -- wild_town_services()
			 * only empties the other two -- so it is not checked here.
			 */
			if (tn->folk == WILD_FOLK_MONSTER ||
				tn->folk == WILD_FOLK_ABANDONED)
				eq(tn->quest_givers, 0);
		}

		wild_free(w);
	}

	printf("QUESTS %d towns, %d with work to offer\n", towns, giving);

	/* Somebody is hiring, or this tests nothing. */
	require(giving > 0);

	/* And not everybody: a village keeps no inn, so work is worth walking to. */
	require(giving < towns);

	ok;
}

/**
 * Every race is playable, and the ported ones kept their character (PLR-01).
 *
 * A race file that will not parse takes the whole game down with it -- when
 * RES_CONFU was written for a resistance that 4.2 keeps as an object flag,
 * nineteen unrelated suites died at once with "Cannot initialize player races",
 * which says nothing about what is wrong or where.
 *
 * This asks the questions the parser cannot: that the ported races are actually
 * there, that their experience factors were kept rather than flattened to 4.2's
 * 120, and that nothing has a hit die or a cost that would make it unplayable.
 */
/**
 * PLR-02: the racial powers came off Zangband's own table, and this pins the
 * numbers so a later edit to p_race.txt cannot quietly reprice them.
 */
static int test_a_race_keeps_its_power(void *state) {
	static const struct {
		const char *race; const char *power;
		int level; int cost; int stat; int fail;
	} table[] = {
		{ "Amberite",   "shift into shadow",       30, 50, STAT_INT, 50 },
		{ "Amberite",   "walk the Pattern",        40, 75, STAT_WIS, 50 },
		{ "Half-Titan", "examine your foes",       35, 20, STAT_STR, 12 },
		{ "Yeek",       "scream",                  15, 15, STAT_WIS, 10 },
		{ "Draconian",  "breathe like a dragon",   15, 25, STAT_CON, 12 },
		{ "Mindflayer", "blast a mind",            15, 12, STAT_INT, 14 },
		{ "Golem",      "turn to stone",           20, 15, STAT_CON,  8 },
		{ "Vampire",    "drink blood",              5, 10, STAT_CON,  9 },
		{ "Sprite",     "throw sleeping dust",     12, 12, STAT_INT, 15 },
	};
	struct player_race *r;
	struct player_power *power;
	size_t i;
	int found = 0, carrying = 0;

	for (r = races; r; r = r->next) {
		if (r->powers) carrying++;

		for (power = r->powers; power; power = power->next) {
			/* Nothing half-parsed: a power with no effect would do nothing. */
			notnull(power->name);
			notnull(power->effects);
			notnull(power->effects->effect);
			require(power->level >= 1 && power->level <= 50);
			require(power->cost > 0);
			require(power->stat >= 0 && power->stat < STAT_MAX);
			require(power->fail >= 0 && power->fail <= 100);

			for (i = 0; i < N_ELEMENTS(table); i++) {
				if (!streq(r->name, table[i].race)) continue;
				if (!streq(power->name, table[i].power)) continue;

				eq(power->level, table[i].level);
				eq(power->cost, table[i].cost);
				eq(power->stat, table[i].stat);
				eq(power->fail, table[i].fail);
				found++;
			}
		}
	}

	/* Every row arrived. */
	eq(found, (int) N_ELEMENTS(table));

	/*
	 * Nine powers across eight races -- the Amberite has two, being the one
	 * bloodline the game is about.  The ninth race we ported, the Beastman,
	 * deliberately has none: in Zangband its whole character was involuntary
	 * mutation rather than anything it could choose to do.
	 */
	eq(carrying, 8);

	ok;
}

/**
 * Trying costs; being told you cannot try does not.  This is why
 * player_use_power() reports those two cases differently, and why ui-map.c asks
 * for a direction before calling it rather than leaving it to the effect.
 */
static int test_a_refused_power_is_free(void *state) {
	struct player_race *r, *mindflayer = NULL;
	const struct player_race *keep_race = player->race;
	struct player_power *power;
	int before;

	for (r = races; r; r = r->next)
		if (streq(r->name, "Mindflayer")) mindflayer = r;
	notnull(mindflayer);
	power = mindflayer->powers;
	notnull(power);

	player->race = mindflayer;
	player->msp = 100;
	player->mhp = 100;

	/* Too junior for it. */
	player->lev = power->level - 1;
	player->csp = 100;
	player->chp = 100;
	before = player->csp;
	require(!player_use_power(player, power, 0));
	eq(player->csp, before);
	eq(player->chp, 100);

	/* Old enough, and paid up.  The price is variable, so bound it. */
	player->lev = power->level;
	require(player_use_power(player, power, 0));
	require(player->csp <= 100 - power->cost / 2);
	require(player->csp >= 100 - power->cost);
	eq(player->chp, 100);

	/* Nothing left in either pool. */
	player->csp = 0;
	player->chp = power->cost - 1;
	before = player->chp;
	require(!player_use_power(player, power, 0));
	eq(player->chp, before);


	player->race = keep_race;
	ok;
}

/**
 * The one that matters: a character with no spells at all can still use what
 * its blood gives it.
 *
 * calc_mana() returns early with a maximum of zero for any class whose book
 * list is empty -- the Warrior and the Monk -- and any character at all can
 * spend their pool down to nothing.  Reading the cost only from spell points
 * would have meant a Draconian Warrior could never once breathe.  Zangband's
 * answer is that the price comes out of the character instead, and this holds
 * that open.
 */
static int test_blood_pays_when_mana_cannot(void *state) {
	struct player_race *r, *draconian = NULL;
	const struct player_race *keep_race = player->race;
	struct player_power *power;

	for (r = races; r; r = r->next)
		if (streq(r->name, "Draconian")) draconian = r;
	notnull(draconian);
	power = draconian->powers;
	notnull(power);

	player->race = draconian;
	player->lev = power->level;

	/* A warrior's pool: there isn't one. */
	player->msp = 0;
	player->csp = 0;
	player->mhp = 100;
	player->chp = 100;

	require(player_use_power(player, power, 0));

	/* It happened, and it was paid for out of the only pool there is. */
	eq(player->csp, 0);
	require(player->chp < 100);
	require(player->chp >= 100 - power->cost);


	player->race = keep_race;
	ok;
}

/**
 * The failure chance has to stay a chance: never certain, never free, and it
 * has to reward the two things the player can actually do about it -- gain
 * levels, and raise the stat the power leans on.
 */
static int test_practice_makes_a_power_surer(void *state) {
	struct player_race *r, *draconian = NULL;
	struct player_power *power;
	int fresh, seasoned, lev;

	for (r = races; r; r = r->next)
		if (streq(r->name, "Draconian")) draconian = r;
	notnull(draconian);
	power = draconian->powers;
	notnull(power);

	player->csp = 100;
	player->msp = 100;

	/* Bounded across the whole range of a career, not just in the middle. */
	for (lev = 1; lev <= 50; lev++) {
		player->lev = lev;
		require(player_power_chance(player, power) >= 5);
		require(player_power_chance(player, power) <= 95);
	}

	player->lev = power->level;
	fresh = player_power_chance(player, power);
	player->lev = 50;
	seasoned = player_power_chance(player, power);

	/* Twenty levels of practice has to show. */
	require(seasoned < fresh);

	/* And so does being short of mana, which is a separate penalty. */
	player->lev = power->level;
	player->csp = 0;
	require(player_power_chance(player, power) >= fresh);

	ok;
}

/**
 * PLR-03/PLR-04: the Monk arrived, and its unarmed ladder came with it.
 */
static int test_the_monk_has_a_ladder(void *state) {
	struct player_class *c, *monk = NULL;
	struct class_blow *blow;
	int rungs = 0, last_level = 0;

	for (c = classes; c; c = c->next)
		if (streq(c->name, "Monk")) monk = c;
	notnull(monk);

	/* The whole point of the class is that it can fight with nothing. */
	require(pf_has(monk->pflags, PF_MARTIAL_ARTS));
	notnull(monk->blows);

	for (blow = monk->blows; blow; blow = blow->next) {
		rungs++;

		notnull(blow->desc);
		require(blow->dd > 0 && blow->ds > 0);
		require(blow->level >= 1 && blow->level <= 50);
		require(blow->chance >= 0 && blow->chance <= 100);

		/*
		 * A ladder, not a bag: each rung arrives later than the last and is
		 * no easier to land.  player_pick_blow() keeps the highest-level
		 * strike it drew, which is only a sensible rule if higher means
		 * better.
		 */
		require(blow->level >= last_level);
		last_level = blow->level;

		if (blow->effect == MA_STUN) require(blow->power > 0);
	}

	/* Zangband's seventeen, from a punch to a crushing blow. */
	eq(rungs, 17);

	/* And the top of the ladder really is the top. */
	blow = monk->blows;
	while (blow->next) blow = blow->next;
	require(blow->dd * blow->ds > monk->blows->dd * monk->blows->ds * 8);

	ok;
}

/**
 * A Monk's bare hands are its weapon, so the blows have to arrive.
 *
 * This is the requirement PLR-04 actually states -- unarmed as a progression
 * rather than as a penalty -- and it is worth a test because 4.2's own answer
 * for an empty weapon slot is a flat 1 damage and no criticals at all.
 */
static int test_bare_hands_are_a_progression(void *state) {
	struct player_class *c, *monk = NULL;
	const struct player_class *keep = player->class;
	int weapon_slot = slot_by_name(player, "weapon");
	struct object *held = slot_object(player, weapon_slot);
	int novice, master, bare_ac, armed;

	for (c = classes; c; c = c->next)
		if (streq(c->name, "Monk")) monk = c;
	notnull(monk);

	player->class = monk;
	player->lev = 50;
	player->upkeep->update |= PU_BONUS;
	update_stuff(player);

	/*
	 * A Monk holding something is just a poor warrior: the ladder is only
	 * reached with the weapon slot empty, which is the trade the class is
	 * built on and worth asserting rather than assuming.
	 */
	armed = player->state.num_blows;

	/* Put the weapon down. */
	player->body.slots[weapon_slot].obj = NULL;

	player->lev = 1;
	player->upkeep->update |= PU_BONUS;
	update_stuff(player);
	novice = player->state.num_blows;

	player->lev = 50;
	player->upkeep->update |= PU_BONUS;
	update_stuff(player);
	master = player->state.num_blows;
	bare_ac = player->state.to_a;

	/* Two at the start, eight at the end -- num_blows is held at 100x. */
	eq(novice, 200);
	eq(master, 800);

	/* And more than the same character got out of the weapon it put down. */
	require(master > armed);

	/*
	 * More than a Warrior gets out of any weapon either, which is the point:
	 * the class gives up the whole object system in exchange for this.
	 */
	require(master > monk->max_attacks * 100);

	/* Nothing worn, so the character is not burdened and is paid for it. */
	require(!player->state.monk_armour);

	/*
	 * The body slot alone is worth 3*lev/2, so a bare Grand Master carries at
	 * least 75 points of armour class it never had to find.
	 */
	require(bare_ac >= 75);

	/* Put everything back for whatever runs next. */
	player->body.slots[weapon_slot].obj = held;
	player->class = keep;
	player->upkeep->update |= PU_BONUS;
	update_stuff(player);

	ok;
}

/**
 * Armour is the Monk's whole cost, so being over the limit has to bite.
 *
 * The trade only means something if it is enforced in both directions, and the
 * threshold is Zangband's: ten pounds plus four tenths of a pound per level,
 * counted over the six armour slots.  Real armour is put on the character here
 * rather than the flag being forced, because the weighing is the part that
 * could be wrong.
 */
static int test_armour_takes_the_balance(void *state) {
	struct player_class *c, *monk = NULL;
	const struct player_class *keep = player->class;
	int weapon_slot = slot_by_name(player, "weapon");
	int body_slot = slot_by_name(player, "body");
	struct object *held = slot_object(player, weapon_slot);
	struct object *worn = slot_object(player, body_slot);
	struct object *plate;
	struct object_kind *kind;
	int light_blows, light_ac, light_toh;

	for (c = classes; c; c = c->next)
		if (streq(c->name, "Monk")) monk = c;
	notnull(monk);

	kind = lookup_kind(TV_HARD_ARMOR,
					   lookup_sval(TV_HARD_ARMOR, "Full Plate Armour"));
	notnull(kind);

	player->class = monk;
	player->lev = 20;
	player->body.slots[weapon_slot].obj = NULL;
	player->body.slots[body_slot].obj = NULL;
	player->upkeep->update |= PU_BONUS;
	update_stuff(player);

	/* Bare: the ladder, the bonuses, and no burden. */
	require(!player->state.monk_armour);
	light_blows = player->state.num_blows;
	light_ac = player->state.to_a;
	light_toh = player->state.to_h;
	require(light_blows > 200);
	require(light_ac >= (player->lev * 3) / 2);
	require(light_toh >= player->lev / 3);

	/* Now put on something a monk has no business wearing. */
	plate = object_new();
	object_prep(plate, kind, 0, RANDOMISE);
	plate->known = object_new();
	plate->number = 1;
	player->body.slots[body_slot].obj = plate;
	player->upkeep->update |= PU_BONUS;
	update_stuff(player);

	/* Full plate is well past ten pounds plus four tenths per level. */
	require(object_weight_one(plate) > 100 + player->lev * 4);
	require(player->state.monk_armour);

	/* Half the blows... */
	eq(player->state.num_blows, light_blows / 2);

	/* ...and the to-hit and bare-slot armour bonuses withdrawn with them. */
	require(player->state.to_h < light_toh);
	require(player->state.to_a < light_ac);

	/* Put everything back for whatever runs next. */
	player->body.slots[body_slot].obj = worn;
	player->body.slots[weapon_slot].obj = held;
	object_delete(NULL, NULL, &plate);
	player->class = keep;
	player->upkeep->update |= PU_BONUS;
	update_stuff(player);

	ok;
}

/**
 * The claim PLR-04 actually makes: unarmed is a progression, not a penalty.
 *
 * 4.2's answer for an empty weapon slot is melee_damage() returning a flat 1
 * with criticals skipped outright, so any character fighting bare does one
 * point a blow.  A Monk has to do very much better than that, or the class is
 * just a Warrior who lost its sword.
 */
static int test_a_monk_hits_harder_than_a_bare_fist(void *state) {
	struct player_class *c, *monk = NULL;
	const struct player_class *keep = player->class;
	struct monster_race *race = NULL;
	struct monster *mon;
	struct loc grid = player->grid;
	struct monster_group_info info = { 0, 0 };
	int weapon_slot = slot_by_name(player, "weapon");
	struct object *held = slot_object(player, weapon_slot);
	int i, r, bare = 0, martial = 0;

	for (c = classes; c; c = c->next)
		if (streq(c->name, "Monk")) monk = c;
	notnull(monk);

	/* Something with enough hit points to be punched a hundred times. */
	for (r = 1; r < z_info->r_max; r++)
		if (r_info[r].name && r_info[r].avg_hp > 500 &&
				!rf_has(r_info[r].flags, RF_UNIQUE) &&
				!rf_has(r_info[r].flags, RF_BLESSING)) {
			race = &r_info[r];
			break;
		}
	notnull(race);

	require(find_open_grid_near(player->grid, &grid));

	require(place_new_monster(cave, grid, race, false, false, info,
							  ORIGIN_DROP));
	mon = square_monster(cave, grid);
	notnull(mon);

	/* Put the weapon down and stop being afraid of anything. */
	player->body.slots[weapon_slot].obj = NULL;
	player->timed[TMD_AFRAID] = 0;
	player->lev = 50;

	/* First as the class the character already was, fighting bare. */
	player->upkeep->update |= PU_BONUS;
	update_stuff(player);
	for (i = 0; i < 100; i++) {
		int before;

		/*
		 * Topped up past anything one turn can get through, so the target
		 * never dies.  It matters: when it did die the count stopped at its
		 * maximum hit points, which silently truncated the martial figure --
		 * the one being measured -- and left the comparison swinging between
		 * five and forty thousand from run to run.
		 */
		mon = square_monster(cave, grid);
		notnull(mon);
		mon->maxhp = 32000;
		mon->hp = mon->maxhp;
		before = mon->hp;

		py_attack(player, grid);

		mon = square_monster(cave, grid);
		notnull(mon);
		bare += before - mon->hp;
	}

	/* Then as a Monk, with the same hands. */
	player->class = monk;
	player->upkeep->update |= PU_BONUS;
	update_stuff(player);
	for (i = 0; i < 100; i++) {
		int before;

		/*
		 * Topped up past anything one turn can get through, so the target
		 * never dies.  It matters: when it did die the count stopped at its
		 * maximum hit points, which silently truncated the martial figure --
		 * the one being measured -- and left the comparison swinging between
		 * five and forty thousand from run to run.
		 */
		mon = square_monster(cave, grid);
		notnull(mon);
		mon->maxhp = 32000;
		mon->hp = mon->maxhp;
		before = mon->hp;

		py_attack(player, grid);

		mon = square_monster(cave, grid);
		notnull(mon);
		martial += before - mon->hp;
	}

	/*
	 * Trained hands against untrained ones.  Measured, this runs at about
	 * fifty to one -- eight strikes a turn of real dice with criticals, against
	 * two blows of a flat point each.  Ten is asserted rather than fifty
	 * because both figures are rolled and the strike drawn varies by design,
	 * but it is far enough above what an unarmed non-Monk can reach that a
	 * regression could not slip under it.
	 */
	printf("MONK %d damage over 100 blows, bare-handed %d\n", martial, bare);
	require(martial > bare * 10);

	player->body.slots[weapon_slot].obj = held;
	player->class = keep;
	player->upkeep->update |= PU_BONUS;
	update_stuff(player);
	if (square_monster(cave, grid)) delete_monster(cave, grid);

	ok;
}

/**
 * PLR-06: the Mindcrafter's psionics are a power list, not a realm.
 *
 * The distinction is the requirement, not a detail: there are no books to find,
 * nothing to study and no realm to choose, so the class has to carry its powers
 * itself and name the stat that feeds them.
 */
static int test_the_mindcrafter_thinks_for_itself(void *state) {
	struct player_class *c, *mind = NULL;
	struct player_power *power;
	int count = 0, last_level = 0;

	for (c = classes; c; c = c->next)
		if (streq(c->name, "Mindcrafter")) mind = c;
	notnull(mind);

	/* No books at all -- that is what makes it not a realm. */
	eq(mind->magic.total_spells, 0);
	eq(mind->magic.num_books, 0);

	/* And so the class names its own casting stat, or it could never spend. */
	eq(mind->power_stat, STAT_WIS);
	require(mind->power_first >= 1);

	notnull(mind->powers);

	for (power = mind->powers; power; power = power->next) {
		struct power_effect *band;
		int bands = 0;

		count++;

		notnull(power->name);
		notnull(power->effects);
		eq(power->stat, STAT_WIS);
		require(power->level >= 1 && power->level <= 50);
		require(power->cost > 0);
		require(power->fail > 0 && power->fail <= 100);

		/* A ladder, like the Monk's: each power arrives after the last. */
		require(power->level >= last_level);
		last_level = power->level;

		for (band = power->effects; band; band = band->next) {
			bands++;
			notnull(band->effect);
			require(band->from >= 0);
			require(band->to == 0 || band->to >= band->from);
		}

		require(bands > 0);
	}

	/* Zangband's twelve, from Neural Blast at 1 to Telekinetic Wave at 28. */
	eq(count, 12);
	eq(mind->powers->level, 1);
	eq(last_level, 28);

	ok;
}

/**
 * A power that grows into something else has to actually grow.
 *
 * Precognition is the one that does this most: it detects monsters at 2, adds
 * traps and doors at 5, invisibility at 15, maps the level at 20, grants
 * telepathy from 25 to 39, detects everything from 30 and lights the whole
 * level at 45.  If the bands were not filtered by level a novice would get all
 * of it at once, which is the bug this is here to catch.
 */
static int test_a_power_grows_with_the_character(void *state) {
	struct player_class *c, *mind = NULL;
	struct player_power *power, *precog = NULL;
	struct power_effect *band;
	int novice = 0, master = 0;

	for (c = classes; c; c = c->next)
		if (streq(c->name, "Mindcrafter")) mind = c;
	notnull(mind);

	for (power = mind->powers; power; power = power->next)
		if (streq(power->name, "see what is coming")) precog = power;
	notnull(precog);

	/* It has to be banded at all, or there is nothing to test. */
	require(precog->effects->next != NULL);

	for (band = precog->effects; band; band = band->next) {
		if (band->from <= 2 && (!band->to || band->to >= 2)) novice++;
		if (band->from <= 50 && (!band->to || band->to >= 50)) master++;
	}

	/* Both get something... */
	require(novice > 0);
	require(master > 0);

	/* ...but the trainee gets markedly less of it. */
	require(master > novice);

	ok;
}

/**
 * A class with no books still has to have something to spend.
 *
 * This is the change calc_mana() needed for PLR-06, and the reason it needed
 * it: mana is derived from the realms a class's spellbooks belong to, and a
 * Mindcrafter has no books, so the general case returns a maximum of zero and
 * the class could never use the one thing it has.  A Warrior returning zero is
 * still correct -- it has nothing to spend it on -- so both are checked here.
 */
static int test_psionics_are_paid_for(void *state) {
	struct player_class *c, *mind = NULL, *warrior = NULL;
	const struct player_class *keep = player->class;
	int mindful, martial;

	for (c = classes; c; c = c->next) {
		if (streq(c->name, "Mindcrafter")) mind = c;
		if (streq(c->name, "Warrior")) warrior = c;
	}
	notnull(mind);
	notnull(warrior);

	player->lev = 20;

	player->class = mind;
	player->upkeep->update |= PU_BONUS;
	update_stuff(player);
	mindful = player->msp;

	player->class = warrior;
	player->upkeep->update |= PU_BONUS;
	update_stuff(player);
	martial = player->msp;

	/* The Mindcrafter can spend; the Warrior has nothing to spend it on. */
	require(mindful > 0);
	eq(martial, 0);

	/*
	 * And enough of it to matter: the most expensive power on the list costs
	 * 20, and a class that could never afford its own top power would be a
	 * worse bug than having no mana at all.
	 */
	player->class = mind;
	player->lev = 50;
	player->upkeep->update |= PU_BONUS;
	update_stuff(player);
	require(player->msp >= 20);

	/*
	 * Wearing its own starting armour, which is the case that was actually
	 * broken and which this test used to miss entirely by leaving the
	 * character undressed.  calc_mana() reads the armour allowance out of the
	 * class's magic block, a power-list class has no magic block, and eight
	 * pounds of soft leather then cancelled more mana than the class ever had
	 * -- so a Mindcrafter began the game unable to pay for anything and stayed
	 * that way for fifty levels.
	 */
	{
		int body_slot = slot_by_name(player, "body");
		struct object *worn = slot_object(player, body_slot);
		struct object *leather = object_new();
		struct object_kind *kind = lookup_kind(TV_SOFT_ARMOR,
			lookup_sval(TV_SOFT_ARMOR, "Soft Leather Armour"));
		int dressed;

		notnull(kind);
		object_prep(leather, kind, 0, RANDOMISE);
		leather->known = object_new();
		leather->number = 1;

		player->body.slots[body_slot].obj = leather;

		/*
		 * At level 1, which is where this actually bit.  The armour costs a
		 * flat eight points of mana, which a fiftieth-level Mindcrafter never
		 * notices and a first-level one cannot survive: it has about two, so
		 * the class started the game with nothing to spend and paid for its
		 * own first power in blood.  Checking a high level only -- which is
		 * what this test did before -- sails straight past that.
		 */
		player->lev = 1;
		player->upkeep->update |= PU_BONUS;
		update_stuff(player);
		dressed = player->msp;

		/* Enough for Neural Blast, which costs one and arrives at level 1. */
		if (dressed < 1)
			printf("a dressed level 1 Mindcrafter has %d mana\n", dressed);
		require(dressed >= 1);

		/* And at fifty it can still afford its dearest power. */
		player->lev = 50;
		player->upkeep->update |= PU_BONUS;
		update_stuff(player);
		require(player->msp >= 20);

		player->body.slots[body_slot].obj = worn;
		object_delete(NULL, NULL, &leather);
	}

	player->class = keep;
	player->upkeep->update |= PU_BONUS;
	update_stuff(player);

	ok;
}

/**
 * PLR-05/DEC-38: nine Lords of the Courts, each with a full ladder.
 */
static int test_the_lords_of_chaos_are_all_there(void *state) {
	static const char *lords[] = {
		"Swayvill", "Suhuy", "Mandor", "Dara", "Gramble",
		"Jurt", "Despil", "Borel", "Gilva"
	};
	struct patron *patron;
	size_t i;
	int count = 0, found = 0;

	notnull(patrons);

	for (patron = patrons; patron; patron = patron->next) {
		int slot;

		count++;

		notnull(patron->name);
		notnull(patron->title);
		notnull(patron->text);

		/*
		 * Every rung resolved.  The ladder is written as codes and matched to
		 * rewards after the file is read, so an unresolved entry would be a
		 * null dereference the first time that Lord felt anything.
		 */
		for (slot = 0; slot < PATRON_LADDER; slot++) {
			notnull(patron->ladder[slot]);
			notnull(patron->ladder[slot]->message);
		}

		for (i = 0; i < N_ELEMENTS(lords); i++)
			if (streq(patron->name, lords[i])) found++;
	}

	/* Zelazny's nine, and nobody from Moorcock or Warhammer. */
	eq(count, (int) N_ELEMENTS(lords));
	eq(found, (int) N_ELEMENTS(lords));

	ok;
}

/**
 * The ladder has to be a ladder, or the roll that indexes it means nothing.
 *
 * gain-level rewards pick a slot and normally skip the bottom quarter, which
 * only makes cruelty rare if the cruelty is in fact at the bottom.  A Lord whose
 * table was written in the wrong order would quietly be handing out full heals
 * as punishments.
 */
static int test_a_patrons_ladder_runs_worst_to_best(void *state) {
	struct patron *patron;

	for (patron = patrons; patron; patron = patron->next) {
		int slot, harm_low = 0, harm_high = 0;

		for (slot = 0; slot < PATRON_LADDER; slot++) {
			const char *code = patron->ladder[slot]->code;
			bool harmful = streq(code, "WRATH") || streq(code, "HURT") ||
				streq(code, "PISS_OFF") || streq(code, "CURSE_WP") ||
				streq(code, "CURSE_AR") || streq(code, "LOSE_ABL") ||
				streq(code, "RUIN_ABL") || streq(code, "LOSE_EXP") ||
				streq(code, "SUMMON_M") || streq(code, "H_SUMMON") ||
				streq(code, "DESTRUCT");

			if (!harmful) continue;

			if (slot < PATRON_LADDER / 2) harm_low++;
			else harm_high++;
		}

		/* Everything unpleasant sits in the bottom half of every ladder. */
		if (harm_high)
			printf("%s has %d harmful rewards in its top half\n",
				   patron->name, harm_high);
		eq(harm_high, 0);

		/* And every Lord can be displeased, or it is not a patron at all. */
		require(harm_low > 0);
	}

	ok;
}

/**
 * Only a Chaos-Warrior is owned, and it is owned from birth.
 */
static int test_only_a_chaos_warrior_is_owned(void *state) {
	struct player_class *c, *chaos = NULL, *warrior = NULL;
	const struct player_class *keep = player->class;
	const struct patron *keep_patron = player->patron;
	int i, distinct = 0;
	const struct patron *seen[16];

	for (c = classes; c; c = c->next) {
		if (streq(c->name, "Chaos-Warrior")) chaos = c;
		if (streq(c->name, "Warrior")) warrior = c;
	}
	notnull(chaos);
	notnull(warrior);

	require(pf_has(chaos->pflags, PF_CHAOS_PATRON));
	require(!pf_has(warrior->pflags, PF_CHAOS_PATRON));

	/* A Warrior answers to nobody. */
	player->class = warrior;
	patron_choose(player);
	null(player->patron);

	/*
	 * A Chaos-Warrior always has one, and which one varies -- the point of
	 * nine Lords is that two characters are not the same character.
	 */
	memset(seen, 0, sizeof(seen));
	player->class = chaos;

	for (i = 0; i < 200; i++) {
		int j;
		bool known = false;

		patron_choose(player);
		notnull(player->patron);

		for (j = 0; j < distinct; j++)
			if (seen[j] == player->patron) known = true;

		if (!known && distinct < (int) N_ELEMENTS(seen))
			seen[distinct++] = player->patron;
	}

	/* Over two hundred births, all nine should have come up. */
	eq(distinct, 9);

	player->class = keep;
	player->patron = keep_patron;

	ok;
}

/**
 * Thirteen is an unlucky level, and the roll is where that lives.
 *
 * Zangband weighted the reward roll so cruelty is uncommon -- one chance in six
 * of reaching the bottom quarter of the ladder, where everything genuinely
 * unpleasant is -- except that the odds swing with the level reached: one in two
 * at thirteen, one in three at every other thirteenth, and one in twelve at
 * every fourteenth. It is a superstition a player can learn, it is invisible
 * from inside the game, and it would survive being silently lost.
 *
 * The roll is measured directly rather than through its consequences.  Watching
 * hit points instead was the first attempt and it was a bad test: most of the
 * cruel outcomes do not cost hit points at all, several of the kind ones
 * recalculate the maximum, and the two levels came out fifteen apart in four
 * hundred -- a difference far too small to tell from noise.
 */
static int test_thirteen_is_an_unlucky_level(void *state) {
	const int trials = 4000;
	const int floor_slot = PATRON_LADDER / 4;
	int lev, low[51];

	memset(low, 0, sizeof(low));

	for (lev = 1; lev <= 50; lev++) {
		int i;

		player->lev = lev;

		for (i = 0; i < trials; i++)
			if (patron_roll_slot(player) < floor_slot) low[lev]++;
	}

	printf("PATRON bottom-of-ladder rolls per %d: lev13 %d, lev20 %d, "
		   "lev26 %d, lev28 %d\n",
		   trials, low[13], low[20], low[26], low[28]);

	/*
	 * Thirteen is the worst year of a Chaos-Warrior's life.  Expected rates
	 * are 1/2, 1/6, 1/3 and 1/12 of the nasty roll, and the margins between
	 * them are wide enough that these hold comfortably over four thousand.
	 */
	require(low[13] > low[26]);		/* 1/2 beats 1/3   */
	require(low[26] > low[20]);		/* 1/3 beats 1/6   */
	require(low[20] > low[28]);		/* 1/6 beats 1/12  */

	/* And no level is ever entirely safe, or the ladder's floor is dead. */
	for (lev = 1; lev <= 50; lev++)
		require(low[lev] > 0);

	ok;
}

/**
 * The mapping from Zangband's class table was measured, not guessed, and this
 * holds the Monk on the numbers that measurement produced.
 */
static int test_the_monk_keeps_zangbands_numbers(void *state) {
	struct player_class *c, *monk = NULL;

	for (c = classes; c; c = c->next)
		if (streq(c->name, "Monk")) monk = c;
	notnull(monk);

	/* Stats, straight across; Zangband's charisma has nowhere to go. */
	eq(monk->c_adj[STAT_STR], 2);
	eq(monk->c_adj[STAT_INT], -1);
	eq(monk->c_adj[STAT_WIS], 1);
	eq(monk->c_adj[STAT_DEX], 3);
	eq(monk->c_adj[STAT_CON], 2);

	/* Hit dice, copied; disarm and device bases, copied. */
	eq(monk->c_mhp, 6);
	eq(monk->c_skills[SKILL_DISARM_PHYS], 45);
	eq(monk->c_skills[SKILL_DEVICE], 32);
	eq(monk->c_skills[SKILL_SAVE], 28);

	/*
	 * The experience factor is the one that carries weight.  4.2 leaves it at
	 * zero for all nine of its classes; Zangband ran 0 to 40 and used it as
	 * the balance dial.  Keeping Zangband's is the same call PLR-01 made for
	 * races, and it is what makes a Monk slow to level.
	 */
	eq(monk->c_exp, 40);

	{
		/*
		 * The rule rather than the count, so this does not need editing every
		 * time a class lands: all nine of Angband's own classes are free, and
		 * every class brought over from Zangband costs, which is the whole
		 * point of keeping a field 4.2 leaves at zero.
		 */
		static const char *angbands[] = {
			"Warrior", "Mage", "Druid", "Priest", "Necromancer",
			"Paladin", "Rogue", "Ranger", "Blackguard"
		};
		struct player_class *other;
		int ported = 0;

		for (other = classes; other; other = other->next) {
			size_t k;
			bool inherited = false;

			for (k = 0; k < N_ELEMENTS(angbands); k++)
				if (streq(other->name, angbands[k])) inherited = true;

			if (inherited) {
				if (other->c_exp != 0)
					printf("%s should be free, costs %d\n", other->name,
						   other->c_exp);
				eq(other->c_exp, 0);
			} else {
				if (other->c_exp <= 0)
					printf("%s is ported and should cost\n", other->name);
				require(other->c_exp > 0);
				ported++;
			}
		}

		/* And there is at least one of ours, or the rule proves nothing. */
		require(ported > 0);
	}

	ok;
}

static int test_every_race_is_playable(void *state) {
	static const struct { const char *name; int exp; } ported[] = {
		{ "Amberite",   225 },
		{ "Beastman",   140 },
		{ "Yeek",       100 },
		{ "Draconian",  250 },
		{ "Mindflayer", 140 },
		{ "Vampire",    200 },
		{ "Golem",      200 },
		{ "Sprite",     175 },
		{ "Half-Titan", 255 },
	};
	struct player_race *r;
	size_t i;
	int found = 0, total = 0;

	for (r = races; r; r = r->next) {
		total++;

		notnull(r->name);

		/* Nothing unplayable: a race with no hit die cannot be rolled up. */
		require(r->r_mhp > 0);
		require(r->r_exp > 0);

		for (i = 0; i < N_ELEMENTS(ported); i++)
			if (streq(r->name, ported[i].name)) {
				/*
				 * The experience factor is the race's design, not drift --
				 * 4.2 flattened nearly everything to 120 and Zangband used
				 * this as its balance dial, which is what makes a Half-Titan
				 * expensive to be.
				 */
				if (r->r_exp != ported[i].exp)
					printf("%s costs %d, expected %d\n", r->name, r->r_exp,
						   ported[i].exp);
				eq(r->r_exp, ported[i].exp);
				found++;
			}
	}

	/* All nine arrived... */
	eq(found, (int) N_ELEMENTS(ported));

	/* ...on top of the eleven 4.2 ships. */
	eq(total, 11 + (int) N_ELEMENTS(ported));

	ok;
}

/**
 * The game can actually be won (WLD-20, WLD-21, DEC-30).
 *
 * The fixed quests are the ones the game ends on, and every way of getting them
 * wrong is silent.  A quest whose dungeon does not reach its depth cannot be
 * completed and nothing says so; a quest naming a monster the bestiary does not
 * hold cannot be completed either; and a quest with no dungeon at all is
 * completable in whichever of the deep dungeons the player happens to be in,
 * which is not the same game.
 *
 * These are cheap to check and were expensive to notice: the endgame is the one
 * part of the game nobody plays through by accident.
 */
static int test_the_game_can_be_won(void *state) {
	int i, fixed = 0;

	require(z_info->quest_fixed > 0);

	/*
	 * The fixed ones only.  The list has room past them for work taken from a
	 * building (WLD-16d), and those slots are empty until somebody fills one --
	 * they are not quests the game ends on and they have no name yet.
	 */
	for (i = 0; i < (int) z_info->quest_fixed; i++) {
		const struct quest *q = &player->quests[i];
		const struct dun_type *type;

		notnull(q->name);

		/* It names a monster that exists. */
		notnull(q->race);

		/* It names a dungeon. */
		require(q->dungeon > 0);
		type = dun_type_by_index(q->dungeon - 1);
		notnull(type);

		/* And that dungeon goes deep enough to hold it, and starts above it. */
		if (q->level > type->max_depth || q->level < type->min_depth)
			printf("QUEST %s wants depth %d, but %s runs %d to %d\n",
				   q->name, q->level, type->name, type->min_depth,
				   type->max_depth);
		require(q->level <= type->max_depth);
		require(q->level >= type->min_depth);

		/* The character is on it from the start, and it ends the game. */
		require(q->fixed);
		eq(q->state, QUEST_TAKEN);

		fixed++;
	}

	require(fixed > 0);

	ok;
}

/**
 * The Unicorn gives the greater blessing (CNT-20, DEC-30).
 *
 * There are deer, and there is the Unicorn, and the difference is not a matter
 * of degree -- so the strength of a blessing comes from the beast being unique
 * rather than from a second flag.  She undoes everything the healer in a town
 * sells: the wounds, the ailments, the drained stats and the levels lost to
 * life-draining.  Once.
 */
static int test_the_unicorn_makes_you_whole(void *state) {
	struct monster_race *race = NULL;
	struct monster *mon;
	struct loc grid = player->grid;
	struct monster_group_info info = { 0, 0 };
	int i;

	for (i = 1; i < z_info->r_max; i++)
		if (r_info[i].name && rf_has(r_info[i].flags, RF_BLESSING) &&
			rf_has(r_info[i].flags, RF_UNIQUE)) {
			race = &r_info[i];
			break;
		}
	notnull(race);

	require(find_open_grid_near(player->grid, &grid));

	require(place_new_monster(cave, grid, race, false, false, info,
							  ORIGIN_DROP));
	mon = square_monster(cave, grid);
	notnull(mon);

	player->upkeep->update |= PU_BONUS;
	update_stuff(player);
	require(player->state.num_blows > 0);

	/* A character in every kind of trouble at once. */
	player->chp = 1;
	player_inc_timed(player, TMD_POISONED, 40, false, false, false);
	player_inc_timed(player, TMD_BLIND, 40, false, false, false);
	player_inc_timed(player, TMD_CONFUSED, 40, false, false, false);
	player_inc_timed(player, TMD_AFRAID, 40, false, false, false);
	player->stat_cur[STAT_STR] = player->stat_max[STAT_STR] - 3;
	require(player->timed[TMD_POISONED] > 0);
	require(player->stat_cur[STAT_STR] < player->stat_max[STAT_STR]);

	player->energy = z_info->move_energy;
	py_attack(player, grid);

	/* Whole again, in every sense the healer charges for. */
	eq(player->chp, player->mhp);
	eq(player->timed[TMD_POISONED], 0);
	eq(player->timed[TMD_BLIND], 0);
	eq(player->timed[TMD_CONFUSED], 0);
	eq(player->timed[TMD_AFRAID], 0);
	eq(player->stat_cur[STAT_STR], player->stat_max[STAT_STR]);

	/* And she is not killed by it. */
	require(mon->hp > 0);

	/* Once.  A second touch restores nothing. */
	player->chp = 1;
	player->grid = loc(mon->grid.x - 1, mon->grid.y);
	if (square_isempty(cave, player->grid)) {
		player->energy = z_info->move_energy;
		py_attack(player, mon->grid);
		eq(player->chp, 1);
	}

	ok;
}

/**
 * The inn's dreams follow the law of the place you sleep in (PLR-41).
 *
 * The point of keying them on the town rather than rolling flat is that every inn
 * in the world would otherwise be the same inn, when there is a whole parameter
 * space here saying how settled a place is.  A lawful city gives visions; a town
 * on the edge of what is governed gives you a bad night.
 */
static int test_the_inn_dreams_by_the_law(void *state) {
	int law, last_bright = -1, last_dark = 999;

	/*
	 * 155 upwards, because that is where an inn can stand: below it the town has
	 * fallen (wild_town_folk) and keeps no services at all, so the lawless end of
	 * the range never comes up and need not be defended.
	 */
	for (law = 155; law <= 255; law += 5) {
		int bright, dark;

		player_dream_chances(law, &bright, &dark);

		/* Visions rise with order... */
		require(bright >= last_bright);

		/* ...and bad nights fall away with it. */
		require(dark <= last_dark);

		/* Neither is ever a certainty, and they cannot both happen. */
		require(bright + dark < 100);

		last_bright = bright;
		last_dark = dark;
	}

	/* And the two ends actually differ, or the law is decorative. */
	{
		int frontier_bright, frontier_dark, city_bright, city_dark;

		player_dream_chances(155, &frontier_bright, &frontier_dark);
		player_dream_chances(254, &city_bright, &city_dark);

		printf("DREAM frontier %d%% true / %d%% dark, city %d%% / %d%%\n",
			   frontier_bright, frontier_dark, city_bright, city_dark);

		require(city_bright > frontier_bright * 2);
		require(frontier_dark > city_dark * 2);

		/* A bed is worth buying even on the frontier. */
		require(frontier_bright > 0);
	}

	ok;
}

/**
 * A true dream shows you the nearest place you have not found, and does not
 * carry you there (PLR-41).
 *
 * The second half is the one worth defending.  A place is *seen* -- you know it
 * is there and it is on the world map -- but not *visited*, and the magetower
 * only travels to places you have stood in.  Marking it visited would turn a
 * night's sleep into free passage to anywhere in the world, which is a different
 * and much worse feature.
 */
static int test_a_true_dream_shows_the_nearest_place(void *state) {
	struct wilderness *w = wild_new(129, 4409 * 7);
	int size = z_info->wild_block_size;
	struct loc from;
	const char *name;
	bool down = false;
	int i, seen = 0, nearest = -1, revealed = 0, guard = 0;

	wild_generate(w);

	/* A character who has found nothing at all. */
	for (i = 0; i < w->blocks * w->blocks; i++)
		w->map[i].info &= ~WILD_INFO_SEEN;
	for (i = 0; i < w->town_count; i++)
		w->towns[i].visited = 0;

	from = loc(w->towns[0].block.x * size + size / 2,
			   w->towns[0].block.y * size + size / 2);

	/* What the nearest unfound place actually is. */
	for (i = 0; i < w->town_count; i++) {
		int d = distance(w->towns[0].block, w->towns[i].block);
		if (i && (nearest < 0 || d < nearest)) nearest = d;
	}
	for (i = 0; i < w->dungeon_count; i++) {
		int d = distance(w->towns[0].block, w->dungeons[i].block);
		if (nearest < 0 || d < nearest) nearest = d;
	}
	require(nearest >= 0);

	name = wild_reveal_nearest(w, from, &down);
	notnull(name);

	/* Exactly one block was put on the map... */
	for (i = 0; i < w->blocks * w->blocks; i++)
		if (w->map[i].info & WILD_INFO_SEEN) seen++;
	eq(seen, 1);

	/* ...and it is the nearest one there was. */
	for (i = 0; i < w->blocks * w->blocks; i++)
		if (w->map[i].info & WILD_INFO_SEEN) {
			struct loc block = loc(i % w->blocks, i / w->blocks);
			eq(distance(w->towns[0].block, block), nearest);
		}

	/* Knowing where it is is not having been there. */
	for (i = 0; i < w->town_count; i++)
		eq(w->towns[i].visited, 0);

	/* Keep dreaming and the map fills; then there is nothing left to show. */
	while (guard++ < 200) {
		if (!wild_reveal_nearest(w, from, &down)) break;
		revealed++;
	}
	require(guard < 200);
	require(revealed > 0);
	require(wild_reveal_nearest(w, from, &down) == NULL);

	wild_free(w);

	ok;
}

/**
 * The lotus is a mushroom you cannot tell from any other until you eat one
 * (PLR-40).
 *
 * The trap is the point: it has to be indistinguishable on the floor and named
 * once it has been eaten, or it is either unfair or harmless.  Mushrooms are
 * flavoured in 4.2, which gives both halves for free -- but the naming of
 * flavoured items goes through enough indirection to be worth checking rather
 * than assuming.
 */
static int test_the_lotus_is_an_unknown_mushroom(void *state) {
	struct object_kind *kind = lookup_kind(TV_MUSHROOM,
										   lookup_sval(TV_MUSHROOM, "Lotus"));
	char unknown[80], known[80];
	bool was_aware;

	notnull(kind);
	notnull(kind->flavor);
	was_aware = kind->aware;

	kind->aware = false;
	object_kind_name(unknown, sizeof(unknown), kind, false);

	kind->aware = true;
	object_kind_name(known, sizeof(known), kind, false);

	printf("LOTUS unknown \"%s\"  known \"%s\"\n", unknown, known);

	/* Unknown, it says nothing about what it is... */
	require(!strstr(unknown, "Lotus"));

	/* ...and known, it does. */
	require(strstr(known, "Lotus") != NULL);

	/* It sets the fuse rather than acting at once. */
	notnull(kind->effect);
	eq(kind->effect->index, EF_TIMED_SET);
	eq(kind->effect->subtype, TMD_LOTUS);

	kind->aware = was_aware;

	ok;
}

/**
 * The lotus forgets everything, and leaves home (PLR-40).
 *
 * Five kinds of knowledge in five different places, so this checks all five
 * rather than trusting that one function touched them all -- the failure mode of
 * a feature like this is quietly forgetting to forget something, and nothing in
 * play would tell you which of the five it was.
 */
static int test_the_lotus_forgets_the_world(void *state) {
	struct monster_race *race = NULL;
	struct object_kind *flavoured = NULL;
	int i, seen = 0, aware = 0, known = 0;

	/* Give the character something to lose. */
	for (i = 0; i < wild->blocks * wild->blocks; i++)
		wild->map[i].info |= WILD_INFO_SEEN;
	for (i = 0; i < wild_town_count(wild); i++)
		wild->towns[i].visited = 1;

	for (i = 0; i < z_info->r_max; i++)
		if (r_info[i].name) { race = &r_info[i]; break; }
	notnull(race);
	get_lore(race)->sights = 7;
	get_lore(race)->tkills = 3;

	for (i = 0; i < z_info->k_max; i++)
		if (k_info[i].flavor) { flavoured = &k_info[i]; break; }
	notnull(flavoured);
	flavoured->aware = true;

	if (player->spell_flags && player->class->magic.total_spells) {
		player->spell_flags[0] |= PY_SPELL_LEARNED;
		player->spell_order[0] = 0;
	}

	player_forget_the_world(player);

	/* The world map is blank... */
	for (i = 0; i < wild->blocks * wild->blocks; i++)
		if (wild->map[i].info & WILD_INFO_SEEN) seen++;

	/* ...except the nine blocks around home, which WLD-12 keeps known. */
	eq(seen, 9);
	eq(wild->towns[0].visited, 1);
	for (i = 1; i < wild_town_count(wild); i++)
		eq(wild->towns[i].visited, 0);

	/* The monsters are strangers again. */
	eq(get_lore(race)->sights, 0);
	eq(get_lore(race)->tkills, 0);

	/* And so is everything in a bottle. */
	for (i = 0; i < z_info->k_max; i++)
		if (k_info[i].flavor && k_info[i].aware) aware++;
	eq(aware, 0);

	/* The spells are gone from the book of the head. */
	if (player->spell_flags && player->class->magic.total_spells) {
		for (i = 0; i < player->class->magic.total_spells; i++)
			if (player->spell_flags[i] & PY_SPELL_LEARNED) known++;
		eq(known, 0);
		eq(player->spell_order[0], 99);
	}

	/* Put the world back for the tests that follow. */
	for (i = 0; i < wild_town_count(wild); i++)
		if (i == 0) wild->towns[i].visited = 1;

	ok;
}

/**
 * A character who has forgotten everything can still get home (PLR-40, WLD-12).
 *
 * The reason the exception exists.  The magetower's list is built from the places
 * the player has found, so forgetting all of them leaves a character with no fast
 * travel, no destination and a blank map -- which is not a setback, it is a lost
 * save.  The first Amber novel opens on a man with no memory who knows only that
 * there is a place called Amber; that is the amount of knowledge this leaves.
 */
static int test_the_lotus_leaves_a_way_home(void *state) {
	int i, dests;

	for (i = 0; i < wild->blocks * wild->blocks; i++)
		wild->map[i].info |= WILD_INFO_SEEN;
	for (i = 0; i < wild_town_count(wild); i++)
		wild->towns[i].visited = 1;

	wild_forget_knowledge(wild);

	/* Exactly one place left to go, and it is home. */
	dests = 0;
	for (i = 0; i < wild_town_count(wild); i++)
		if (wild->towns[i].visited) dests++;
	eq(dests, 1);
	eq(wild->towns[0].visited, 1);

	/* And its ground is on the map, so it is somewhere rather than a name. */
	require(wild_seen(wild, wild->towns[0].block.x, wild->towns[0].block.y));

	for (i = 0; i < wild_town_count(wild); i++)
		if (i == 0) wild->towns[i].visited = 1;

	ok;
}

/**
 * There is one home in the world, and every town's home door opens onto it
 * (WLD-11a, WLD-16a).
 *
 * Asked from play: leave something in your house, walk to another town, open the
 * house there -- is it the same house?  It is, and the answer matters enough to
 * pin down, because the machinery around it says otherwise.  Every town holds a
 * home, and a shop's shelves are restocked when the player carries their custom
 * to a different town.  Home is exempt from that, deliberately: a per-town home
 * would strand a character's spare gear in whichever village they happened to be
 * standing in when they outgrew it, four days' walk away and unreachable except
 * on foot, and nothing in the game would tell them which village it was.
 *
 * So the exemption is load-bearing, and this is what would catch its removal.
 */
static int test_there_is_one_home_in_the_world(void *state) {
	struct store *home = NULL;
	struct object *obj;
	int n;

	for (n = 0; n < (int) z_info->store_max; n++)
		if (stores[n].feat == FEAT_HOME) home = &stores[n];
	notnull(home);

	/* Every town has one to walk into, whatever else it keeps. */
	for (n = 0; n < wild_town_count(wild); n++)
		require(wild->towns[n].stores & (1u << WILD_STORE_HOME));

	/* Leave something on the shelf. */
	obj = object_new();
	object_prep(obj, lookup_kind(TV_FOOD, 1), 1, RANDOMISE);
	obj->known = object_new();
	obj->number = 1;
	pile_insert(&home->stock, obj);
	home->stock_num = 1;

	/*
	 * Now arrive from somewhere else entirely.  stock_town is what the restock
	 * keys on, so setting it to a town this is not is the strongest form of the
	 * question: an ordinary shop would empty its shelves here.
	 */
	home->stock_town = wild_town_count(wild) - 1;
	store_enter(home);

	/* Still there. */
	eq(home->stock_num, 1);
	notnull(home->stock);

	/* Clean up after ourselves; the suite shares these stores. */
	object_pile_free(NULL, NULL, home->stock);
	home->stock = NULL;
	home->stock_num = 0;
	home->stock_town = 0;

	ok;
}

/**
 * A better shop deals in better goods (WLD-16a).
 *
 * The tier has to be worth travelling for, and the only way to tell is to look
 * at what ends up on the shelves.  The tier is added to the level goods are
 * generated at, which object_prep() and apply_magic() both work from, so a
 * higher tier should buy deeper kinds and better magic on them rather than just
 * a longer word on the sign.
 */
static int test_a_better_shop_deals_in_better_goods(void *state) {
	struct store *s = NULL;
	int i, n, plain = 0, plain_n = 0, arcane = 0, arcane_n = 0;
	int plain_plus = 0, arcane_plus = 0;

	/*
	 * The shop with the widest range of goods, whichever that is -- picking one
	 * by name would test store.txt as much as the ladder, and the first shop
	 * with any turnover at all is the general store, whose entire stock is food
	 * and torches.  There is no deep end of that table to bias towards, which
	 * is exactly the measurement that caught this the first time round.
	 */
	for (n = 0; n < (int) z_info->store_max; n++) {
		size_t k;
		int lo = 999, hi = -1, spread, best = -1;

		if (!stores[n].turnover || !stores[n].normal_num) continue;
		if (stores[n].feat == FEAT_STORE_BLACK) continue;

		for (k = 0; k < stores[n].normal_num; k++) {
			int lv = stores[n].normal_table[k]->level;
			if (lv < lo) lo = lv;
			if (lv > hi) hi = lv;
		}

		spread = hi - lo;
		if (s) {
			size_t j;
			int slo = 999, shi = -1;
			for (j = 0; j < s->normal_num; j++) {
				int lv = s->normal_table[j]->level;
				if (lv < slo) slo = lv;
				if (lv > shi) shi = lv;
			}
			best = shi - slo;
		}
		if (spread > best) s = &stores[n];
	}

	notnull(s);
	require(quality_tier_count > 0);

	/* Enough restockings that one lucky shelf cannot carry the result. */
	for (i = 0; i < 20; i++) {
		struct object *obj;

		store_stock_at_quality(s, 0);
		for (obj = s->stock; obj; obj = obj->next) {
			plain += obj->kind->level;
			plain_n++;
		}

		store_stock_at_quality(s, quality_tier_count);
		for (obj = s->stock; obj; obj = obj->next) {
			arcane += obj->kind->level;
			arcane_n++;
		}
	}

	require(plain_n > 0 && arcane_n > 0);

	printf("QUALITY %s: plain level %d over %d items; %s level %d over %d\n",
		   f_info[s->feat].name, plain / plain_n, plain_n,
		   wild_quality_name(quality_tier_count), arcane / arcane_n, arcane_n);

	/* The top of the ladder deals in deeper kinds than the bottom of it... */
	require(arcane / arcane_n > plain / plain_n);

	/* ...and keeps a fuller shelf. */
	require(arcane_n > plain_n);

	/*
	 * The other half of what the tier buys is better magic on the goods, since
	 * the raised level is what apply_magic() works from.  Measured across every
	 * ordinary shop rather than the one above, because only some of them sell
	 * anything that *can* carry a plus -- the widest range of stock in the game
	 * belongs to the alchemist, and a potion has no to-hit to improve.  Object
	 * value is no proxy either: a deeper potion is not a dearer one, and the
	 * measurement came out flat.
	 */
	for (n = 0; n < (int) z_info->store_max; n++) {
		struct store *shop = &stores[n];

		if (!shop->turnover || !shop->normal_num) continue;
		if (shop->feat == FEAT_STORE_BLACK) continue;

		for (i = 0; i < 20; i++) {
			struct object *obj;

			store_stock_at_quality(shop, 0);
			for (obj = shop->stock; obj; obj = obj->next)
				plain_plus += obj->to_h + obj->to_d + obj->to_a;

			store_stock_at_quality(shop, quality_tier_count);
			for (obj = shop->stock; obj; obj = obj->next)
				arcane_plus += obj->to_h + obj->to_d + obj->to_a;
		}
	}

	printf("QUALITY plusses on the shelves: plain %d, %s %d\n", plain_plus,
		   wild_quality_name(quality_tier_count), arcane_plus);

	require(plain_plus > 0);
	require(arcane_plus > plain_plus);

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
	/*
	 * First on purpose.  This is the only test that loads a savefile holding a
	 * remembered surface, and the bug it guards against only bites on the first
	 * load in a process -- load.c's record of how many info planes a chunk was
	 * written with is static, and any earlier test that loaded a savefile left
	 * it set, so from anywhere further down the list this passed either way.
	 */
	{ "the-map-survives-a-save-from-below", test_the_map_survives_a_save_from_below },
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
	{ "the-quality-ladder-is-a-ladder", test_the_quality_ladder_is_a_ladder },
	{ "a-blessed-beast-bounds-away", test_a_blessed_beast_bounds_away },
	{ "a-seed-keeps-its-world", test_a_seed_keeps_its_world },
	{ "a-road-out-of-a-gate-goes-somewhere", test_a_road_out_of_a_gate_goes_somewhere },
	{ "work-is-offered-where-there-is-a-door", test_work_is_offered_where_there_is_a_door },
	{ "every-race-is-playable", test_every_race_is_playable },
	{ "the-game-can-be-won", test_the_game_can_be_won },
	{ "the-unicorn-makes-you-whole", test_the_unicorn_makes_you_whole },
	{ "the-inn-dreams-by-the-law", test_the_inn_dreams_by_the_law },
	{ "a-true-dream-shows-the-nearest-place", test_a_true_dream_shows_the_nearest_place },
	{ "the-lotus-is-an-unknown-mushroom", test_the_lotus_is_an_unknown_mushroom },
	{ "the-lotus-forgets-the-world", test_the_lotus_forgets_the_world },
	{ "the-lotus-leaves-a-way-home", test_the_lotus_leaves_a_way_home },
	{ "there-is-one-home-in-the-world", test_there_is_one_home_in_the_world },
	{ "a-better-shop-deals-in-better-goods", test_a_better_shop_deals_in_better_goods },
	{ "every-service-held-is-built", test_every_service_held_is_built },
	{ "a-race-keeps-its-power", test_a_race_keeps_its_power },
	{ "a-refused-power-is-free", test_a_refused_power_is_free },
	{ "blood-pays-when-mana-cannot", test_blood_pays_when_mana_cannot },
	{ "practice-makes-a-power-surer", test_practice_makes_a_power_surer },
	{ "the-monk-has-a-ladder", test_the_monk_has_a_ladder },
	{ "bare-hands-are-a-progression", test_bare_hands_are_a_progression },
	{ "armour-takes-the-balance", test_armour_takes_the_balance },
	{ "a-monk-hits-harder-than-a-bare-fist", test_a_monk_hits_harder_than_a_bare_fist },
	{ "the-monk-keeps-zangbands-numbers", test_the_monk_keeps_zangbands_numbers },
	{ "the-mindcrafter-thinks-for-itself", test_the_mindcrafter_thinks_for_itself },
	{ "a-power-grows-with-the-character", test_a_power_grows_with_the_character },
	{ "psionics-are-paid-for", test_psionics_are_paid_for },
	{ "the-lords-of-chaos-are-all-there", test_the_lords_of_chaos_are_all_there },
	{ "a-patrons-ladder-runs-worst-to-best", test_a_patrons_ladder_runs_worst_to_best },
	{ "only-a-chaos-warrior-is-owned", test_only_a_chaos_warrior_is_owned },
	{ "thirteen-is-an-unlucky-level", test_thirteen_is_an_unlucky_level },

	{ NULL, NULL }
};
