/* object/figurine.c
 *
 * The magical figurine (ZangbandTK): Zangband's monster in miniature, which is
 * thrown, breaks, and leaves the monster it held behind as a pet.
 *
 * It is the first object that holds a monster race, so what is checked here is
 * mostly that the race is there everywhere the object goes -- made, named,
 * priced, thrown -- and that it comes out as a pet rather than as one more
 * thing to fight.  That it survives a save is game/roundtrip's business, and
 * that adding it moved nobody's old knowledge is game/saves'.
 */

#include "unit-test.h"
#include "test-utils.h"

#include "cave.h"
#include "cmd-core.h"
#include "game-world.h"
#include "generate.h"
#include "init.h"
#include "mon-make.h"
#include "mon-util.h"
#include "monster.h"
#include "obj-desc.h"
#include "obj-gear.h"
#include "obj-info.h"
#include "obj-knowledge.h"
#include "obj-make.h"
#include "obj-pile.h"
#include "obj-power.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "object.h"
#include "player-attack.h"
#include "player-birth.h"
#include "player-calcs.h"
#include "player-util.h"
#include "target.h"
#include "z-textblock.h"
#include <wchar.h>

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

static struct object_kind *figurine_kind(void) {
	return lookup_kind(TV_FIGURINE, lookup_sval(TV_FIGURINE, "Magical Figurine"));
}

/** A figurine of that race, known as the player would know one picked up. */
static struct object *figurine_of(struct monster_race *race) {
	struct object *obj = object_new();

	object_prep(obj, figurine_kind(), 30, MINIMISE);
	obj->race = race;
	obj->known = object_new();
	object_set_base_known(player, obj);
	object_touch(player, obj);
	return obj;
}

static void unmake(struct object *obj) {
	if (obj->known) object_delete(NULL, NULL, &obj->known);
	object_delete(NULL, NULL, &obj);
}

/**
 * The first ordinary race that will stand on floor, not deeper than `level`,
 * whose name starts with a vowel or not as asked.
 */
static struct monster_race *a_race(int level, bool vowel) {
	int i;

	for (i = 1; i < z_info->r_max; i++) {
		struct monster_race *race = &r_info[i];

		if (!race->name || !race->rarity || race->level > level) continue;
		if (rf_has(race->flags, RF_UNIQUE)) continue;
		if (monster_is_aquatic(race)) continue;
		if (rf_has(race->flags, RF_NEVER_MOVE)) continue;
		if (is_a_vowel(race->name[0]) != vowel) continue;
		return race;
	}
	return NULL;
}

static struct monster_race *a_fish(void) {
	int i;

	for (i = 1; i < z_info->r_max; i++) {
		struct monster_race *race = &r_info[i];

		if (!race->name || rf_has(race->flags, RF_UNIQUE)) continue;
		if (monster_is_aquatic(race)) return race;
	}
	return NULL;
}

static void clear_the_level(void) {
	int i;

	for (i = 1; i < cave_monster_max(cave); i++) {
		if (cave_monster(cave, i)->race) delete_monster_idx(cave, i);
	}
}

/** A walled room of floor at a depth, the player at (7, 5). */
static void a_room_at(int depth) {
	struct loc grid;

	clear_the_level();
	character_dungeon = false;
	cave_free(cave);
	if (player->cave) cave_free(player->cave);
	cave = cave_new(11, 15);
	for (grid.y = 0; grid.y < cave->height; grid.y++) {
		for (grid.x = 0; grid.x < cave->width; grid.x++) {
			bool edge = (grid.y == 0 || grid.x == 0
				|| grid.y == cave->height - 1 || grid.x == cave->width - 1);

			square_set_feat(cave, grid, edge ? FEAT_PERM : FEAT_FLOOR);
		}
	}
	player->depth = depth;
	cave->depth = depth;
	player->cave = cave_new(cave->height, cave->width);
	player->cave->depth = depth;
	player_place(cave, player, loc(7, 5));
}

/** How many monsters of this race are on the level, and on which side. */
static int count_of(const struct monster_race *race, int *pets) {
	int i, n = 0;

	*pets = 0;
	for (i = 1; i < cave_monster_max(cave); i++) {
		struct monster *mon = cave_monster(cave, i);

		if (mon->race != race) continue;
		n++;
		if (mon->allegiance == MON_ALLEGIANCE_PET) (*pets)++;
	}
	return n;
}

static int figurines_in_the_pack(void) {
	struct object *obj;
	int n = 0;

	for (obj = player->gear; obj; obj = obj->next)
		if (tval_is_figurine(obj)) n++;
	return n;
}

static int figurines_on_the_floor(void) {
	struct loc grid;
	int n = 0;

	for (grid.y = 0; grid.y < cave->height; grid.y++) {
		for (grid.x = 0; grid.x < cave->width; grid.x++) {
			struct object *obj;

			for (obj = square_object(cave, grid); obj; obj = obj->next)
				if (tval_is_figurine(obj)) n++;
		}
	}
	return n;
}

/** Carry the figurine and throw it at the grid. */
static void throw_at(struct object *obj, struct loc grid) {
	struct command cmd;

	inven_carry(player, obj, true, false);
	player->upkeep->update |= (PU_BONUS | PU_INVEN);
	update_stuff(player);

	target_set_location(grid.y, grid.x);
	memset(&cmd, 0, sizeof(cmd));
	cmd.code = CMD_THROW;
	cmd_set_arg_item(&cmd, "item", obj);
	cmd_set_arg_target(&cmd, "target", DIR_TARGET);
	do_cmd_throw(&cmd);
}

/**
 * Zangband's kind: found from 26, five pounds, and always broken by a throw.
 */
static int test_the_kind_is_zangbands(void *state) {
	struct object_kind *kind = figurine_kind();

	notnull(kind);
	eq(kind->tval, TV_FIGURINE);
	eq(kind->level, 50);
	eq(kind->weight, 50);
	eq(kind->alloc_min, 26);
	eq(kind->base->break_perc, 100);
	ok;
}

/**
 * A figurine made in the dungeon holds an ordinary monster: one that is
 * generated at all, and never a unique.  Mostly one no deeper than where it
 * was found -- a deeper one is kept only one time in the difference.
 */
static int test_a_made_figurine_holds_an_ordinary_monster(void *state) {
	int i, shallow = 0;

	for (i = 0; i < 200; i++) {
		struct object *obj = object_new();

		object_prep(obj, figurine_kind(), 30, RANDOMISE);
		apply_magic(obj, 30, false, false, false, false);
		notnull(obj->race);
		require(obj->race->rarity > 0);
		require(!rf_has(obj->race->flags, RF_UNIQUE));
		if (obj->race->level <= 30) shallow++;
		object_delete(NULL, NULL, &obj);
	}
	require(shallow > 100);
	ok;
}

/** Its name is its monster's, with the article the monster takes. */
static int test_its_name_is_its_monster(void *state) {
	struct monster_race *hard = a_race(30, false), *soft = a_race(30, true);
	struct object *obj;
	char name[160], want[160];

	notnull(hard);
	notnull(soft);

	obj = figurine_of(hard);
	object_desc(name, sizeof(name), obj, ODESC_PREFIX | ODESC_FULL, player);
	strnfmt(want, sizeof(want), "a Magical Figurine of a %s", hard->name);
	require(streq(name, want));
	unmake(obj);

	obj = figurine_of(soft);
	object_desc(name, sizeof(name), obj, ODESC_PREFIX | ODESC_FULL, player);
	strnfmt(want, sizeof(want), "a Magical Figurine of an %s", soft->name);
	require(streq(name, want));
	unmake(obj);

	/* One with nothing in it is only the kind */
	obj = figurine_of(NULL);
	object_desc(name, sizeof(name), obj, ODESC_PREFIX | ODESC_FULL, player);
	require(streq(name, "a Magical Figurine"));
	unmake(obj);
	ok;
}

/** Never in a stack, even of the same monster: Zangband's "never okay". */
static int test_figurines_never_stack(void *state) {
	struct monster_race *race = a_race(30, false);
	struct object *one = figurine_of(race), *two = figurine_of(race);

	require(!object_similar(one, two, OSTACK_PACK));
	require(!object_similar(one, two, OSTACK_STORE));
	unmake(one);
	unmake(two);
	ok;
}

/** Worth its monster's level squared, times five, and an empty one nothing. */
static int test_it_is_worth_its_monster(void *state) {
	struct monster_race *race = a_race(30, false);
	struct object *obj = figurine_of(race), *empty = figurine_of(NULL);
	int want = race->level * race->level * 5;

	eq(object_value_real(obj, 1), want);
	eq(object_value(obj, 1), want);
	eq(object_value(empty, 1), 0);
	unmake(obj);
	unmake(empty);
	ok;
}

/** And says what it is for, in Zangband's words. */
static int test_it_says_what_it_is_for(void *state) {
	struct object *obj = figurine_of(a_race(30, false));
	textblock *tb = object_info(obj, OINFO_NONE);

	notnull(wcsstr(textblock_text(tb), L"transform into a pet when thrown"));
	textblock_free(tb);
	unmake(obj);
	ok;
}

/**
 * Thrown, it is gone, and its monster is there beside where it landed -- on
 * the player's side, and only one of it even when the race comes in groups.
 */
static int test_a_thrown_figurine_becomes_a_pet(void *state) {
	struct monster_race *race = a_race(20, false);
	struct loc there;
	int i, n, pets;

	notnull(race);
	a_room_at(30);
	there = loc(player->grid.x + 4, player->grid.y);
	throw_at(figurine_of(race), there);

	n = count_of(race, &pets);
	eq(n, 1);
	eq(pets, 1);
	eq(figurines_in_the_pack(), 0);
	eq(figurines_on_the_floor(), 0);

	for (i = 1; i < cave_monster_max(cave); i++) {
		struct monster *mon = cave_monster(cave, i);

		if (mon->race == race) require(distance(mon->grid, there) <= 4);
	}
	clear_the_level();
	ok;
}

/** In single combat nothing may be summoned, so it only breaks. */
static int test_in_single_combat_it_only_shatters(void *state) {
	struct monster_race *race = a_race(20, false);
	int pets;

	notnull(race);
	a_room_at(30);
	player->upkeep->arena_level = true;
	throw_at(figurine_of(race), loc(player->grid.x + 4, player->grid.y));
	player->upkeep->arena_level = false;

	eq(count_of(race, &pets), 0);
	eq(figurines_in_the_pack(), 0);
	eq(figurines_on_the_floor(), 0);
	ok;
}

/**
 * A fish is let out into water, and not anywhere else: the summons' bare-floor
 * rule would have refused the pool, and the race's own refuses the floor.
 */
static int test_a_fish_lands_in_the_water(void *state) {
	struct monster_race *fish = a_fish();
	struct loc pool;
	struct monster *mon;

	notnull(fish);
	a_room_at(30);
	require(!place_pet_near(cave, loc(player->grid.x + 3, player->grid.y),
		fish));

	pool = loc(player->grid.x + 4, player->grid.y + 2);
	square_set_feat(cave, pool, FEAT_WATER);
	mon = place_pet_near(cave, loc(player->grid.x + 3, player->grid.y), fish);
	notnull(mon);
	require(loc_eq(mon->grid, pool));
	eq(mon->allegiance, MON_ALLEGIANCE_PET);
	clear_the_level();
	ok;
}

/** Not into lava, for something that would burn in it. */
static int test_it_is_not_let_out_into_lava(void *state) {
	struct monster_race *race = NULL;
	struct loc there;
	struct monster *mon;
	int i;

	for (i = 1; i < z_info->r_max && !race; i++) {
		struct monster_race *r = &r_info[i];

		if (!r->name || rf_has(r->flags, RF_UNIQUE)) continue;
		if (monster_is_aquatic(r) || rf_has(r->flags, RF_IM_FIRE)) continue;
		if (rf_has(r->flags, RF_CAN_FLY)) continue;
		race = r;
	}
	notnull(race);
	a_room_at(30);
	there = loc(player->grid.x + 4, player->grid.y);
	square_set_feat(cave, there, FEAT_LAVA);

	mon = place_pet_near(cave, there, race);
	notnull(mon);
	require(!loc_eq(mon->grid, there));
	require(!square_isdamaging(cave, mon->grid));
	clear_the_level();
	ok;
}

const char *suite_name = "object/figurine";
struct test tests[] = {
	{ "the-kind-is-zangbands", test_the_kind_is_zangbands },
	{ "a-made-figurine-holds-an-ordinary-monster",
	  test_a_made_figurine_holds_an_ordinary_monster },
	{ "its-name-is-its-monster", test_its_name_is_its_monster },
	{ "figurines-never-stack", test_figurines_never_stack },
	{ "it-is-worth-its-monster", test_it_is_worth_its_monster },
	{ "it-says-what-it-is-for", test_it_says_what_it_is_for },
	{ "a-thrown-figurine-becomes-a-pet",
	  test_a_thrown_figurine_becomes_a_pet },
	{ "in-single-combat-it-only-shatters",
	  test_in_single_combat_it_only_shatters },
	{ "a-fish-lands-in-the-water", test_a_fish_lands_in_the_water },
	{ "it-is-not-let-out-into-lava", test_it_is_not_let_out_into_lava },
	{ NULL, NULL }
};
