/*
 * effects/workings
 *
 * Thirteen realm spells that did nothing until 3.125.0, 3.126.0 and 3.127.0
 * (DEC-139 to DEC-141), and the effects, summon types and glyph that gave
 * them a way.  Each is exercised
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
 *   OMNICIDE        Omnicide
 *   GLYPH:EXPLOSIVE Explosive Rune
 *   TIMED_INC:WRAITH  Wraithform, the WRAITH mutation's form
 *   SHUFFLE         Shuffle, Trump's deck
 *
 * And five mutations' workings (DEC-142): SENSE_DANGER for Warning, SP_TO_HP
 * and HP_TO_SP for the two that trade pools, REPORT_MAGICS for Weigh Magic,
 * and THROW for the Launcher.
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
#include "mon-move.h"
#include "mon-util.h"
#include "mon-predicate.h"
#include "mon-summon.h"
#include "obj-curse.h"
#include "obj-desc.h"
#include "obj-gear.h"
#include "obj-knowledge.h"
#include "obj-make.h"
#include "obj-pile.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "object.h"
#include "player-birth.h"
#include "player-calcs.h"
#include "player-mutation.h"
#include "player-timed.h"
#include "player-util.h"
#include "message.h"
#include "player-attack.h"
#include "target.h"
#include "source.h"
#include "trap.h"

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


/** A monster of a race at a grid, on a side, awake. */
static struct monster *placed(struct loc grid, struct monster_race *race,
		enum monster_allegiance side) {
	struct monster_group_info info = { 0, 0 };
	struct monster *mon;

	if (!race) return NULL;
	if (!place_new_monster(cave, grid, race, false, false, info,
			ORIGIN_DROP)) return NULL;
	mon = square_monster(cave, grid);
	monster_set_allegiance(mon, side);
	mon->m_timed[MON_TMD_SLEEP] = 0;

	/*
	 * A unit test takes no player turns, so the view is stale until it is
	 * brought up to date -- and a monster the view does not reach is not
	 * active, and does not act (monster/ally-ai found this first).
	 */
	player->upkeep->update |= (PU_UPDATE_VIEW | PU_MONSTERS);
	update_stuff(player);
	return square_monster(cave, grid);
}

/** The same, by the race's name. */
static struct monster *beside(struct loc grid, const char *name,
		enum monster_allegiance side) {
	return placed(grid, lookup_monster(name), side);
}

/** The first ordinary creature at least this deep: no unique, no quest's. */
static struct monster_race *a_race_at_least(int level) {
	int i;

	for (i = 1; i < z_info->r_max; i++) {
		struct monster_race *race = &r_info[i];

		if (!race->name || race->level < level) continue;
		if (rf_has(race->flags, RF_UNIQUE)) continue;
		if (rf_has(race->flags, RF_QUESTOR)) continue;
		return race;
	}
	return NULL;
}

/** One monster processing pass, everything on the level ready to act. */
static void a_monster_turn(void) {
	int i;

	for (i = 1; i < cave_monster_max(cave); i++) {
		struct monster *mon = cave_monster(cave, i);

		if (!mon->race) continue;
		mflag_off(mon->mflag, MFLAG_HANDLED);
		mon->energy = z_info->move_energy;
	}
	process_monsters(0);
}

/** A class's spell by name, whatever book it is in. */
static const struct class_spell *a_spell(const char *cls, const char *name) {
	const struct player_class *c;
	int b, k;

	for (c = classes; c; c = c->next) {
		if (!streq(c->name, cls)) continue;
		for (b = 0; b < c->magic.num_books; b++) {
			const struct class_book *book = &c->magic.books[b];

			for (k = 0; k < book->num_spells; k++) {
				if (streq(book->spells[k].name, name)) {
					return &book->spells[k];
				}
			}
		}
	}
	return NULL;
}

/*
 * Explosive Rune.  Drawn, it is a ward: monsters must break it to pass, and
 * it breaks against break-minor-glyph, so these lower that to one -- a break
 * every time -- to ask what a break does rather than how often.
 */
static int test_the_rune_is_a_ward(void *state) {
	a_room_at(10);
	require(work(EF_GLYPH, GLYPH_EXPLOSIVE, "0", NULL));
	require(square_isexplosive(cave, player->grid));
	require(square_iswarded(cave, player->grid));

	/* And it is not the glyph of warding, which is another rune */
	require(lookup_trap("explosive rune"));
	require(!square_trap_specific(cave, player->grid,
		lookup_trap("glyph of warding")->tidx));
	ok;
}

static int test_the_rune_bursts_under_you(void *state) {
	uint16_t hardness = z_info->minor_glyph_hardness;
	struct loc next;
	struct monster *mon;
	int hp;

	a_room_at(10);
	next = loc(player->grid.x + 1, player->grid.y);
	require(work(EF_GLYPH, GLYPH_EXPLOSIVE, "0", NULL));
	mon = beside(next, "soldier", MON_ALLEGIANCE_HOSTILE);
	notnull(mon);
	player->chp = player->mhp;
	hp = player->chp;

	z_info->minor_glyph_hardness = 1;
	a_monster_turn();
	z_info->minor_glyph_hardness = hardness;

	/* Broken, and burst: the soldier beside you took the ball */
	require(!square_isexplosive(cave, player->grid));
	mon = square_monster(cave, next);
	require(!mon || mon->hp < mon->maxhp);

	/* Killed by it, the soldier never struck, and your own ball spares you */
	if (!mon) eq(player->chp, hp);
	clear_the_level();
	ok;
}

static int test_a_rune_broken_elsewhere_is_only_broken(void *state) {
	uint16_t hardness = z_info->minor_glyph_hardness;
	struct loc rune, far;
	struct monster *mon;

	a_room_at(10);
	rune = loc(player->grid.x - 1, player->grid.y);
	far = loc(player->grid.x - 2, player->grid.y);
	square_add_glyph(cave, rune, GLYPH_EXPLOSIVE);
	require(square_isexplosive(cave, rune));
	mon = beside(far, "soldier", MON_ALLEGIANCE_HOSTILE);
	notnull(mon);

	z_info->minor_glyph_hardness = 1;
	a_monster_turn();
	z_info->minor_glyph_hardness = hardness;

	require(!square_isexplosive(cave, rune));
	notnull(mon->race);
	eq(mon->hp, mon->maxhp);
	clear_the_level();
	ok;
}

static int test_the_rune_breaks_more_easily_than_warding(void *state) {
	uint16_t minor = z_info->minor_glyph_hardness;
	uint16_t major = z_info->glyph_hardness;
	struct loc next;
	int i;

	a_room_at(10);
	next = loc(player->grid.x + 1, player->grid.y);
	square_add_glyph(cave, player->grid, GLYPH_WARDING);
	notnull(beside(next, "soldier", MON_ALLEGIANCE_HOSTILE));

	/* The rune's strength does not reach the glyph of warding */
	z_info->minor_glyph_hardness = 1;
	z_info->glyph_hardness = 60000;
	for (i = 0; i < 5; i++) a_monster_turn();
	z_info->minor_glyph_hardness = minor;
	z_info->glyph_hardness = major;

	require(square_iswarded(cave, player->grid));
	require(!square_isexplosive(cave, player->grid));
	clear_the_level();
	ok;
}

/*
 * A pet steps onto your square when you are between it and what it is after:
 * in a corridor, with an enemy beyond you.  That move reaches the ward before
 * it reaches the rule that sends a pet round you, so without its own rule a
 * pet broke the rune you stood on -- and an explosive one went off in its
 * face.
 */
static int test_your_own_side_leaves_your_runes_alone(void *state) {
	uint16_t hardness = z_info->minor_glyph_hardness;
	struct monster *pet, *foe;
	struct loc grid;
	int i;

	a_room_at(10);
	for (grid.y = 1; grid.y < cave->height - 1; grid.y++) {
		for (grid.x = 1; grid.x < cave->width - 1; grid.x++) {
			if (grid.y != player->grid.y) {
				square_set_feat(cave, grid, FEAT_GRANITE);
			}
		}
	}
	require(work(EF_GLYPH, GLYPH_EXPLOSIVE, "0", NULL));
	pet = beside(loc(player->grid.x - 1, player->grid.y), "soldier",
		MON_ALLEGIANCE_PET);
	notnull(pet);
	foe = beside(loc(player->grid.x + 2, player->grid.y), "soldier",
		MON_ALLEGIANCE_HOSTILE);
	notnull(foe);
	foe->m_timed[MON_TMD_SLEEP] = 1000;

	z_info->minor_glyph_hardness = 1;
	for (i = 0; i < 5; i++) a_monster_turn();
	z_info->minor_glyph_hardness = hardness;

	require(square_isexplosive(cave, player->grid));
	notnull(pet->race);
	eq(pet->hp, pet->maxhp);
	clear_the_level();
	ok;
}

/** The first unique shallow enough to place, and not a quest's. */
static struct monster_race *a_unique(void) {
	int i;

	for (i = 1; i < z_info->r_max; i++) {
		struct monster_race *race = &r_info[i];

		if (!race->name || !rf_has(race->flags, RF_UNIQUE)) continue;
		if (rf_has(race->flags, RF_QUESTOR)) continue;
		if (race->level < 1 || race->level > 15) continue;
		if (race->cur_num >= race->max_num) continue;
		return race;
	}
	return NULL;
}

static int test_omnicide_spares_only_the_uniques(void *state) {
	struct monster_group_info info = { 0, 0 };
	struct monster_race *unique = a_unique();
	struct loc grid;
	int hp, removed = 0, i, left = 0;

	a_room_at(10);
	notnull(unique);
	for (i = 0; i < 4; i++) {
		grid = loc(2 + 2 * i, 2);
		notnull(beside(grid, "soldier", MON_ALLEGIANCE_HOSTILE));
		removed++;
	}
	notnull(beside(loc(2, 8), "soldier", MON_ALLEGIANCE_PET));
	removed++;
	require(place_new_monster(cave, loc(12, 8), unique, false, false, info,
		ORIGIN_DROP));

	player->msp = 10;
	player->csp = 0;
	player->chp = player->mhp = 100;
	hp = player->chp;

	require(work(EF_OMNICIDE, 0, "0", NULL));

	for (i = 1; i < cave_monster_max(cave); i++) {
		struct monster *mon = cave_monster(cave, i);

		if (!mon->race) continue;
		ptreq(mon->race, unique);
		left++;
	}
	eq(left, 1);

	/* A point of mana for each, and between one and four hit points */
	eq(player->csp, removed);
	require(player->chp <= hp - removed);
	require(player->chp >= hp - 4 * removed);
	clear_the_level();
	ok;
}

static int test_omnicide_takes_mana_only_to_twice_the_maximum(void *state) {
	int i;

	a_room_at(10);
	for (i = 0; i < 4; i++) {
		notnull(beside(loc(2 + 2 * i, 2), "soldier",
			MON_ALLEGIANCE_HOSTILE));
	}
	player->msp = 10;
	player->csp = 18;
	player->chp = player->mhp = 100;

	require(work(EF_OMNICIDE, 0, "0", NULL));
	eq(player->csp, 20);
	ok;
}

/* The spell itself, cast as a caster at level 40 casts it */
static int test_wraithform_is_the_wraiths_form(void *state) {
	const struct class_spell *sp = a_spell("High-Mage", "Wraithform");
	int lev = player->lev;
	bool ident = false;

	notnull(sp);
	notnull(sp->effect);
	eq(sp->effect->index, EF_TIMED_INC);
	eq(sp->effect->subtype, TMD_WRAITH);

	player->lev = 40;
	player->timed[TMD_WRAITH] = 0;
	require(effect_do(sp->effect, source_player(), NULL, &ident, true,
		DIR_NONE, 0, 0, NULL));
	player->lev = lev;

	/* Zangband's rand_range(plev / 2, plev) */
	require(player->timed[TMD_WRAITH] >= 20);
	require(player->timed[TMD_WRAITH] <= 40);
	(void) player_clear_timed(player, TMD_WRAITH, false, false);
	ok;
}

/* And the other two spells are what they are tested as above */
static int test_the_spells_carry_the_effects(void *state) {
	const struct class_spell *rune = a_spell("Mage", "Explosive Rune");
	const struct class_spell *omni = a_spell("High-Mage", "Omnicide");

	notnull(rune);
	notnull(rune->effect);
	eq(rune->effect->index, EF_GLYPH);
	eq(rune->effect->subtype, GLYPH_EXPLOSIVE);
	notnull(omni);
	notnull(omni->effect);
	eq(omni->effect->index, EF_OMNICIDE);
	ok;
}


/*
 * Shuffle.  The draw is the effect's value, so a test deals the card it
 * wants.  Zangband's bands, boundary for boundary: these deal either side of
 * a few of them.
 */
static bool deal(int die) {
	char card[16];

	strnfmt(card, sizeof(card), "%d", die);
	return work(EF_SHUFFLE, 0, card, NULL);
}

static int test_justice_is_forty_and_forty_one(void *state) {
	int die;

	a_room_at(10);
	for (die = 38; die <= 43; die++) {
		bool justice = (die == 40 || die == 41);

		(void) player_clear_timed(player, TMD_BLESSED, false, false);
		require(deal(die));
		eq(player->timed[TMD_BLESSED] > 0, justice);
	}
	(void) player_clear_timed(player, TMD_BLESSED, false, false);
	clear_the_level();
	ok;
}

static int test_the_world_is_the_last_card(void *state) {
	int32_t exp;

	a_room_at(10);
	player_exp_gain(player, 1000);
	exp = player->exp;
	require(deal(120));
	eq(player->exp, exp + exp / 25 + 1);

	/* And the Sun, one short of it, gives none */
	exp = player->exp;
	require(deal(119));
	eq(player->exp, exp);
	clear_the_level();
	ok;
}

static int test_the_fool_takes_a_mind(void *state) {
	int intel, wis;

	a_room_at(10);
	intel = player->stat_cur[STAT_INT];
	wis = player->stat_cur[STAT_WIS];
	require(deal(24));
	require(player->stat_cur[STAT_INT] < intel);
	require(player->stat_cur[STAT_WIS] < wis);
	clear_the_level();
	ok;
}

static int test_judgement_cures_every_mutation(void *state) {
	int i;

	a_room_at(10);
	for (i = 0; i < 3; i++) (void) player_mutate(player);
	require(player_mutation_total(player) > 0);
	require(deal(105));
	eq(player_mutation_total(player), 0);
	clear_the_level();
	ok;
}

static int test_the_pictures_are_the_four_groups(void *state) {
	static const struct { int die; const char *base; } cards[] = {
		{ 80, "mold" }, { 83, "bat" }, { 84, "vortex" },
		{ 87, "creeping coins" },
	};
	size_t c;

	a_room_at(40);
	for (c = 0; c < N_ELEMENTS(cards); c++) {
		int tries, i, found = 0;

		for (tries = 0; tries < 10 && !found; tries++) {
			clear_the_level();
			require(deal(cards[c].die));
			for (i = 1; i < cave_monster_max(cave); i++) {
				struct monster *mon = cave_monster(cave, i);

				if (!mon->race) continue;
				require(streq(mon->race->base->name, cards[c].base));
				require(monster_is_pet(mon));
				found++;
			}
		}
		require(found > 0);
	}
	clear_the_level();
	ok;
}

static int test_death_calls_against_you(void *state) {
	int i, hostile = 0;

	a_room_at(40);
	require(deal(3));
	for (i = 1; i < cave_monster_max(cave); i++) {
		struct monster *mon = cave_monster(cave, i);

		if (!mon->race) continue;
		require(monster_is_hostile(mon));
		hostile++;
	}
	require(hostile > 0);
	clear_the_level();
	ok;
}

/* The card sharps draw on a better deck */
static int test_the_card_sharps_draw_higher(void *state) {
	const struct class_spell *mage = a_spell("Mage", "Shuffle");
	const struct class_spell *rogue = a_spell("Rogue", "Shuffle");
	const struct class_spell *high = a_spell("High-Mage", "Shuffle");

	notnull(mage);
	notnull(rogue);
	notnull(high);
	eq(mage->effect->index, EF_SHUFFLE);
	eq(rogue->effect->index, EF_SHUFFLE);
	eq(high->effect->index, EF_SHUFFLE);

	/* d120 for the Mage; d110 plus a fifth of the level for the other two */
	require(dice_test_values(mage->effect->dice, 0, 1, 120, 0));
	require(dice_test_values(rogue->effect->dice, 0, 1, 110, 0));
	require(dice_test_variables(rogue->effect->dice, "B", NULL, NULL, NULL));
	require(dice_test_values(high->effect->dice, 0, 1, 110, 0));
	require(dice_test_variables(high->effect->dice, "B", NULL, NULL, NULL));
	ok;
}


/* Whether one of the last few messages says this */
static bool said(const char *text) {
	uint16_t age;

	for (age = 0; age < 8 && age < messages_num(); age++) {
		if (strstr(message_str(age), text)) return true;
	}
	return false;
}

/* Warning: the sum of how far each hostile creature out-levels you */
static int test_warning_weighs_the_level(void *state) {
	struct monster *foe;

	a_room_at(10);
	player->lev = 1;
	require(work(EF_SENSE_DANGER, 0, "0", NULL));
	require(said("You feel lonely."));

	/* Your own creatures are no danger, however deep */
	foe = placed(loc(2, 2), a_race_at_least(30), MON_ALLEGIANCE_PET);
	notnull(foe);
	require(work(EF_SENSE_DANGER, 0, "0", NULL));
	require(said("You feel lonely."));

	/* A hostile one thirty levels above a beginner is a fright */
	foe = placed(loc(4, 2), a_race_at_least(30), MON_ALLEGIANCE_HOSTILE);
	notnull(foe);
	require(work(EF_SENSE_DANGER, 0, "0", NULL));
	require(said("You feel very worried!") || said("You feel terrified!")
		|| said("You feel utterly terrified!"));
	clear_the_level();
	ok;
}

static int test_spell_points_heal_wounds(void *state) {
	player->mhp = 100;
	player->chp = 50;
	player->msp = 40;
	player->csp = 30;
	require(work(EF_SP_TO_HP, 0, "0", NULL));
	eq(player->chp, 80);
	eq(player->csp, 0);

	/* Only as much as the wounds need */
	player->chp = 90;
	player->csp = 30;
	require(work(EF_SP_TO_HP, 0, "0", NULL));
	eq(player->chp, 100);
	eq(player->csp, 20);
	ok;
}

static int test_hit_points_restore_mana(void *state) {
	player->mhp = player->chp = 100;
	player->msp = 50;
	player->csp = 10;
	require(work(EF_HP_TO_SP, 0, "0", NULL));
	eq(player->csp, 50);
	eq(player->chp, 60);

	/* Every hit point, if that is what is missing: none left, and alive */
	player->chp = 20;
	player->csp = 0;
	require(work(EF_HP_TO_SP, 0, "0", NULL));
	eq(player->csp, 20);
	eq(player->chp, 0);
	require(!player->is_dead);
	player->chp = player->mhp;
	ok;
}

static int test_weigh_magic_says_how_long(void *state) {
	int i;

	for (i = 0; i < TMD_MAX; i++) {
		if (i != TMD_FOOD) player->timed[i] = 0;
	}
	player->word_recall = 0;
	player->deep_descent = 0;

	/* Hunger is a grade, not a magic */
	require(player->timed[TMD_FOOD] > 0);
	require(work(EF_REPORT_MAGICS, 0, "0", NULL));
	require(said("You sense no magic upon you."));

	player->timed[TMD_FAST] = 15;
	player->timed[TMD_BLESSED] = 300;
	require(work(EF_REPORT_MAGICS, 0, "0", NULL));
	require(said("Haste will last for a while."));
	require(said("for an incredibly long time."));
	require(!said("ourishment"));
	player->timed[TMD_FAST] = 0;
	player->timed[TMD_BLESSED] = 0;
	ok;
}

/*
 * The Launcher: a throw at a launcher's might does that many times the
 * damage.  Thirty daggers at might one and thirty at might three, into a
 * soldier with hit points to spare; the second lot has to do better than
 * twice the first, which chance alone does not explain.
 */
static int thrown_into(struct monster *mon, struct object *stack, int might) {
	int before = mon->hp, n;

	for (n = 0; n < 30; n++) {
		struct command cmd;

		memset(&cmd, 0, sizeof(cmd));
		cmd.code = CMD_THROW;
		cmd_set_arg_item(&cmd, "item", stack);
		cmd_set_arg_target(&cmd, "target", DIR_TARGET);
		player_throw_with_might(player, might, &cmd);
	}
	return before - mon->hp;
}

static int test_the_launcher_throws_harder(void *state) {
	struct object *stack = make(TV_SWORD, "Dagger");
	struct monster *mon;
	int plain, mighty;

	a_room_at(10);
	mon = beside(loc(player->grid.x + 3, player->grid.y), "soldier",
		MON_ALLEGIANCE_HOSTILE);
	notnull(mon);
	mon->maxhp = mon->hp = 30000;
	mon->m_timed[MON_TMD_SLEEP] = 30000;
	require(target_set_monster(mon));

	/* Ten a hit, every hit, so what varies is only whether and how well */
	notnull(stack);
	stack->number = 60;
	stack->dd = 1;
	stack->ds = 1;
	stack->to_d = 9;
	stack->to_h = 50;
	inven_carry(player, stack, true, false);
	player->upkeep->update |= (PU_BONUS | PU_INVEN);
	update_stuff(player);

	plain = thrown_into(mon, stack, 1);
	mighty = thrown_into(mon, stack, 3);
	require(plain > 0);
	require(mighty > 2 * plain);
	clear_the_level();
	ok;
}

/** How many things are in the pack, walking it as everything else does. */
static int pack_count(void) {
	struct object *obj;
	int n = 0;

	for (obj = player->gear; obj; obj = obj->next) n++;
	return n;
}

static void drop_from_pack(struct object *obj) {
	bool none_left = false;

	unmake(gear_object_for_use(player, obj, obj->number, false, &none_left));
}

/*
 * Mundanity strips the magic and leaves the object where it was.
 *
 * object_prep() clears the whole structure, and until 3.129.0 only the stack
 * size was put back: the pile links went with the rest, so the object was cut
 * out of the pack along with everything after it, and the pointer to the
 * player's record of it went too, which nearly every description reads.  The
 * dagger goes in first, so that two more things hang off its `next`.
 */
static int test_mundanity_leaves_the_pack_whole(void *state) {
	struct object *dagger = make(TV_SWORD, "Dagger");
	struct object *flask = make(TV_FLASK, "Flask of oil");
	struct object *torch = make(TV_LIGHT, "Wooden Torch");
	int before;
	char name[80];

	notnull(dagger);
	notnull(flask);
	notnull(torch);

	/* Not absorbed into the kit's own flasks and torches: three objects */
	inven_carry(player, dagger, false, false);
	inven_carry(player, flask, false, false);
	inven_carry(player, torch, false, false);
	before = pack_count();
	dagger->to_h = 9;

	require(work(EF_MUNDANE, 0, "0", dagger));
	eq(pack_count(), before);
	eq(dagger->to_h, 0);
	notnull(dagger->known);
	object_desc(name, sizeof(name), dagger, ODESC_BASE, player);
	require(!streq(name, "(nothing)"));

	/* And it says so in words, not in object.txt's "& Dagger~" */
	notnull(strstr(message_str(0), "is a Dagger once more."));

	drop_from_pack(dagger);
	drop_from_pack(flask);
	drop_from_pack(torch);
	ok;
}

/* And a figurine is a figurine of the same monster afterwards */
static int test_mundanity_keeps_a_figurines_monster(void *state) {
	struct object *fig = make(TV_FIGURINE, "Magical Figurine");
	struct monster_race *race = NULL;
	int i;

	notnull(fig);
	for (i = 1; i < z_info->r_max && !race; i++)
		if (r_info[i].name && !rf_has(r_info[i].flags, RF_UNIQUE))
			race = &r_info[i];
	fig->race = race;
	fig->known->race = race;
	inven_carry(player, fig, false, false);

	require(work(EF_MUNDANE, 0, "0", fig));
	require(fig->race == race);
	require(fig->known->race == race);

	drop_from_pack(fig);
	ok;
}

/* And the two powers and three random mutations carry them */
static int test_the_mutations_carry_the_effects(void *state) {
	static const struct { const char *name; int effect; } built[] = {
		{ "WEIGH_MAG", EF_REPORT_MAGICS }, { "LAUNCHER", EF_THROW },
	};
	static const struct { const char *name; int effect; } fires[] = {
		{ "WARNING", EF_SENSE_DANGER }, { "SP_TO_HP", EF_SP_TO_HP },
		{ "HP_TO_SP", EF_HP_TO_SP },
	};
	size_t i;

	for (i = 0; i < N_ELEMENTS(built); i++) {
		const struct mutation *m = mutation_by_name(built[i].name);

		notnull(m);
		notnull(m->action);
		notnull(m->action->effects);
		eq(m->action->effects->effect->index, built[i].effect);
	}
	for (i = 0; i < N_ELEMENTS(fires); i++) {
		const struct mutation *m = mutation_by_name(fires[i].name);

		notnull(m);
		notnull(m->fires);
		notnull(m->fires->effects);
		eq(m->fires->effects->effect->index, fires[i].effect);
	}
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
	{ "the-rune-is-a-ward", test_the_rune_is_a_ward },
	{ "the-rune-bursts-under-you", test_the_rune_bursts_under_you },
	{ "a-rune-broken-elsewhere-is-only-broken",
	  test_a_rune_broken_elsewhere_is_only_broken },
	{ "the-rune-breaks-more-easily-than-warding",
	  test_the_rune_breaks_more_easily_than_warding },
	{ "your-own-side-leaves-your-runes-alone",
	  test_your_own_side_leaves_your_runes_alone },
	{ "omnicide-spares-only-the-uniques",
	  test_omnicide_spares_only_the_uniques },
	{ "omnicide-takes-mana-only-to-twice-the-maximum",
	  test_omnicide_takes_mana_only_to_twice_the_maximum },
	{ "wraithform-is-the-wraiths-form", test_wraithform_is_the_wraiths_form },
	{ "the-spells-carry-the-effects", test_the_spells_carry_the_effects },
	{ "justice-is-forty-and-forty-one", test_justice_is_forty_and_forty_one },
	{ "the-world-is-the-last-card", test_the_world_is_the_last_card },
	{ "the-fool-takes-a-mind", test_the_fool_takes_a_mind },
	{ "judgement-cures-every-mutation", test_judgement_cures_every_mutation },
	{ "the-pictures-are-the-four-groups",
	  test_the_pictures_are_the_four_groups },
	{ "death-calls-against-you", test_death_calls_against_you },
	{ "the-card-sharps-draw-higher", test_the_card_sharps_draw_higher },
	{ "warning-weighs-the-level", test_warning_weighs_the_level },
	{ "spell-points-heal-wounds", test_spell_points_heal_wounds },
	{ "hit-points-restore-mana", test_hit_points_restore_mana },
	{ "weigh-magic-says-how-long", test_weigh_magic_says_how_long },
	{ "the-launcher-throws-harder", test_the_launcher_throws_harder },
	{ "mundanity-leaves-the-pack-whole",
	  test_mundanity_leaves_the_pack_whole },
	{ "mundanity-keeps-a-figurines-monster",
	  test_mundanity_keeps_a_figurines_monster },
	{ "the-mutations-carry-the-effects",
	  test_the_mutations_carry_the_effects },
	{ NULL, NULL }
};
