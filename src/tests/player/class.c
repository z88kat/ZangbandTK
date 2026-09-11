/* player/class — what a class is, and what it grows into (PLR-06)
 *
 * A home for the class assertions, which had nowhere to live: `player/realm`
 * covers spells and `player/race` covers races, and the level gates below
 * belong to neither. The race suite exists because two race tests had
 * accumulated in the 6,000-line wilderness suite for want of anywhere better;
 * putting class tests there would have repeated that exactly.
 *
 * Zangband gates a class's intrinsics on character level the same way it gates
 * a race's -- `player_flags()`, files.c:1278 -- and DEC-72 recorded that no
 * class needed the mechanism. Four do, and DEC-78 is the correction. The Monk's
 * two are gated on its armour as well as its level, which is the only condition
 * the archive has and the reason a gain carries one at all.
 */
#include "unit-test.h"

#include "init.h"
#include "player.h"
#include "player-birth.h"
#include "player-calcs.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "obj-make.h"
#include "obj-pile.h"
#include "obj-knowledge.h"
#include "obj-gear.h"
#include "obj-desc.h"
#include "test-utils.h"

int setup_tests(void **state) {
	set_file_paths();
	init_angband();
	if (!player_make_simple(NULL, NULL, "Tester")) return 1;
	*state = NULL;
	return 0;
}

int teardown_tests(void *state) {
	cleanup_angband();
	return 0;
}

static struct player_class *class_named(const char *name) {
	struct player_class *c = classes;
	while (c && !streq(c->name, name)) c = c->next;
	return c;
}

/** Put the player in this class at this level, and recalculate. */
static struct player *as_class(const char *name, int lev) {
	struct player_class *c = class_named(name);

	if (!c) return NULL;
	player->class = c;
	player->lev = lev;
	player->upkeep->update |= (PU_BONUS);
	update_stuff(player);
	return player;
}

/** Whether the character has that object flag right now. */
static bool has_flag(struct player *p, int flag) {
	bitflag f[OF_SIZE];

	player_flags(p, f);
	return of_has(f, flag);
}

/**
 * Four classes gain something on a level threshold, and not before it.
 *
 * Zangband's class switch (files.c:1278) gates six classes. The Warrior's is
 * 4.2's own BRAVERY_30 and the Ranger's two want a terrain penalty this game
 * has not got, so these four are what was missing. Each row is asserted on both
 * sides of its threshold, because a gate that is always open looks exactly like
 * a gate that works if you only ever test above it.
 */
static int test_the_class_gates_open_on_time(void *state) {
	static const struct {
		const char *class; int level, flag;
	} rows[] = {
		{ "Paladin",       40, OF_PROT_FEAR },
		{ "Chaos-Warrior", 40, OF_PROT_FEAR },
		{ "Mindcrafter",   10, OF_PROT_FEAR },
		{ "Mindcrafter",   20, OF_SUST_WIS },
		{ "Mindcrafter",   30, OF_PROT_CONF },
		{ "Mindcrafter",   40, OF_TELEPATHY },
	};
	size_t i;

	for (i = 0; i < N_ELEMENTS(rows); i++) {
		struct player *p = as_class(rows[i].class, rows[i].level - 1);

		require(p);
		if (has_flag(p, rows[i].flag)) {
			printf("%s has flag %d at level %d, one early\n", rows[i].class,
					rows[i].flag, rows[i].level - 1);
			require(false);
		}

		p = as_class(rows[i].class, rows[i].level);
		if (!has_flag(p, rows[i].flag)) {
			printf("%s lacks flag %d at level %d\n", rows[i].class,
					rows[i].flag, rows[i].level);
			require(false);
		}
	}
	ok;
}

/**
 * A Chaos-Warrior's chaos resistance is a resist, not a flag, and arrives at 30.
 *
 * Separate from the flags above because it travels a different road through
 * `calc_bonuses()` -- `el_info` rather than the flag set -- and the gain
 * mechanism has to carry both. A gate built only for flags would pass every
 * test above and grant this at birth.
 */
static int test_a_chaos_warrior_grows_into_chaos(void *state) {
	struct player *p = as_class("Chaos-Warrior", 29);

	require(p);
	eq(p->state.el_info[ELEM_CHAOS].res_level, 0);

	p = as_class("Chaos-Warrior", 30);
	eq(p->state.el_info[ELEM_CHAOS].res_level, 1);
	ok;
}

/**
 * A Monk's two arrive only while it is light enough to fight bare-handed.
 *
 * `if (!p_ptr->state.monk_armour_stat)` wraps both in the archive
 * (files.c:1301), and it is the only condition any class gate has. Speed is
 * `lev / 10` -- the same arithmetic a Sprite gets, because Zangband turns the
 * flag into that sum for race and class alike (xtra1.c:2550).
 *
 * The armour is the assertion that matters. A gate that ignores the condition
 * passes every level check and hands a Monk in full plate the speed of one in
 * nothing.
 */
static int test_a_monks_gains_want_an_empty_pack(void *state) {
	struct player *p = as_class("Monk", 25);
	struct object_kind *kind;
	struct object *armour;
	int bare_speed;

	require(p);

	/* Unencumbered: speed from 10, free action from 25. */
	require(has_flag(p, OF_FREE_ACT));
	bare_speed = p->state.speed;

	p = as_class("Monk", 9);
	require(!has_flag(p, OF_FREE_ACT));

	/* Now weigh it down. 100 + lev*4 is the limit, so at 25 that is 200. */
	kind = lookup_kind(TV_HARD_ARMOR,
			lookup_sval(TV_HARD_ARMOR, "Full Plate Armour"));
	require(kind);
	armour = object_new();
	object_prep(armour, kind, 0, MINIMISE);
	armour->known = object_new();
	object_set_base_known(player, armour);
	inven_carry(player, armour, true, false);
	inven_wield(armour, wield_slot(armour));

	p = as_class("Monk", 25);
	if (has_flag(p, OF_FREE_ACT)) {
		printf("a Monk in full plate still has free action\n");
		require(false);
	}
	if (p->state.speed >= bare_speed) {
		printf("a Monk in full plate is %d, bare-handed was %d\n",
				p->state.speed, bare_speed);
		require(false);
	}

	/*
	 * And put it back down. A worn object outlives the test that wore it, and
	 * the next test in this file measures an unencumbered Monk -- which is how
	 * it first failed, reporting a speed bug that was this armour.
	 */
	inven_takeoff(armour);
	p = as_class("Monk", 25);
	require(has_flag(p, OF_FREE_ACT));
	ok;
}

/**
 * The Monk's speed is level divided by ten, and stops there.
 *
 * Measured at four levels rather than asserted once, because `lev / 10` and
 * `lev / 10 + 1` differ at exactly one level in ten and a single sample would
 * not tell them apart.
 */
static int test_a_monk_speeds_up_by_a_tenth(void *state) {
	static const int levels[] = { 9, 10, 29, 50 };
	size_t i;

	for (i = 0; i < N_ELEMENTS(levels); i++) {
		struct player *p = as_class("Monk", levels[i]);
		int base = as_class("Warrior", levels[i])->state.speed;
		int monk;

		p = as_class("Monk", levels[i]);
		monk = p->state.speed;

		if (monk - base != levels[i] / 10) {
			printf("Monk at %d: %d over a Warrior's %d, wanted %d\n",
					levels[i], monk - base, base, levels[i] / 10);
			require(false);
		}
	}
	ok;
}

const char *suite_name = "player/class";
struct test tests[] = {
	{ "the-class-gates-open-on-time", test_the_class_gates_open_on_time },
	{ "a-chaos-warrior-grows-into-chaos",
			test_a_chaos_warrior_grows_into_chaos },
	{ "a-monks-gains-want-an-empty-pack",
			test_a_monks_gains_want_an_empty_pack },
	{ "a-monk-speeds-up-by-a-tenth", test_a_monk_speeds_up_by_a_tenth },
	{ NULL, NULL }
};
