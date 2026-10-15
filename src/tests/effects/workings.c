/*
 * effects/workings
 *
 * Nine realm spells that did nothing until 3.125.0 (DEC-139), and the five
 * effects and four summon types that gave them a way.  Each is exercised
 * rather than merely present: player/realm counts the spells that have an
 * effect, and an effect that is wired up and does the wrong thing passes that
 * count as easily as one that works.
 *
 *   IDENTIFY_FULLY  Identify True, Holy Vision, Stone Tell, Trump Lore
 *   SELF_KNOWLEDGE  Self Knowledge
 *   BLESS_WEAPON    Bless Weapon
 *   REFUEL_LIGHT    Phlogiston
 *   RUSTPROOF       Protect from Corrosion
 *   BIZARRE_*       Joker Card's four groups
 */

#include "unit-test.h"
#include "test-utils.h"

#include "cave.h"
#include "cmd-core.h"
#include "effects.h"
#include "game-world.h"
#include "generate.h"
#include "init.h"
#include "mon-make.h"
#include "mon-predicate.h"
#include "mon-summon.h"
#include "obj-curse.h"
#include "obj-gear.h"
#include "obj-knowledge.h"
#include "obj-make.h"
#include "obj-pile.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "object.h"
#include "player-birth.h"
#include "player-calcs.h"
#include "player-util.h"
#include "source.h"

int setup_tests(void **state) {
	set_file_paths();
	if (!init_angband()) return 1;
#ifdef UNIX
	create_needed_dirs();
#endif
	if (!player_make_simple(NULL, "Priest", "Tester")) {
		cleanup_angband();
		return 1;
	}
	(void) test_seed_rng_reported(suite_name);
	prepare_next_level(player);
	on_new_level();
	return 0;
}

int teardown_tests(void *state) {
	if (cave) wipe_mon_list(cave, player);
	cleanup_angband();
	return 0;
}

/** An object of a named kind, with the player knowing what any object shows. */
static struct object *make(int tval, const char *name) {
	struct object_kind *kind = lookup_kind(tval, lookup_sval(tval, name));
	struct object *obj;

	if (!kind) return NULL;
	obj = object_new();
	object_prep(obj, kind, 1, MINIMISE);
	obj->known = object_new();
	object_set_base_known(player, obj);
	object_touch(player, obj);
	return obj;
}

static void unmake(struct object *obj) {
	if (obj->known) object_delete(NULL, NULL, &obj->known);
	object_delete(NULL, NULL, &obj);
}

/** Put an object in the body slot it goes in. */
static void wear(struct object *obj) {
	int slot = wield_slot(obj);

	player->body.slots[slot].obj = obj;
	player->upkeep->equip_cnt++;
	player->upkeep->update |= (PU_BONUS | PU_TORCH);
	update_stuff(player);
}

static void take_off(struct object *obj) {
	int slot = wield_slot(obj);

	player->body.slots[slot].obj = NULL;
	player->upkeep->equip_cnt--;
	player->upkeep->update |= (PU_BONUS | PU_TORCH);
	update_stuff(player);
}

/**
 * Run an effect as a spell does, with `obj` as the answer to its item prompt.
 * With no answer, or one the effect will not take, there is no prompt to fall
 * back on here, so the effect is cancelled -- which is what the tests that
 * pass nothing are asking about.  Returns whether the effect was used.
 */
static bool work(int index, int subtype, const char *dice,
		struct object *obj) {
	struct effect effect;
	struct command cmd;
	bool ident = false, used;

	memset(&effect, 0, sizeof(effect));
	effect.index = index;
	effect.subtype = subtype;
	effect.dice = dice_new();
	dice_parse_string(effect.dice, dice);
	memset(&cmd, 0, sizeof(cmd));
	if (obj) cmd_set_arg_item(&cmd, "tgtitem", obj);
	used = effect_do(&effect, source_player(), NULL, &ident, true, DIR_NONE,
		0, 0, &cmd);
	dice_free(effect.dice);
	return used;
}

/** Give an object runes the player cannot know yet: new ones every call. */
static int next_flag = 0;
static void give_unknown_runes(struct object *obj) {
	static const int flags[] = {
		OF_SEE_INVIS, OF_FREE_ACT, OF_SLOW_DIGEST, OF_FEATHER,
		OF_REGEN, OF_TELEPATHY, OF_HOLD_LIFE, OF_PROT_FEAR,
	};
	int n;

	for (n = 0; n < 2; n++) {
		of_on(obj->flags, flags[next_flag % N_ELEMENTS(flags)]);
		next_flag++;
	}
	obj->modifiers[OBJ_MOD_STEALTH] = 2;
}

static int test_identify_fully_learns_every_rune(void *state) {
	struct object *obj = make(TV_SWORD, "Dagger");

	notnull(obj);
	give_unknown_runes(obj);
	player_know_object(player, obj);
	require(!object_runes_known(obj));

	require(work(EF_IDENTIFY_FULLY, 0, "0", obj));
	require(object_runes_known(obj));
	unmake(obj);
	ok;
}

/* Where IDENTIFY, the spell it is the big brother of, learns one */
static int test_identify_learns_only_one(void *state) {
	struct object *obj = make(TV_SWORD, "Dagger");

	notnull(obj);
	give_unknown_runes(obj);
	player_know_object(player, obj);
	require(!object_runes_known(obj));

	require(work(EF_IDENTIFY, 0, "0", obj));
	require(!object_runes_known(obj));
	unmake(obj);
	ok;
}

static int test_identify_fully_is_not_offered_the_known(void *state) {
	struct object *obj = make(TV_SWORD, "Dagger");

	notnull(obj);
	object_learn_all_runes(player, obj);
	require(object_runes_known(obj));

	/* Nothing to learn, so not an answer, so nothing cast */
	require(!work(EF_IDENTIFY_FULLY, 0, "0", obj));
	unmake(obj);
	ok;
}

static int test_self_knowledge_learns_what_is_worn(void *state) {
	struct object *obj = make(TV_SOFT_ARMOR, "Soft Leather Armour");

	notnull(obj);
	give_unknown_runes(obj);
	wear(obj);
	player_know_object(player, obj);
	require(!object_runes_known(obj));

	require(work(EF_SELF_KNOWLEDGE, 0, "0", NULL));
	require(object_runes_known(obj));
	take_off(obj);
	unmake(obj);
	ok;
}

static int test_bless_weapon_blesses(void *state) {
	struct object *obj = make(TV_SWORD, "Dagger");

	notnull(obj);
	require(!of_has(obj->flags, OF_BLESSED));
	require(work(EF_BLESS_WEAPON, 0, "0", obj));
	require(of_has(obj->flags, OF_BLESSED));

	/* And the player can see it: the rune is learned with it */
	player_know_object(player, obj);
	require(of_has(obj->known->flags, OF_BLESSED));
	unmake(obj);
	ok;
}

static int test_bless_weapon_drives_out_a_curse(void *state) {
	struct object *obj = make(TV_SWORD, "Dagger");
	int curse = lookup_curse("vulnerability");

	notnull(obj);
	require(curse > 0);
	require(append_object_curse(obj, curse, 40));
	notnull(obj->curses);

	require(work(EF_BLESS_WEAPON, 0, "0", obj));
	null(obj->curses);
	require(of_has(obj->flags, OF_BLESSED));
	unmake(obj);
	ok;
}

static int test_a_permanent_curse_disrupts_the_blessing(void *state) {
	struct object *obj = make(TV_SWORD, "Dagger");
	int curse = lookup_curse("vulnerability");

	notnull(obj);
	require(append_object_curse(obj, curse, 100));

	/* The casting is spent, but nothing changes */
	require(work(EF_BLESS_WEAPON, 0, "0", obj));
	notnull(obj->curses);
	eq(obj->curses[curse].power, 100);
	require(!of_has(obj->flags, OF_BLESSED));
	unmake(obj);
	ok;
}

static int test_bless_weapon_takes_only_melee_weapons(void *state) {
	struct object *obj = make(TV_SOFT_ARMOR, "Soft Leather Armour");

	notnull(obj);
	require(!work(EF_BLESS_WEAPON, 0, "0", obj));
	require(!of_has(obj->flags, OF_BLESSED));
	unmake(obj);
	ok;
}

static int test_phlogiston_fills_a_lantern_by_half(void *state) {
	struct object *obj = make(TV_LIGHT, "Lantern");
	int full = z_info->fuel_lamp;

	notnull(obj);
	require(of_has(obj->flags, OF_TAKES_FUEL));
	obj->timeout = 100;
	wear(obj);

	require(work(EF_REFUEL_LIGHT, 0, "0", NULL));
	eq(obj->timeout, 100 + full / 2);

	/* Then to the brim, and no further */
	require(work(EF_REFUEL_LIGHT, 0, "0", NULL));
	eq(obj->timeout, full);
	require(!work(EF_REFUEL_LIGHT, 0, "0", NULL));
	eq(obj->timeout, full);

	take_off(obj);
	unmake(obj);
	ok;
}

static int test_phlogiston_fills_a_torch_too(void *state) {
	struct object *obj = make(TV_LIGHT, "Wooden Torch");

	notnull(obj);
	require(of_has(obj->flags, OF_BURNS_OUT));
	obj->timeout = 10;
	wear(obj);

	require(work(EF_REFUEL_LIGHT, 0, "0", NULL));
	eq(obj->timeout, 10 + z_info->fuel_torch / 2);

	take_off(obj);
	unmake(obj);
	ok;
}

static int test_phlogiston_needs_a_light_that_burns(void *state) {
	require(!equipped_item_by_slot_name(player, "light"));
	require(!work(EF_REFUEL_LIGHT, 0, "0", NULL));
	ok;
}

static int test_rustproofing_ignores_acid_and_mends(void *state) {
	struct object *obj = make(TV_SOFT_ARMOR, "Soft Leather Armour");

	notnull(obj);
	obj->to_a = -3;
	require(!(obj->el_info[ELEM_ACID].flags & EL_INFO_IGNORE));

	require(work(EF_RUSTPROOF, 0, "0", obj));
	require(obj->el_info[ELEM_ACID].flags & EL_INFO_IGNORE);
	eq(obj->to_a, 0);

	/* Acid's rune is learned, so the armour shows it */
	player_know_object(player, obj);
	require(obj->known->el_info[ELEM_ACID].flags & EL_INFO_IGNORE);

	/* And it is not offered a second time */
	require(!work(EF_RUSTPROOF, 0, "0", obj));
	unmake(obj);
	ok;
}

static int test_rustproofing_leaves_cursed_damage(void *state) {
	struct object *obj = make(TV_SOFT_ARMOR, "Soft Leather Armour");

	notnull(obj);
	obj->to_a = -3;
	require(append_object_curse(obj, lookup_curse("vulnerability"), 40));

	require(work(EF_RUSTPROOF, 0, "0", obj));
	require(obj->el_info[ELEM_ACID].flags & EL_INFO_IGNORE);
	eq(obj->to_a, -3);
	unmake(obj);
	ok;
}

static int test_rustproofing_takes_only_armour(void *state) {
	struct object *obj = make(TV_SWORD, "Dagger");

	notnull(obj);
	require(!work(EF_RUSTPROOF, 0, "0", obj));
	unmake(obj);
	ok;
}

/** Everything off the level, one at a time, as game/carry clears it. */
static void clear_the_level(void) {
	int i;

	for (i = 1; i < cave_monster_max(cave); i++) {
		if (cave_monster(cave, i)->race) delete_monster_idx(cave, i);
	}
}

/**
 * A walled room of floor at a depth, with the player in the middle: room to
 * summon into whatever the dungeon would have made, as effects/swap does it.
 */
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

/*
 * Each of Joker Card's four groups calls one of its own, and on the caster's
 * side.  Deep enough for every group to have something to send: the
 * shallowest vortex lives at 21, and like every Trump summon these call at the
 * dungeon's depth.  In a room rather than a generated level, which now and
 * then put the player where nothing could be summoned beside them at all.
 */
static int test_joker_cards_four_groups_answer(void *state) {
	static const char *const groups[][2] = {
		{ "BIZARRE_MOLD", "mold" },
		{ "BIZARRE_BAT", "bat" },
		{ "BIZARRE_VORTEX", "vortex" },
		{ "BIZARRE_COINS", "creeping coins" },
	};
	size_t g;

	a_room_at(40);

	for (g = 0; g < N_ELEMENTS(groups); g++) {
		int type = summon_name_to_idx(groups[g][0]);
		int tries, i, found = 0;

		require(type >= 0);
		for (tries = 0; tries < 10 && !found; tries++) {
			clear_the_level();
			effect_simple(EF_SUMMON_PET, source_player(), "1", type, 0, 0,
				0, 0, NULL);
			for (i = 1; i < cave_monster_max(cave); i++) {
				struct monster *mon = cave_monster(cave, i);

				if (!mon->race) continue;
				require(streq(mon->race->base->name, groups[g][1]));
				require(!rf_has(mon->race->flags, RF_UNIQUE));
				require(monster_is_pet(mon));
				found++;
			}
		}
		require(found > 0);
	}
	clear_the_level();
	ok;
}

const char *suite_name = "effects/workings";
struct test tests[] = {
	{ "identify-fully-learns-every-rune",
	  test_identify_fully_learns_every_rune },
	{ "identify-learns-only-one", test_identify_learns_only_one },
	{ "identify-fully-is-not-offered-the-known",
	  test_identify_fully_is_not_offered_the_known },
	{ "self-knowledge-learns-what-is-worn",
	  test_self_knowledge_learns_what_is_worn },
	{ "bless-weapon-blesses", test_bless_weapon_blesses },
	{ "bless-weapon-drives-out-a-curse",
	  test_bless_weapon_drives_out_a_curse },
	{ "a-permanent-curse-disrupts-the-blessing",
	  test_a_permanent_curse_disrupts_the_blessing },
	{ "bless-weapon-takes-only-melee-weapons",
	  test_bless_weapon_takes_only_melee_weapons },
	{ "phlogiston-fills-a-lantern-by-half",
	  test_phlogiston_fills_a_lantern_by_half },
	{ "phlogiston-fills-a-torch-too", test_phlogiston_fills_a_torch_too },
	{ "phlogiston-needs-a-light-that-burns",
	  test_phlogiston_needs_a_light_that_burns },
	{ "rustproofing-ignores-acid-and-mends",
	  test_rustproofing_ignores_acid_and_mends },
	{ "rustproofing-leaves-cursed-damage",
	  test_rustproofing_leaves_cursed_damage },
	{ "rustproofing-takes-only-armour", test_rustproofing_takes_only_armour },
	{ "joker-cards-four-groups-answer", test_joker_cards_four_groups_answer },
	{ NULL, NULL }
};
