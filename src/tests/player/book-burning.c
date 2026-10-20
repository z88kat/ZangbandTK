/* player/book-burning
 *
 * Burning a rare spellbook for experience (ZangbandTK), Zangband's reward
 * from do_cmd_destroy for a Warrior, a Paladin of Life and a Paladin of
 * Death, each of whom hates a different set of books.
 *
 * 4.2 has no destroy command, so the burning is a choice in the ignore menu
 * and a command of its own.  What is checked here is Zangband's rule -- who is
 * paid, for which books, and how much -- and that the command takes the book,
 * pays, and refuses to burn a book that pays nothing.  That the menu offers it
 * is the menu's: it asks player_book_burn_exp(), which is what is tested.
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
#include "obj-gear.h"
#include "obj-knowledge.h"
#include "obj-make.h"
#include "obj-pile.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "object.h"
#include "player-birth.h"
#include "player-calcs.h"
#include "player-spell.h"
#include "player-util.h"
#include "player-virtue.h"

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

/** A character of that class, studying that realm first if one is named. */
static bool become(const char *class_name, const char *realm_name) {
	if (!player_make_simple(NULL, class_name, "Tester")) return false;
	if (realm_name) player->realm[0] = lookup_realm(realm_name);
	player->upkeep->update |= PU_BONUS;
	update_stuff(player);
	return streq(player->class->name, class_name);
}

/** The kind of book by its name in class.txt, brackets and all. */
static struct object_kind *book_named(const char *name) {
	int k;

	for (k = 0; k < z_info->k_max; k++) {
		if (k_info[k].name && streq(k_info[k].name, name)) return &k_info[k];
	}
	return NULL;
}

/** That many of the book, known as a book picked up is known. */
static struct object *a_book(const char *name, int number) {
	struct object_kind *kind = book_named(name);
	struct object *obj;

	if (!kind) return NULL;
	obj = object_new();
	object_prep(obj, kind, 0, MINIMISE);
	obj->number = number;
	obj->known = object_new();
	object_set_base_known(player, obj);
	object_touch(player, obj);
	return obj;
}

static int32_t worth(const char *name) {
	struct object *obj = a_book(name, 1);
	int32_t exp = player_book_burn_exp(player, obj);

	object_delete(NULL, NULL, &obj->known);
	object_delete(NULL, NULL, &obj);
	return exp;
}

/** A walled room of floor, the player in the middle and nothing else. */
static void a_room(void) {
	character_dungeon = false;
	if (cave) {
		wipe_mon_list(cave, player);
		cave_free(cave);
	}
	if (player->cave) cave_free(player->cave);
	cave = t_build_arena(11, 15);
	player->cave = cave_new(cave->height, cave->width);
	player_place(cave, player, loc(7, 5));
	character_dungeon = true;
}

static int books_in_the_pack(const char *name) {
	struct object *obj;
	int n = 0;

	for (obj = player->gear; obj; obj = obj->next) {
		if (streq(obj->kind->name, name)) n += obj->number;
	}
	return n;
}

static int books_underfoot(void) {
	struct object *obj;
	int n = 0;

	for (obj = square_object(cave, player->grid); obj; obj = obj->next) {
		if (tval_is_book_k(obj->kind)) n += obj->number;
	}
	return n;
}

static void burn(struct object *obj, int amt) {
	struct command cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.code = CMD_BURN;
	cmd_set_arg_item(&cmd, "item", obj);
	cmd_set_arg_number(&cmd, "quantity", amt);
	player->upkeep->energy_use = 0;
	do_cmd_burn(&cmd);
}

/**
 * The books that can pay are the dungeon books: two in each of six realms,
 * and none in Arcane, which has four town books and no rare ones.
 */
static int test_only_the_dungeon_books_pay(void *state) {
	int k, paying = 0;

	require(become("Warrior", NULL));
	for (k = 0; k < z_info->k_max; k++) {
		struct object_kind *kind = &k_info[k];
		const struct class_book *book;

		if (!kind->name || !tval_is_book_k(kind)) continue;
		book = object_kind_to_book(kind);
		notnull(book);
		if (worth(kind->name) > 0) {
			require(book->dungeon);
			require(!streq(book->realm->name, "arcane"));
			paying++;
		} else {
			require(!book->dungeon);
		}
	}
	eq(paying, 12);
	ok;
}

/**
 * Zangband's sum: a twentieth of the maximum, capped at 10,000, a quarter of
 * it for the first of the two, and at least 1.
 */
static int test_the_sum_is_zangbands(void *state) {
	require(become("Warrior", NULL));

	player->max_exp = player->exp = 100000;
	eq(worth("[Blessings of the Grail]"), 5000);
	eq(worth("[Book of the Unicorn]"), 1250);
	eq(worth("[Necronomicon]"), 5000);

	player->max_exp = player->exp = 2000000;
	eq(worth("[Five Aces]"), 10000);
	eq(worth("[Trumps of Doom]"), 2500);

	player->max_exp = player->exp = 0;
	eq(worth("[Armageddon Tome]"), 1);
	eq(worth("[Chaos Channels]"), 1);

	player->max_exp = player->exp = PY_MAX_EXP;
	eq(worth("[Armageddon Tome]"), 0);
	ok;
}

/** A Warrior is paid for every realm's rare books, and for no town book. */
static int test_a_warrior_burns_anyones(void *state) {
	require(become("Warrior", NULL));
	player->max_exp = player->exp = 100000;
	require(worth("[Blessings of the Grail]") > 0);
	require(worth("[Necronomicon]") > 0);
	require(worth("[Nature's Wrath]") > 0);
	require(worth("[Grimoire of Power]") > 0);
	require(worth("[Armageddon Tome]") > 0);
	require(worth("[Five Aces]") > 0);
	eq(worth("[Manual of Mastery]"), 0);
	eq(worth("[High Mass]"), 0);
	ok;
}

/** A Paladin of Life burns every rare book but Life's own. */
static int test_a_paladin_of_life_burns_the_others(void *state) {
	require(become("Paladin", "life"));
	player->max_exp = player->exp = 100000;
	eq(worth("[Blessings of the Grail]"), 0);
	eq(worth("[Book of the Unicorn]"), 0);
	require(worth("[Necronomicon]") > 0);
	require(worth("[Nature's Wrath]") > 0);
	require(worth("[Five Aces]") > 0);
	eq(worth("[Black Mass]"), 0);
	ok;
}

/** A Paladin of Death burns Life's rare books, and no one else's. */
static int test_a_paladin_of_death_burns_only_life(void *state) {
	require(become("Paladin", "death"));
	player->max_exp = player->exp = 100000;
	require(worth("[Blessings of the Grail]") > 0);
	require(worth("[Book of the Unicorn]") > 0);
	eq(worth("[Necronomicon]"), 0);
	eq(worth("[Nature's Wrath]"), 0);
	eq(worth("[Armageddon Tome]"), 0);
	ok;
}

/** Nobody else is paid for burning anything, the Chaos-Warrior included. */
static int test_nobody_else_is_paid(void *state) {
	struct player_class *c;
	int k, classes_seen = 0;

	for (c = classes; c; c = c->next) {
		if (streq(c->name, "Warrior") || streq(c->name, "Paladin")) continue;
		require(become(c->name, NULL));
		player->max_exp = player->exp = 100000;
		for (k = 0; k < z_info->k_max; k++) {
			if (!k_info[k].name || !tval_is_book_k(&k_info[k])) continue;
			eq(worth(k_info[k].name), 0);
		}
		classes_seen++;
	}
	require(classes_seen > 10);
	ok;
}

/**
 * Burning one of two Grails from the pack takes one, pays for one, and takes
 * a turn.  The virtues move as Zangband's did: a Life book burnt is a step
 * towards Unlife and away from Vitality, and one worth more than 30,000 gold
 * is a sacrifice of two.
 */
static int test_burning_from_the_pack(void *state) {
	struct object *obj;

	require(become("Warrior", NULL));
	a_room();
	player->vir_types[0] = V_UNLIFE;
	player->vir_types[1] = V_VITALITY;
	player->vir_types[2] = V_SACRIFICE;
	player->virtues[0] = player->virtues[1] = player->virtues[2] = 0;
	player->max_exp = player->exp = 100000;

	obj = a_book("[Blessings of the Grail]", 2);
	notnull(obj);
	inven_carry(player, obj, true, false);
	player->upkeep->update |= (PU_BONUS | PU_INVEN);
	update_stuff(player);
	eq(books_in_the_pack("[Blessings of the Grail]"), 2);

	burn(obj, 1);
	eq(books_in_the_pack("[Blessings of the Grail]"), 1);
	eq(player->exp, 105000);
	eq(player->upkeep->energy_use, z_info->move_energy);
	eq(virtue_value(player, V_UNLIFE), 1);
	eq(virtue_value(player, V_VITALITY), -1);
	eq(virtue_value(player, V_SACRIFICE), 2);
	ok;
}

/** A whole stack from the floor: both go, and both are paid for. */
static int test_burning_from_the_floor(void *state) {
	struct object *obj;
	bool note = false;

	require(become("Warrior", NULL));
	a_room();
	player->max_exp = player->exp = 100000;

	obj = a_book("[Necronomicon]", 2);
	notnull(obj);
	require(floor_carry(cave, player->grid, obj, &note));
	eq(books_underfoot(), 2);

	burn(obj, 2);
	eq(books_underfoot(), 0);
	eq(player->exp, 110000);
	ok;
}

/** A new character, with no experience at all, still gets the one point. */
static int test_a_new_character_gets_one_point(void *state) {
	struct object *obj;

	require(become("Warrior", NULL));
	a_room();
	player->max_exp = player->exp = 0;

	obj = a_book("[Blessings of the Grail]", 1);
	notnull(obj);
	inven_carry(player, obj, true, false);
	player->upkeep->update |= (PU_BONUS | PU_INVEN);
	update_stuff(player);

	burn(obj, 1);
	eq(books_in_the_pack("[Blessings of the Grail]"), 0);
	eq(player->exp, 1);
	eq(player->max_exp, 1);
	ok;
}

/**
 * A book that pays nothing is not burnt, whoever hands it in: the command
 * asks again rather than trusting the menu, and spends no turn.
 */
static int test_a_book_that_pays_nothing_is_kept(void *state) {
	struct object *obj;

	require(become("Paladin", "life"));
	a_room();
	player->max_exp = player->exp = 100000;

	obj = a_book("[Blessings of the Grail]", 1);
	notnull(obj);
	inven_carry(player, obj, true, false);
	player->upkeep->update |= (PU_BONUS | PU_INVEN);
	update_stuff(player);

	burn(obj, 1);
	eq(books_in_the_pack("[Blessings of the Grail]"), 1);
	eq(player->exp, 100000);
	eq(player->upkeep->energy_use, 0);
	ok;
}

/** Nor is one across the room, which the look command's menu can reach. */
static int test_a_book_out_of_reach_is_kept(void *state) {
	struct object *obj;
	bool note = false;

	require(become("Warrior", NULL));
	a_room();
	player->max_exp = player->exp = 100000;

	obj = a_book("[Five Aces]", 1);
	notnull(obj);
	require(floor_carry(cave, loc(2, 2), obj, &note));

	burn(obj, 1);
	notnull(square_object(cave, loc(2, 2)));
	eq(player->exp, 100000);
	eq(player->upkeep->energy_use, 0);
	ok;
}

const char *suite_name = "player/book-burning";
struct test tests[] = {
	{ "only-the-dungeon-books-pay", test_only_the_dungeon_books_pay },
	{ "the-sum-is-zangbands", test_the_sum_is_zangbands },
	{ "a-warrior-burns-anyones", test_a_warrior_burns_anyones },
	{ "a-paladin-of-life-burns-the-others",
	  test_a_paladin_of_life_burns_the_others },
	{ "a-paladin-of-death-burns-only-life",
	  test_a_paladin_of_death_burns_only_life },
	{ "nobody-else-is-paid", test_nobody_else_is_paid },
	{ "burning-from-the-pack", test_burning_from_the_pack },
	{ "burning-from-the-floor", test_burning_from_the_floor },
	{ "a-new-character-gets-one-point",
	  test_a_new_character_gets_one_point },
	{ "a-book-that-pays-nothing-is-kept",
	  test_a_book_that_pays_nothing_is_kept },
	{ "a-book-out-of-reach-is-kept", test_a_book_out_of_reach_is_kept },
	{ NULL, NULL }
};
