/* player/race — what each race is, at birth and as it grows (PLR-01)
 *
 * A home for the race assertions, which until now were scattered through
 * `game/wild.c` — a 6,295-line wilderness suite that had accumulated
 * `test_every_race_is_playable` and `a-race-keeps-its-power` because it was
 * where somebody happened to be working. Wilderness is not where anyone looks
 * for a race regression, and a file that large is one nobody reads before
 * adding the hundred-and-first test. New race work belongs here; the two in
 * `wild.c` can move when something next touches them.
 *
 * The four races checked below were imported with their stats and skills but
 * only part of their flags, because Zangband grants an intrinsic in two
 * different places — `player_flags()` in `files.c` for most of them, and
 * `calc_bonuses()` in `xtra1.c` for the Golem's armour — and the import read
 * only the first. Each test names the archive line it is holding us to.
 */
#include "unit-test.h"

#include "init.h"
#include "player.h"
#include "player-birth.h"
#include "player-calcs.h"
#include "test-utils.h"
#include "cave.h"
#include "generate.h"
#include "effects.h"
#include "game-world.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "obj-make.h"
#include "obj-pile.h"
#include "player-timed.h"
#include "player-util.h"
#include "ui-input.h"

int setup_tests(void **state) {
	set_file_paths();
	init_angband();
	/* Any race and class will do; every test sets the race it cares about */
	if (!player_make_simple(NULL, NULL, "Tester")) return 1;
	*state = NULL;
	return 0;
}

int teardown_tests(void *state) {
	cleanup_angband();
	return 0;
}

static struct player_race *race_named(const char *name) {
	struct player_race *r = races;
	while (r && !streq(r->name, name)) r = r->next;
	return r;
}

/** Put the player at this level as this race, and recalculate. */
static struct player *grown_to(const char *name, int lev) {
	struct player_race *r = race_named(name);
	if (!r) return NULL;
	player->race = r;
	player->lev = lev;
	/*
	 * Underground on purpose. `calc_light()` returns early in town by day --
	 * the sun is doing the work -- so a race's own light is invisible to any
	 * test run at depth 0, and this asserted nothing until it moved down one.
	 */
	player->depth = 1;
	calc_bonuses(player, &player->state, false, true);
	return player;
}

/** Does the race hold this object flag once grown to `lev`? */
static bool has_at(const char *name, int lev, int flag) {
	struct player *p = grown_to(name, lev);
	bitflag f[OF_SIZE];
	if (!p) return false;
	player_flags(p, f);
	return of_has(f, flag);
}

/** Resistance level for an element once grown to `lev`. */
static int res_at(const char *name, int lev, int elem) {
	struct player *p = grown_to(name, lev);
	return p ? p->state.el_info[elem].res_level : 0;
}

/** Eat one ration as this race; return the nourishment actually gained. */
static int fed_as(const char *name) {
	struct object *obj;
	int before;

	if (!player_make_simple(name, NULL, "Tester")) return -1;
	/*
	 * A real level, because nourishment redraws the status line and that walks
	 * the cave. Without one this segfaults in `mark_wasseen()`.
	 */
	player->depth = 1;
	prepare_next_level(player);
	player_set_timed(player, TMD_FOOD, PY_FOOD_HUNGRY, false, false);
	before = player->timed[TMD_FOOD];

	obj = object_new();
	object_prep(obj, lookup_kind(TV_FOOD,
			lookup_sval(TV_FOOD, "Ration of Food")), 0, RANDOMISE);
	{
		bool ident = false;
		effect_do(obj->kind->effect, source_player(), obj, &ident, true, 0,
				0, 0, NULL);
	}
	object_delete(NULL, NULL, &obj);

	return player->timed[TMD_FOOD] - before;
}

/**
 * Stand this race on a lit grid at this depth, run one game turn, and report
 * the hit points lost.
 */
static int burned(const char *name, int depth, bool day) {
	int before;

	if (!player_make_simple(name, NULL, "Tester")) return -1;

	/* `is_daytime()` reads the global turn counter; set it before generating */
	turn = day ? 1 : (10L * z_info->day_length) / 2 + 1;
	if (is_daytime() != day) return -1;

	player->depth = depth;
	prepare_next_level(player);
	sqinfo_on(square(cave, player->grid)->info, SQUARE_GLOW);

	player->chp = player->mhp;
	before = player->chp;
	process_world(cave);

	return before - player->chp;
}

/*
 * A Draconian earns its resistances one at a time
 * ([files.c:1408](../archive/zangband/src/files.c#L1408)): fire above 4, cold
 * above 9, acid above 14, electricity above 19, poison above 34. All five were
 * missing — the race had the wings and none of the scales.
 *
 * Checked on both sides of every threshold, because a gate that is always open
 * passes a test that only looks above it.
 */
static int test_the_draconian_grows_into_its_scales(void *state) {
	static const struct { int lev, elem; } band[] = {
		{ 4,  ELEM_FIRE }, { 9,  ELEM_COLD }, { 14, ELEM_ACID },
		{ 19, ELEM_ELEC }, { 34, ELEM_POIS },
	};
	size_t i;

	for (i = 0; i < N_ELEMENTS(band); i++) {
		require(res_at("Draconian", band[i].lev, band[i].elem) == 0);
		require(res_at("Draconian", band[i].lev + 1, band[i].elem) == 1);
	}

	/* And the wings, which it has from the start */
	require(has_at("Draconian", 1, OF_FEATHER));
	ok;
}

/*
 * A Mindflayer sustains both its mental stats from birth, sees the invisible
 * above 14 and reads minds above 29
 * ([files.c:1421](../archive/zangband/src/files.c#L1421)).
 */
static int test_the_mindflayer_grows_into_its_mind(void *state) {
	require(has_at("Mindflayer", 1, OF_SUST_INT));
	require(has_at("Mindflayer", 1, OF_SUST_WIS));

	require(!has_at("Mindflayer", 14, OF_SEE_INVIS));
	require(has_at("Mindflayer", 15, OF_SEE_INVIS));

	require(!has_at("Mindflayer", 29, OF_TELEPATHY));
	require(has_at("Mindflayer", 30, OF_TELEPATHY));
	ok;
}

/*
 * A Golem is `20 + lev / 5` of armour
 * ([xtra1.c:2670](../archive/zangband/src/xtra1.c#L2670)) and holds its life
 * above 34 ([files.c:1434](../archive/zangband/src/files.c#L1434)).
 *
 * The armour was the largest single omission of the imported races — thirty
 * points at level 50 — and it was missed because it is the one intrinsic
 * Zangband grants outside `player_flags()`.
 */
static int test_the_golem_is_made_of_something(void *state) {
	struct player_race *golem = race_named("Golem");
	int with1, with50, without1, without50, keep, keep_scale;

	require(golem);
	require(grown_to("Golem", 1));
	with1 = player->state.to_a;
	require(grown_to("Golem", 50));
	with50 = player->state.to_a;

	/*
	 * The same character with the intrinsic switched off, which is the only
	 * honest baseline. Comparing a Golem against a Human instead measures the
	 * stat line as well -- the Golem's dexterity is a point of armour worse,
	 * so that comparison is off by one and says nothing about this mechanism.
	 */
	keep = golem->armour;
	keep_scale = golem->armour_scale;
	golem->armour = 0;
	golem->armour_scale = 0;
	require(grown_to("Golem", 1));
	without1 = player->state.to_a;
	require(grown_to("Golem", 50));
	without50 = player->state.to_a;
	golem->armour = keep;
	golem->armour_scale = keep_scale;

	require(with1 - without1 == 20);	/* 20 at birth */
	require(with50 - without50 == 30);	/* plus lev / 5 */

	require(!has_at("Golem", 34, OF_HOLD_LIFE));
	require(has_at("Golem", 35, OF_HOLD_LIFE));
	ok;
}

/*
 * A Vampire carries a light of its own
 * ([files.c:1469](../archive/zangband/src/files.c#L1469)) — which the import
 * missed, leaving the one race the sun burns with nothing to see by — and it
 * does *not* have `SLOW_DIGEST`, which the import invented. That invention was
 * the wrong sign twice over: a benefit where the archive gives a penalty, on
 * the race whose whole relationship with food is that food does not work.
 */
static int test_the_vampire_glows_and_starves(void *state) {
	struct player *p = grown_to("Vampire", 1);

	require(p);
	require(p->race->light == 1);
	require(p->state.cur_light >= 1);

	require(!has_at("Vampire", 1, OF_SLOW_DIGEST));
	require(player_has(p, PF_BLOOD_DIET));

	/* Immune to dark, vulnerable to light: the bargain */
	require(res_at("Vampire", 1, ELEM_DARK) == 3);
	require(res_at("Vampire", 1, ELEM_LIGHT) == -1);
	ok;
}

/*
 * Every level-gated intrinsic in the data actually reaches the player.
 *
 * The gates are new, and there are eight more races queued behind these four
 * that will use them. A gate that parses but is never applied would look
 * exactly like a correct import from the data file alone, so this walks what
 * was loaded rather than a list written out by hand: for each race, for each
 * `gain-at` band, the flags and resists it names must be absent the level
 * below and present the level above.
 */
static int test_every_level_gate_actually_opens(void *state) {
	struct player_race *r;
	int gates = 0;

	for (r = races; r; r = r->next) {
		struct player_race_gain *g;
		for (g = r->gains; g; g = g->next) {
			int i;
			int below = MAX(g->level - 1, 1);
			for (i = of_next(g->flags, FLAG_START); i != FLAG_END;
					i = of_next(g->flags, i + 1)) {
				require(has_at(r->name, g->level, i));
				if (below < g->level)
					require(!has_at(r->name, below, i));
				gates++;
			}
			for (i = 0; i < ELEM_MAX; i++) {
				if (!g->el_info[i].res_level) continue;
				require(res_at(r->name, g->level, i)
						== g->el_info[i].res_level);
				if (below < g->level)
					require(res_at(r->name, below, i) == 0);
				gates++;
			}
		}
	}

	/*
	 * If nothing was gated, the loop above proved nothing. Eight is what the
	 * four races carry today -- five Draconian resists, two Mindflayer flags,
	 * one Golem flag. Raise it as races arrive.
	 */
	require(gates >= 8);
	ok;
}

/*
 * A Vampire gets a tenth of a meal
 * ([cmd6.c:102](../archive/zangband/src/cmd6.c#L102)).
 *
 * The import gave it `SLOW_DIGEST` instead -- a benefit where Zangband imposes
 * a penalty, on the one race whose defining problem is that food does not work
 * for it. Measured against the same ration eaten by the same character as a
 * Human, so it cannot pass by the food simply being worth little.
 */
static int test_a_vampire_gets_little_from_food(void *state) {
	int as_human, as_vampire;

	as_human = fed_as("Human");
	as_vampire = fed_as("Vampire");

	require(as_human > 0);
	require(as_vampire > 0);			/* a tenth, not nothing */
	require(as_vampire == as_human / 10);
	ok;
}

/*
 * And it burns in the sun
 * ([dungeon.c:1014](../archive/zangband/src/dungeon.c#L1014)): a point a turn
 * on a lit town grid by day, and no regeneration while it lasts.
 *
 * Run four ways -- Vampire by day, Vampire by night, Vampire underground,
 * Human by day -- because a burn that fires everywhere is as wrong as one that
 * never fires, and only the first of those four should hurt.
 */
static int test_the_sun_burns_a_vampire_in_town(void *state) {
	require(burned("Vampire", 0, true) == 1);
	require(burned("Vampire", 0, false) == 0);	/* night */
	require(burned("Vampire", 1, true) == 0);	/* underground */
	require(burned("Human", 0, true) == 0);		/* not vulnerable */
	ok;
}

/*
 * The undead start the game just after midnight
 * ([dungeon.c:3270](../archive/zangband/src/dungeon.c#L3270)) -- which for the
 * race above is the difference between starting the game and starting it on
 * fire.
 */
static int test_the_undead_wake_in_the_dark(void *state) {
	require(player_make_simple("Vampire", NULL, "Tester"));
	require(!is_daytime());

	require(player_make_simple("Human", NULL, "Tester"));
	require(is_daytime());
	ok;
}

const char *suite_name = "player/race";
struct test tests[] = {
	{ "the-draconian-grows-into-its-scales",
			test_the_draconian_grows_into_its_scales },
	{ "the-mindflayer-grows-into-its-mind",
			test_the_mindflayer_grows_into_its_mind },
	{ "the-golem-is-made-of-something",
			test_the_golem_is_made_of_something },
	{ "the-vampire-glows-and-starves",
			test_the_vampire_glows_and_starves },
	{ "every-level-gate-actually-opens",
			test_every_level_gate_actually_opens },
	{ "a-vampire-gets-little-from-food",
			test_a_vampire_gets_little_from_food },
	{ "the-sun-burns-a-vampire-in-town",
			test_the_sun_burns_a_vampire_in_town },
	{ "the-undead-wake-in-the-dark",
			test_the_undead_wake_in_the_dark },
	{ NULL, NULL }
};
