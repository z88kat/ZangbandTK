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

#include "z-dice.h"

#include "init.h"
#include "player.h"
#include "player-birth.h"
#include "player-calcs.h"
#include "test-utils.h"
#include "cave.h"
#include "generate.h"
#include "cmd-core.h"
#include "player-util.h"
#include "message.h"
#include "project.h"
#include "effects.h"
#include "game-world.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "obj-make.h"
#include "obj-pile.h"
#include "player-timed.h"
#include "player-util.h"
#include "project.h"
#include "ui-input.h"

int setup_tests(void **state) {
	set_file_paths();
	init_angband();
	/*
	 * Report the seed, so an intermittent failure here can be replayed.
	 * Several tests in this suite generate a level, which makes them
	 * sensitive to the dice in ways the assertions do not show, and without
	 * this a one-in-twenty failure is only ever a number.
	 */
	(void) test_seed_rng_reported(suite_name);

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
static int burned_lit(const char *name, int depth, bool day, bool force_glow) {
	int before;

	if (!player_make_simple(name, NULL, "Tester")) return -1;

	/* `is_daytime()` reads the global turn counter; set it before generating */
	turn = day ? 1 : (10L * z_info->day_length) / 2 + 1;
	if (is_daytime() != day) return -1;

	player->depth = depth;
	prepare_next_level(player);
	if (force_glow)
		sqinfo_on(square(cave, player->grid)->info, SQUARE_GLOW);

	player->chp = player->mhp;
	before = player->chp;
	process_world(cave);

	return before - player->chp;
}

static int burned(const char *name, int depth, bool day) {
	return burned_lit(name, depth, day, true);
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
 * A Sprite flies, and flies faster as it grows (PLR-01).
 *
 * `lev / 10` ([xtra1.c:2550](../archive/zangband/src/xtra1.c#L2550)): one point
 * at 10, five at 50. It had none of this -- the largest single number missing
 * from the imported races after the Golem's armour, and worth more than most
 * races' whole list of resistances.
 *
 * The boundary is the point of the test. Zangband gates the flag above level 9
 * and then divides the level by ten, which is the same thing twice: a flat
 * bonus, or a scale of anything but ten, would pass a test that only looked at
 * level 50.
 */
static int test_a_sprite_flies_faster_as_it_grows(void *state) {
	struct player_race *sprite = race_named("Sprite");
	int at9, at10, at20, at50, without, keep;

	require(sprite);
	require(grown_to("Sprite", 9));  at9  = player->state.speed;
	require(grown_to("Sprite", 10)); at10 = player->state.speed;
	require(grown_to("Sprite", 20)); at20 = player->state.speed;
	require(grown_to("Sprite", 50)); at50 = player->state.speed;

	/* The same character with the scale switched off, as the only baseline
	 * that measures this and nothing else. */
	keep = sprite->speed_scale;
	sprite->speed_scale = 0;
	require(grown_to("Sprite", 50));
	without = player->state.speed;
	sprite->speed_scale = keep;

	eq(at50 - without, 5);		/* lev / 10 at 50 */
	eq(at10 - at9, 1);			/* and nothing at all until 10 */
	eq(at20 - at10, 1);
	eq(at9, at10 - 1);
	ok;
}

/*
 * And a Yeek stops merely resisting acid (PLR-01).
 *
 * Immune above 19 (`files.c`, RACE_YEEK) -- the one thing a Yeek is good at,
 * and it had the resistance without the immunity. Checked either side of the
 * threshold and against the resistance it starts with, so a version that
 * granted immunity at birth would fail as loudly as one that never granted it.
 */
static int test_a_yeek_becomes_immune_to_acid(void *state) {
	eq(res_at("Yeek", 1, ELEM_ACID), 1);
	eq(res_at("Yeek", 19, ELEM_ACID), 1);
	eq(res_at("Yeek", 20, ELEM_ACID), 3);
	eq(res_at("Yeek", 50, ELEM_ACID), 3);
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
	int lit, unlit;

	require(p);
	require(p->race->light == 1);

	/*
	 * The *difference* a Vampire makes, not the total.
	 *
	 * This asserted `cur_light >= 1`, which the Wooden Torch in the starting
	 * kit satisfies whether or not the race contributes anything: deleting the
	 * line in `calc_bonuses()` that applies `race->light` broke no test at all.
	 * `grown_to()` swaps the race and recalculates without touching the gear,
	 * so two calls differ by exactly the race's own light and nothing else.
	 */
	lit = p->state.cur_light;
	unlit = grown_to("Human", 1)->state.cur_light;
	eq(lit - unlit, 1);

	p = grown_to("Vampire", 1);
	require(p);

	require(!has_at("Vampire", 1, OF_SLOW_DIGEST));
	require(player_has(p, PF_BLOOD_DIET));

	/* Immune to dark, vulnerable to light: the bargain */
	require(res_at("Vampire", 1, ELEM_DARK) == 3);
	require(res_at("Vampire", 1, ELEM_LIGHT) == -1);
	ok;
}

/*
 * The four near-data races of the second wave (PLR-01).
 *
 * Barbarian, Klackon, Nibelung and Imp: one gate between them, one innate
 * speed, and the rest flags. Checked because "it parsed" is not the same as
 * "it reaches the player" -- every one of these goes through a different path
 * (`PROT_FEAR` and `PROT_CONF` are object flags, the resists are `el_info`,
 * the Imp's see-invisible is a gate and the Klackon's speed is a field).
 */
static int test_the_second_wave_of_races(void *state) {
	int with_speed, without, keep;
	struct player_race *klackon = race_named("Klackon");

	require(has_at("Barbarian", 1, OF_PROT_FEAR));

	require(klackon);
	require(has_at("Klackon", 1, OF_PROT_CONF));
	eq(res_at("Klackon", 1, ELEM_ACID), 1);
	require(grown_to("Klackon", 50)); with_speed = player->state.speed;
	keep = klackon->speed_scale;
	klackon->speed_scale = 0;
	require(grown_to("Klackon", 50)); without = player->state.speed;
	klackon->speed_scale = keep;
	eq(with_speed - without, 5);		/* lev / 10, as the Sprite */

	eq(res_at("Nibelung", 1, ELEM_DISEN), 1);
	eq(res_at("Nibelung", 1, ELEM_DARK), 1);

	eq(res_at("Imp", 1, ELEM_FIRE), 1);
	require(!has_at("Imp", 9, OF_SEE_INVIS));
	require(has_at("Imp", 10, OF_SEE_INVIS));

	/*
	 * And the Golem cannot be stunned (effects.c:1875) -- one of the three
	 * grants the archive makes outside `player_flags()`, which is why the
	 * first sweep missed it.
	 */
	require(has_at("Golem", 1, OF_PROT_STUN));
	ok;
}

/*
 * The four undead, which share one shape (PLR-01).
 *
 * Skeleton, Zombie, Spectre and Ghoul: none of them can eat, all of them wake
 * just after midnight, and each takes scrolls of Remove Hunger in place of the
 * rations it cannot use. Checked as a group because the point of them is that
 * they are a group -- a fifth undead race should be a data block and nothing
 * else, and if the pattern has drifted this is where it shows.
 *
 * Note the Vampire is *not* here: it eats badly rather than not at all
 * (BLOOD_DIET), and it is the only one of the five the light hurts.
 */
static int test_the_undead_share_one_shape(void *state) {
	static const char *undead[] = { "Skeleton", "Zombie", "Spectre", "Ghoul" };
	size_t i;

	for (i = 0; i < N_ELEMENTS(undead); i++) {
		struct player *p = grown_to(undead[i], 1);

		require(p);
		require(has_at(undead[i], 1, OF_CANT_EAT));
		require(player_has(p, PF_UNDEAD));
		require(has_at(undead[i], 1, OF_HOLD_LIFE));
		eq(res_at(undead[i], 1, ELEM_POIS), 3);	/* immune, all four */
	}

	/* Each has a vulnerability, and they are not the same one */
	eq(res_at("Skeleton", 1, ELEM_ACID), -1);
	eq(res_at("Zombie", 1, ELEM_FIRE), -1);
	eq(res_at("Spectre", 1, ELEM_ELEC), -1);
	eq(res_at("Ghoul", 1, ELEM_FIRE), -1);

	/* Their gates, which differ */
	eq(res_at("Zombie", 4, ELEM_COLD), 0);
	eq(res_at("Zombie", 5, ELEM_COLD), 1);
	eq(res_at("Skeleton", 9, ELEM_COLD), 0);
	eq(res_at("Skeleton", 10, ELEM_COLD), 1);
	require(!has_at("Spectre", 34, OF_TELEPATHY));
	require(has_at("Spectre", 35, OF_TELEPATHY));
	eq(res_at("Ghoul", 10, ELEM_DARK), 1);
	eq(res_at("Ghoul", 20, ELEM_NETHER), 1);

	/* And the Ghoul's touch, which arrives as a flag it already had a home for */
	require(has_at("Ghoul", 1, OF_GHOUL_TOUCH));
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
		struct player_gain *g;
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
				/*
				 * Below the band it must be *less*, not absent. The Draconian
				 * gains resistances it did not have; the Yeek upgrades one it
				 * did -- acid 1 at birth, 3 above 19 -- and demanding zero
				 * there fails a race that is correct.
				 */
				if (below < g->level)
					require(res_at(r->name, below, i)
							< g->el_info[i].res_level);
				gates++;
			}
		}
	}

	/*
	 * If nothing was gated, the loop above proved nothing. Fifteen is what the
	 * races carry today: five Draconian resists, two Mindflayer flags, one
	 * Golem flag, the Yeek's acid immunity, the Imp's see-invisible, a cold
	 * resistance each for Skeleton and Zombie, the Spectre's telepathy and the
	 * Ghoul's two. Raise it as races arrive.
	 */
	require(gates >= 15);
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
 * Run six ways -- Vampire by day, by night and underground, a Human by day, and
 * the first two again without lighting the grid by hand -- because a burn that
 * fires everywhere is as wrong as one that never fires, and only daylight on
 * the surface should hurt.
 */
static int test_the_sun_burns_a_vampire_outdoors(void *state) {
	require(burned("Vampire", 0, true) == 1);
	require(burned("Vampire", 0, false) == 0);	/* night */
	require(burned("Vampire", 1, true) == 0);	/* underground */
	require(burned("Human", 0, true) == 0);		/* not vulnerable */

	/*
	 * And without anybody lighting the grid by hand. Dawn calls
	 * `cave_illuminate()` over the whole surface, wilderness included
	 * (`game-world.c:670`), so a Vampire outdoors by day is burning wherever it
	 * stands -- not only on a town street. The manual says so, so it is
	 * checked rather than reasoned about.
	 */
	require(burned_lit("Vampire", 0, true, false) == 1);
	require(burned_lit("Vampire", 0, false, false) == 0);
	ok;
}

/**
 * Read a scroll of Darkness where the player stands; is the grid dark after?
 */
static bool darkness_takes_from(const char *name, int depth, bool day,
		bool by_monster) {
	struct object *obj;
	struct source origin;
	bool dark;

	if (!player_make_simple(name, NULL, "Tester")) return false;

	turn = day ? 1 : (10L * z_info->day_length) / 2 + 1;
	player->depth = depth;
	prepare_next_level(player);
	sqinfo_on(square(cave, player->grid)->info, SQUARE_GLOW);

	if (by_monster) {
		struct monster *mon = NULL;
		int d;

		/*
		 * Whichever neighbouring grid is actually free. Assuming the one to
		 * the east is not safe -- `prepare_next_level()` can put the player
		 * against a wall, and `t_add_monster()` asserts rather than returning
		 * null, so the wrong guess is a crash on some seeds and not others.
		 */
		for (d = 0; d < 8 && !mon; d++) {
			struct loc g = loc_sum(player->grid, ddgrid_ddd[d]);
			if (square_in_bounds_fully(cave, g) && square_isempty(cave, g))
				mon = t_add_monster(cave, g, "grid bug");
		}
		if (!mon) return false;
		origin = source_monster(mon->midx);
	} else {
		origin = source_player();
	}

	obj = object_new();
	object_prep(obj, lookup_kind(TV_SCROLL,
			lookup_sval(TV_SCROLL, "Darkness")), 0, RANDOMISE);
	{
		bool ident = false;
		effect_do(obj->kind->effect, origin, obj, &ident, true, 0, 0, 0, NULL);
	}
	object_delete(NULL, NULL, &obj);

	dark = !square_isglow(cave, player->grid);
	return dark;
}

static bool darkness_takes(const char *name, int depth, bool day) {
	return darkness_takes_from(name, depth, day, false);
}

/*
 * A Vampire can put the sun out where it stands (DEC-73).
 *
 * Stock 4.2 refuses to darken the surface in daylight, which is right for a
 * game where nobody minds the sun and wrong for the one race that burns in it.
 * Zangband's darkening had no such guard, and the two to five scrolls a Vampire
 * starts with were its shelter.
 *
 * Checked from both sides: the Vampire can do it, an ordinary character still
 * cannot, and underground -- where the guard never applied -- both can. If the
 * relaxation had been written as "always allow", the Human-by-day case would
 * catch it.
 */
static int test_a_vampire_can_put_out_the_daylight(void *state) {
	require(darkness_takes("Vampire", 0, true));
	require(!darkness_takes("Human", 0, true));

	/* Night and depth were never guarded, and still are not */
	require(darkness_takes("Human", 0, false));
	require(darkness_takes("Human", 1, true));

	/*
	 * And it is the caster's vulnerability that counts, not the player's.
	 * Without that, a monster casting darkness anywhere near a Vampire could
	 * put out the daylight on its behalf -- the relaxation is meant to be a
	 * thing the Vampire does, not a thing that happens around it.
	 */
	require(!darkness_takes_from("Vampire", 0, true, true));
	ok;
}

/*
 * And doing so actually stops the burning, which is the whole point.
 *
 * The two halves are separately plausible and separately useless: a scroll that
 * darkens a grid the sun check does not read, or a sun check reading a grid
 * nothing can darken. This runs the burn, reads the scroll, runs it again.
 */
static int test_darkness_is_shelter_from_the_sun(void *state) {
	int before, after;

	require(player_make_simple("Vampire", NULL, "Tester"));
	turn = 1;
	player->depth = 0;
	prepare_next_level(player);
	sqinfo_on(square(cave, player->grid)->info, SQUARE_GLOW);

	player->chp = player->mhp;
	before = player->chp;
	process_world(cave);
	require(before - player->chp == 1);

	{
		struct object *obj = object_new();
		bool ident = false;
		object_prep(obj, lookup_kind(TV_SCROLL,
				lookup_sval(TV_SCROLL, "Darkness")), 0, RANDOMISE);
		effect_do(obj->kind->effect, source_player(), obj, &ident, true, 0,
				0, 0, NULL);
		object_delete(NULL, NULL, &obj);
	}

	player->chp = player->mhp;
	after = player->chp;
	process_world(cave);
	eq(after - player->chp, 0);
	ok;
}

/**
 * Stand this race in a level and try to step onto `feat`; did it move?
 */
static bool steps_into(const char *name, const char *feat_name)
{
	struct loc target;
	int feat = lookup_feat_code(feat_name);
	int d;

	/*
	 * A code that does not exist is a failing test, not a passing one.
	 *
	 * `lookup_feat_code()` returns -1 for an unknown name and
	 * `square_set_feat()` indexes `f_info` with it, which ASAN reports as a
	 * heap-buffer-overflow -- but without ASAN it quietly scribbles and the
	 * move still succeeds, so the assertion passes having tested nothing.
	 * That is what happened here: mountainside's code is `ROCK`, not
	 * `MOUNTAIN`, so "a Spectre walks through a mountain" passed for three
	 * hours without once involving a mountain.
	 */
	if (feat < 0) return false;

	if (!player_make_simple(name, NULL, "Tester")) return false;
	player->depth = 1;
	prepare_next_level(player);

	/*
	 * Whichever neighbour is actually there, and the direction that reaches
	 * it, found together.
	 *
	 * This used to try east and fall back to west without re-checking the
	 * fallback, which walks off the array whenever the player is placed on
	 * the top row -- `square_in_bounds_fully()` excludes the outer ring, so
	 * east fails, and west of x=0 is x=-1. ASAN called it a
	 * heap-buffer-overflow in `square_set_feat()`, which is exactly what it
	 * was.
	 */
	for (d = 0; d < 8; d++) {
		target = loc_sum(player->grid, ddgrid_ddd[d]);
		if (!square_in_bounds_fully(cave, target)) continue;

		/*
		 * And nothing standing on it. `move_player()` into an occupied grid
		 * attacks rather than moves, so the walk silently does not happen and
		 * the test reads that as a Spectre unable to pass through rock.
		 *
		 * Measured before it was fixed: about one run in twenty, which is how
		 * often level generation puts a monster next to the player. It had
		 * been in the suite for two days and `check-flakes` -- eight passes --
		 * misses a one-in-twenty failure about two times in three.
		 */
		if (square_monster(cave, target)) continue;
		break;
	}
	if (d == 8) return false;

	square_set_feat(cave, target, feat);
	move_player(ddd[d], false);
	return loc_eq(player->grid, target);
}

/*
 * A Spectre walks through rock, and only through rock that gives (DEC-74).
 *
 * The project owner's ruling was a consistency argument -- "if he can walk
 * through walls he can walk through anything" -- and it lands close to the
 * archive anyway, whose mountains were passable to everyone in the first place
 * ([cmd1.c:2382](../archive/zangband/src/cmd1.c#L2382)).
 *
 * The two that must still hold are the point of the test. Permanent wall is
 * what the dungeon is built out of at its edges, and the world's edge is
 * `PERMANENT` in `terrain.txt` for exactly this reason -- a Spectre that could
 * step off the edge of the world would be a much worse bug than one that
 * cannot cross a mountain.
 */
static int test_a_spectre_walks_through_rock(void *state) {
	require(steps_into("Spectre", "GRANITE"));
	require(steps_into("Spectre", "MAGMA"));
	require(steps_into("Spectre", "ROCK"));	/* mountainside */

	require(!steps_into("Spectre", "PERM"));
	require(!steps_into("Spectre", "WORLD_EDGE"));

	/* And nobody else walks through any of it */
	require(!steps_into("Human", "GRANITE"));
	require(!steps_into("Human", "ROCK"));
	ok;
}

/*
 * And standing in rock hurts without killing (DEC-74).
 *
 * Zangband applies the damage only while `chp > depth / 10`
 * ([dungeon.c:1202](../archive/zangband/src/dungeon.c#L1202)), which is what
 * stops a Spectre suffocating inside a mountain it walked into. Checked by
 * running the world until it stops taking damage and confirming it is alive --
 * a version that simply dealt damage every turn would fail here and nowhere
 * else.
 */
static int test_rock_grinds_a_spectre_but_does_not_kill_it(void *state) {
	int i, before;

	require(steps_into("Spectre", "GRANITE"));
	require(player->depth > 0);
	require(!square_ispassable(cave, player->grid));

	player->chp = player->mhp;
	before = player->chp;
	process_world(cave);
	require(player->chp < before);		/* it hurts */

	/*
	 * Now from one hit point, which is the case that matters and the one a
	 * clamp would pass without doing anything. A guard that never fires looks
	 * exactly like a guard that works when the character starts healthy.
	 */
	player->chp = 1;
	for (i = 0; i < 500 && !player->is_dead; i++)
		process_world(cave);
	require(!player->is_dead);
	require(player->chp >= 0);
	ok;
}

/*
 * And a mountain costs a Spectre blood, the same as any other wall (DEC-74).
 *
 * The project owner overruled the archive-faithful reading here: *"Our
 * mountains are walls so the Spector takes damage."* Zangband's mountains were
 * not `FF_BLOCK` grids, so a Spectre there was never inside a wall and paid
 * nothing for a crossing; ours are walls, and the rule follows our world
 * rather than the original's.
 *
 * Measured before it was asserted, because the formula at depth 0 is not
 * obviously survivable: one point a turn, and the guard `chp > depth / 10`
 * becomes `chp > 0`, which is its tightest form. It holds -- death is
 * `chp < 0` -- so a Spectre pins at nothing left and stays there rather than
 * dying. Checked at both ends, because a guard that only works underground
 * would look exactly like one that works.
 */
static int test_a_mountain_costs_a_spectre_blood(void *state) {
	int before, i;

	require(player_make_simple("Spectre", NULL, "Tester"));
	player->depth = 0;
	prepare_next_level(player);
	square_set_feat(cave, player->grid, lookup_feat_code("ROCK"));
	require(!square_ispassable(cave, player->grid));

	/* A point a turn on the surface, where the depth term is zero */
	player->chp = player->mhp;
	before = player->chp;
	process_world(cave);
	eq(before - player->chp, 1);

	/*
	 * And it will not kill, which on the surface is the case that matters:
	 * a range is many blocks wide, so a crossing spends far more turns in
	 * rock than a level-one Spectre has hit points.
	 */
	player->chp = 1;
	for (i = 0; i < 1000 && !player->is_dead; i++)
		process_world(cave);
	require(!player->is_dead);
	require(player->chp >= 0);
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

/**
 * Every race has a body of its own.
 *
 * Nine imported races shared one placeholder -- `age:20:20`, `height:70:6`,
 * `weight:150:20` -- so a Sprite and a Half-Titan were born the same size, and
 * a Golem weighed what a man weighs. The figures exist in Zangband's own race
 * table; nothing had to be invented, only merged, because Zangband keeps a
 * pair per race for the two sexes and 4.2 keeps one (DEC-76).
 *
 * Two assertions, and the first is the one that matters: no two races may
 * share the placeholder triple, which is what a future import silently
 * inheriting it would look like. The second names four races whose size is
 * part of what they are, so a merge done wrongly in the other direction --
 * everything converging on the average -- fails as well.
 */
static int test_every_race_has_a_body(void *state) {
	static const struct {
		const char *race;
		int age, height, weight;
	} rows[] = {
		{ "Sprite",     50,  30,  70 },
		{ "Half-Titan", 100, 105, 252 },
		{ "Yeek",       14,  50,  82 },
		{ "Golem",      1,   64,  190 },
	};
	struct player_race *r;
	size_t i;

	/* Nobody is left on the placeholder. */
	for (r = races; r; r = r->next) {
		if (r->b_age == 20 && r->m_age == 20 && r->base_hgt == 70
				&& r->mod_hgt == 6 && r->base_wgt == 150
				&& r->mod_wgt == 20) {
			printf("%s still carries the placeholder body\n", r->name);
			require(false);
		}
	}

	for (i = 0; i < N_ELEMENTS(rows); i++) {
		r = race_named(rows[i].race);
		require(r);
		if (r->b_age != rows[i].age || r->base_hgt != rows[i].height
				|| r->base_wgt != rows[i].weight) {
			printf("%s: %d/%d/%d, wanted %d/%d/%d\n", rows[i].race,
					r->b_age, r->base_hgt, r->base_wgt,
					rows[i].age, rows[i].height, rows[i].weight);
			require(false);
		}
	}
	ok;
}

/** The named power of that race, or NULL. */
static struct player_power *power_of(const char *race, const char *power)
{
	struct player_race *r = race_named(race);
	struct player_power *pw;

	if (!r) return NULL;
	for (pw = r->powers; pw; pw = pw->next)
		if (streq(pw->name, power)) return pw;
	return NULL;
}

/**
 * The six races Angband and Zangband share kept their racial powers.
 *
 * A Dwarf, Hobbit, Gnome, Half-Orc, Half-Troll and Kobold each have one in
 * Zangband (racial.c, with the level, cost, stat and failure from
 * tables.c:7752) and had none here. The import took the eleven shared races'
 * stats and skills and stopped: a Dwarf arrived as Angband's Dwarf, which is
 * the same race minus the one button it can press.
 *
 * The figures are the archive's, so this pins them rather than arguing them --
 * an edit to p_race.txt that reprices one is a decision somebody makes.
 */
static int test_the_shared_races_kept_their_powers(void *state) {
	static const struct {
		const char *race, *power;
		int level, cost, stat, fail;
	} rows[] = {
		{ "Dwarf",      "examine your surroundings",  5,  5,  STAT_WIS, 12 },
		{ "Hobbit",     "cook some food",            15, 10,  STAT_INT, 10 },
		{ "Gnome",      "blink",                      5, 10,  STAT_INT, 12 },
		{ "Half-Orc",   "play tough",                 3,  5,  STAT_WIS,  8 },
		{ "Half-Troll", "work yourself into a frenzy", 10, 12, STAT_WIS, 9 },
		{ "Kobold",     "throw a dart of poison",    12,  8,  STAT_DEX, 14 },
	};
	size_t i;

	for (i = 0; i < N_ELEMENTS(rows); i++) {
		struct player_power *pw = power_of(rows[i].race, rows[i].power);

		if (!pw) {
			printf("%s has no power '%s'\n", rows[i].race, rows[i].power);
			require(false);
		}
		if (pw->level != rows[i].level || pw->cost != rows[i].cost
				|| pw->stat != rows[i].stat || pw->fail != rows[i].fail) {
			printf("%s: %d/%d/%d/%d, wanted %d/%d/%d/%d\n", rows[i].race,
					pw->level, pw->cost, pw->stat, pw->fail,
					rows[i].level, rows[i].cost, rows[i].stat, rows[i].fail);
			require(false);
		}
	}
	ok;
}

/**
 * The two powers whose strength is written as an expression give the archive's
 * numbers, measured rather than read.
 *
 * `power-dice` goes through the dice grammar, and DEC-75 is the reason this
 * test exists: the same text means different things to different parsers here,
 * and `values:[-d5M5]` looked like a penalty and granted a bonus. So the
 * Gnome's `10+$B` and the Kobold's `$B` are evaluated at two character levels
 * and compared against `10 + plev` and `plev`, which is what Zangband passes
 * to `teleport_player` and `fire_bolt` (racial.c:236, 357).
 */
static int test_a_power_expression_means_what_it_says(void *state) {
	struct player_power *blink = power_of("Gnome", "blink");
	struct player_power *dart = power_of("Kobold", "throw a dart of poison");
	static const int levels[] = { 5, 50 };
	size_t i;

	require(blink && blink->effects && blink->effects->effect);
	require(dart && dart->effects && dart->effects->effect);

	/*
	 * And the form that was already shipping, because it is the same grammar
	 * and the Gnome's near-identical `10+$B` proved to mean something else
	 * entirely. A Half-Troll's frenzy is `inc_shero(10 + randint1(plev))`,
	 * so the duration must span 11..10+level and no wider.
	 */
	{
		struct player_power *rage = power_of("Half-Troll",
				"work yourself into a frenzy");
		struct power_effect *pe;
		struct effect *shero = NULL;

		require(rage);
		for (pe = rage->effects; pe; pe = pe->next) {
			struct effect *e;

			for (e = pe->effect; e; e = e->next)
				if (e->index == EF_TIMED_INC) shero = e;
		}
		require(shero && shero->dice);
		player->lev = 40;
		eq(dice_evaluate(shero->dice, 40, MINIMISE, NULL), 11);
		eq(dice_evaluate(shero->dice, 40, MAXIMISE, NULL), 50);
	}

	for (i = 0; i < N_ELEMENTS(levels); i++) {
		int lev = levels[i];
		int range, damage;

		player->lev = lev;
		range = dice_evaluate(blink->effects->effect->dice, lev, AVERAGE,
				NULL);
		damage = dice_evaluate(dart->effects->effect->dice, lev, AVERAGE,
				NULL);

		if (range != 10 + lev || damage != lev) {
			printf("at level %d: blink %d (wanted %d), dart %d (wanted %d)\n",
					lev, range, 10 + lev, damage, lev);
			require(false);
		}
	}
	ok;
}

/**
 * Four races do not bleed, and one of them has to grow into it.
 *
 * `set_cut()` zeroes the value outright for a Golem, Skeleton, Spectre, and a
 * Zombie above level 11 ([effects.c:2064](../../archive/zangband/src/effects.c#L2064)).
 * 4.2 had protection from fear, blindness, confusion and stunning and not from
 * bleeding, so the races that needed it had nowhere to say so and simply bled.
 *
 * The Zombie is the interesting row. Its threshold is the reason this could not
 * be four `obj-flags:` entries: it is a level gate on a property the other
 * three are born with, so it needs both mechanisms at once.
 *
 * Tested through `player_inc_timed` rather than by reading the flag, because
 * the flag only matters if the timed effect honours it -- the whole thing hangs
 * on one `fail:1:PROT_CUT` line in player_timed.txt, and a test that read the
 * flag would pass with that line deleted.
 */
static int test_the_bloodless_do_not_bleed(void *state) {
	static const struct { const char *race; int lev; bool bleeds; } rows[] = {
		{ "Golem",    1,  false },
		{ "Skeleton", 1,  false },
		{ "Spectre",  1,  false },
		{ "Zombie",   11, true  },	/* below the gate, it still bleeds */
		{ "Zombie",   12, false },
		{ "Human",    1,  true  },	/* and the control */
	};
	size_t i;

	/*
	 * A real level, because `player_inc_check()` reads `cave->mon_current` to
	 * decide whether a monster is watching, and does it before asking whether
	 * there is a cave at all. Nothing in the game reaches that with no level;
	 * a test does.
	 */
	for (i = 0; i < N_ELEMENTS(rows); i++) {
		struct player *p = grown_to(rows[i].race, rows[i].lev);
		bool bled;

		require(p);
		p->depth = 1;
		prepare_next_level(p);
		p->timed[TMD_CUT] = 0;
		player_inc_timed(p, TMD_CUT, 50, false, false, true);
		bled = p->timed[TMD_CUT] > 0;

		/*
		 * And through a real source rather than the setter, because three of
		 * the four places the game cuts you passed `check = false` and so went
		 * round the whole mechanism. A character's own exertion is one of
		 * them: the flag was right and a Skeleton still bled from casting.
		 */
		p->timed[TMD_CUT] = 0;
		player_over_exert(p, PY_EXERT_CUT, 100, 50);
		if ((p->timed[TMD_CUT] > 0) != rows[i].bleeds) {
			printf("%s at %d bled from exertion: %s, wanted %s\n",
					rows[i].race, rows[i].lev,
					p->timed[TMD_CUT] > 0 ? "yes" : "no",
					rows[i].bleeds ? "yes" : "no");
			require(false);
		}

		if (bled != rows[i].bleeds) {
			printf("%s at %d: %s, wanted %s\n", rows[i].race, rows[i].lev,
					bled ? "bled" : "did not bleed",
					rows[i].bleeds ? "bleeding" : "no bleeding");
			require(false);
		}
	}
	ok;
}

/**
 * A Draconian breathes fire or cold, for twice its level.
 *
 * Both halves were wrong. The element was fire only, where the archive rolls
 * `one_in_(3) ? GF_COLD : GF_FIRE` on every breath
 * ([racial.c:381](../../archive/zangband/src/racial.c#L381)); and the damage
 * was `plev * 3 / 2` against the archive's `plev * 2`
 * ([racial.c:481](../../archive/zangband/src/racial.c#L481)) -- three-quarters,
 * at every level, for the life of the character.
 *
 * The three branches are asserted by element and by damage together, because
 * a RANDOM chain with the right elements and the wrong dice looks correct in
 * the data file.
 *
 * And then the class takes it over (DEC-87). From level 15 the archive rolls
 * `randint1(100) < plev` and, on success, swaps the element for a pair
 * belonging to the character's class. Every band below is one of those pairs;
 * the last, with a chance of 101, is the fire and cold everyone else gets.
 */

/** What the archive's class table says, read off `racial.c:383`. */
struct breath_group {
	const char *classes;	/* exactly as the data spells it */
	const char *a;			/* the element named once */
	const char *b;			/* and the one named twice */
};

static const struct breath_group breath_table[] = {
	{ "Warrior|Ranger",					"MISSILE",	"SHARD" },
	{ "Mage|Warrior-Mage|High-Mage",	"MANA",		"DISEN" },
	{ "Chaos-Warrior",					"CHAOS",	"CONFUSION" },
	{ "Monk",							"SOUND",	"CONFUSION" },
	{ "Mindcrafter",					"MON_PSI",	"CONFUSION" },
	{ "Priest|Paladin",					"HOLY_ORB",	"HOLY_FIRE" },
	{ "Rogue",							"DARK",		"POIS" },
};

/**
 * The Draconian's breath is the archive's class table, pair by pair.
 *
 * The multiplicities are the point and are read off the source rather than
 * assumed: the archive writes `one_in_(3)` for some branches and `!one_in_(3)`
 * for others, so a Warrior breathes shards twice as often as the elements while
 * a Chaos-Warrior breathes confusion twice as often as chaos. A port that made
 * every pair an even split would look right and play wrong.
 */
static int test_a_draconian_breathes_by_its_class(void *state) {
	struct player_power *pw = power_of("Draconian", "breathe like a dragon");
	struct power_effect *pe;
	size_t g = 0;
	int bands = 0;

	require(pw && pw->effects);

	for (pe = pw->effects; pe; pe = pe->next) {
		struct effect *e = pe->effect;
		int seen_a = 0, seen_b = 0, other = 0;
		const char *a, *b;

		bands++;
		notnull(e);

		/* Every band is a RANDOM over exactly three breaths. */
		require(e->index == EF_RANDOM);
		require(dice_evaluate(e->dice, 1, AVERAGE, NULL) == 3);

		if (g < N_ELEMENTS(breath_table)) {
			notnull(pe->classes);
			require(streq(pe->classes, breath_table[g].classes));
			a = breath_table[g].a;
			b = breath_table[g].b;
		} else {
			/* The fallback: no class, and it always wins */
			null(pe->classes);
			a = "FIRE";			/* named twice here, see below */
			b = "COLD";
		}
		notnull(pe->chance);

		for (e = e->next; e; e = e->next) {
			int idx;

			require(e->index == EF_BREATH);

			/* Twice your level, in every branch of every band */
			player->lev = 30;
			if (dice_evaluate(e->dice, 30, AVERAGE, NULL) != 60) {
				printf("%s branch does %d at level 30, wanted 60\n",
					   pe->classes ? pe->classes : "fallback",
					   dice_evaluate(e->dice, 30, AVERAGE, NULL));
				require(false);
			}

			idx = proj_name_to_idx(a);
			if (e->subtype == idx) seen_a++;
			else if (e->subtype == proj_name_to_idx(b)) seen_b++;
			else other++;
		}

		eq(other, 0);
		if (g < N_ELEMENTS(breath_table)) {
			eq(seen_a, 1);
			eq(seen_b, 2);
		} else {
			eq(seen_a, 2);		/* fire twice */
			eq(seen_b, 1);		/* cold once */
		}
		g++;
	}

	/* Seven class pairs and the fallback, and nothing else */
	eq(bands, (int) N_ELEMENTS(breath_table) + 1);
	ok;
}

/**
 * Every class named in the data is a class that exists.
 *
 * `power-when-class` is matched by name at use, because races are parsed before
 * classes and there is nothing to resolve against at parse time. A misspelled
 * name is therefore not a parse error -- it is a band that silently never
 * fires, which is the exact shape of defect this project keeps finding months
 * later. This is the guard, and it covers every race rather than the Draconian.
 */
static int test_every_class_named_by_a_race_exists(void *state) {
	struct player_race *r;
	int names = 0;

	for (r = races; r; r = r->next) {
		struct player_power *pw;

		for (pw = r->powers; pw; pw = pw->next) {
			struct power_effect *pe;

			for (pe = pw->effects; pe; pe = pe->next) {
				char buf[128];
				char *at;

				if (!pe->classes) continue;
				my_strcpy(buf, pe->classes, sizeof(buf));

				for (at = strtok(buf, "|"); at; at = strtok(NULL, "|")) {
					struct player_class *c;
					bool found = false;

					for (c = classes; c; c = c->next)
						if (streq(c->name, at)) found = true;

					if (!found) {
						printf("%s's \"%s\" names class \"%s\", "
							   "which does not exist\n", r->name, pw->name,
							   at);
						require(false);
					}
					names++;
				}
			}
		}
	}

	/* And the mechanism is actually in use, or this has tested nothing */
	require(names >= 10);
	ok;
}

/**
 * A class the archive never had breathes fire and cold, and that is ours.
 *
 * Zangband has eleven classes and its switch covers all eleven, so it has no
 * `default:` -- the fallthrough here is not a rule the archive ever exercised.
 * The Druid, the Necromancer and the Blackguard are 4.2's own, and they get the
 * base roll, which is what the archive's code would do with them. Asserted
 * because it is a judgement rather than a lookup and should not drift silently.
 */
static int test_a_class_the_archive_lacks_falls_back(void *state) {
	static const char *const ours[] = { "Druid", "Necromancer", "Blackguard" };
	struct player_power *pw = power_of("Draconian", "breathe like a dragon");
	size_t i;

	notnull(pw);

	for (i = 0; i < N_ELEMENTS(ours); i++) {
		struct power_effect *pe;

		require(player_make_simple("Draconian", ours[i], "Tester"));
		player->lev = 50;

		for (pe = pw->effects; pe; pe = pe->next) {
			bool applies = power_band_applies(player, pe);

			/* Only the fallback may admit them */
			if (pe->classes) require(!applies);
			else require(applies);
		}
	}

	/* And a class the archive does cover is admitted by its own band */
	require(player_make_simple("Draconian", "Priest", "Tester"));
	player->lev = 50;
	{
		struct power_effect *pe;
		int admitted = 0;

		for (pe = pw->effects; pe; pe = pe->next)
			if (power_band_applies(player, pe)) admitted++;

		/* Its own pair, and the fallback beneath it */
		eq(admitted, 2);
	}
	ok;
}

/**
 * Chance bands are alternatives: one fires, never both (DEC-87).
 *
 * Built by hand rather than through the Draconian, because this is a mechanism
 * and not a Draconian behaviour -- the next race to want it should be able to
 * rely on the rule rather than on this one power's data.
 *
 * Two alternatives and one unconditional band. Over many uses the first must
 * win about half the time, the second the rest, *never both*, and the
 * unconditional one must fire every single time. A build where chance bands
 * were independent would fire both together about a quarter of the time.
 */
static int test_chance_bands_are_alternatives(void *state) {
	struct player_power pw;
	struct power_effect first, second, always;
	struct effect e_first, e_second, e_always;
	int i, both = 0, neither = 0, saw_first = 0, saw_second = 0;
	int fizzled = 0;

	require(player_make_simple("Human", "Warrior", "Tester"));
	player->depth = 1;
	prepare_next_level(player);		/* using a power reaches into `cave` */
	player->lev = 50;
	player->upkeep->update |= (PU_BONUS | PU_HP | PU_SPELLS);
	update_stuff(player);
	player->csp = player->msp;
	player->mhp = 5000;
	player->chp = 5000;

	memset(&pw, 0, sizeof(pw));
	memset(&first, 0, sizeof(first));
	memset(&second, 0, sizeof(second));
	memset(&always, 0, sizeof(always));
	memset(&e_first, 0, sizeof(e_first));
	memset(&e_second, 0, sizeof(e_second));
	memset(&e_always, 0, sizeof(e_always));

	e_first.index = EF_TIMED_INC;
	e_first.subtype = TMD_FAST;
	e_first.dice = dice_new();
	require(dice_parse_string(e_first.dice, "20"));

	e_second.index = EF_TIMED_INC;
	e_second.subtype = TMD_HERO;
	e_second.dice = dice_new();
	require(dice_parse_string(e_second.dice, "20"));

	e_always.index = EF_TIMED_INC;
	e_always.subtype = TMD_BOLD;
	e_always.dice = dice_new();
	require(dice_parse_string(e_always.dice, "20"));

	first.effect = &e_first;
	first.chance = expression_new();
	expression_add_operations_string(first.chance, "+ 51");	/* half the time */
	first.next = &second;

	second.effect = &e_second;
	second.chance = expression_new();
	expression_add_operations_string(second.chance, "+ 101");	/* always */
	second.next = &always;

	always.effect = &e_always;			/* no chance: unconditional */

	pw.name = string_make("test power");
	pw.effects = &first;
	pw.stat = STAT_STR;

	/*
	 * A power can fail on its own roll, and this one has no failure figure to
	 * speak of but still gets the floor every power gets.  The unconditional
	 * band is what says whether the cast happened at all -- it runs on every
	 * success and on none of the failures -- so it is both the signal and the
	 * thing being tested: if either alternative fired, the plain band must have
	 * fired with it.
	 */
	for (i = 0; i < 600; i++) {
		bool a, b;

		player_clear_timed(player, TMD_FAST, false, false);
		player_clear_timed(player, TMD_HERO, false, false);
		player_clear_timed(player, TMD_BOLD, false, false);
		player->csp = player->msp;

		require(player_use_power(player, &pw, 0));

		a = player->timed[TMD_FAST] > 0;
		b = player->timed[TMD_HERO] > 0;

		if (a || b) require(player->timed[TMD_BOLD] > 0);
		if (player->timed[TMD_BOLD] == 0) {
			fizzled++;
			continue;
		}

		if (a && b) both++;
		if (!a && !b) neither++;
		if (a) saw_first++;
		if (b) saw_second++;
	}

	/* Never both, and never neither: exactly one alternative every time */
	eq(both, 0);
	eq(neither, 0);

	/* And the sample is real rather than mostly failures */
	require(fizzled < 200);
	require(saw_first + saw_second + fizzled == 600);

	/* 51 in a hundred for the first, the rest to the second */
	require(saw_first > (600 - fizzled) / 4);
	require(saw_first < (3 * (600 - fizzled)) / 4);
	require(saw_second > (600 - fizzled) / 4);

	dice_free(e_first.dice);
	dice_free(e_second.dice);
	dice_free(e_always.dice);
	expression_free(first.chance);
	expression_free(second.chance);
	string_free(pw.name);
	ok;
}


/**
 * A Draconian says what it breathed, in words (review of 3.105-3.124).
 *
 * "You breathe %s." used to be filled from the projection's `desc`, which for a
 * monster-facing projection is a verb phrase written for a different sentence
 * -- so three of the class bands produced "You breathe hurts and bewilders
 * monsters at once." and the like. `player_desc` is the noun the game already
 * uses when listing what a creature can breathe.
 *
 * Asserted on the message the player is shown rather than on the field, because
 * the field being present was never the problem: the handler was reading the
 * one next to it.
 */
static int test_a_draconian_says_what_it_breathed(void *state) {
	static const char *const bands[] = {
		"MISSILE", "SHARD", "MANA", "DISEN", "CONFUSION", "CHAOS",
		"SOUND", "MON_PSI", "HOLY_ORB", "HOLY_FIRE", "DARK", "POIS",
		"FIRE", "COLD"
	};
	size_t b;

	require(player_make_simple("Draconian", "Warrior", "Tester"));
	player->depth = 1;
	prepare_next_level(player);
	player->lev = 40;

	for (b = 0; b < N_ELEMENTS(bands); b++) {
		int idx = proj_name_to_idx(bands[b]);
		uint16_t before;
		const char *msg;
		bool said = false;
		int i, fresh;

		require(idx >= 0);
		before = messages_num();

		effect_simple(EF_BREATH, source_player(), "20", idx, 0, 30, 0, 0,
					  NULL);

		fresh = (int) messages_num() - (int) before;
		for (i = 0; i < fresh && i < (int) messages_num(); i++) {
			msg = message_str((uint16_t) i);
			if (!msg || !prefix(msg, "You breathe ")) continue;
			said = true;

			/*
			 * The noun, and nothing that reads as a sentence. Every
			 * `player_desc` in the game is a short noun phrase; a `desc`
			 * written for "%s which are in line of sight" is not, and the
			 * three that bit here all contain a verb followed by "monsters"
			 * or "the".
			 */
			if (strstr(msg, "monsters") || strstr(msg, " and leaves ")) {
				printf("breathing %s says: %s\n", bands[b], msg);
				require(false);
			}
			if (!strstr(msg, projections[idx].player_desc)) {
				printf("breathing %s says '%s', wanted the player "
					   "description '%s'\n", bands[b], msg,
					   projections[idx].player_desc);
				require(false);
			}
		}
		require(said);
	}
	ok;
}

/**
 * The Golem's power is a shield, and it costs nothing to wear.
 *
 * Zangband's `inc_shield()` ([racial.c:514](../archive/zangband/src/racial.c#L514))
 * is +50 armour class and nothing else. Ours read `TIMED_INC:STONESKIN`, which
 * is +40 *and -5 speed* -- so the Golem paid a penalty the archive never
 * charged and got less armour for it. The name misled: Zangband's shield
 * message is literally "Your skin turns to stone."
 *
 * Asserted on what the character is afterwards rather than on which timed
 * effect the data file names, because the effect's name is the thing that was
 * wrong. The speed half is the part that matters most and is easiest to lose:
 * a test that only checked the armour would pass for `STONESKIN` at +50.
 */
static int test_the_golems_power_is_a_shield(void *state) {
	struct player_race *golem = race_named("Golem");
	struct player_power *power;
	int ac_before, speed_before;

	require(golem);

	/*
	 * A real birth and a real level, not `grown_to()`.
	 *
	 * That helper swaps the race pointer and recalculates, which is all the
	 * intrinsic tests need; using a *power* runs the effect machinery, and
	 * that wants a character who was born and a level under their feet. The
	 * first version of this crashed for exactly that reason.
	 */
	require(player_make_simple("Golem", NULL, "Tester"));
	prepare_next_level(player);
	player->lev = 25;
	calc_bonuses(player, &player->state, false, true);

	power = (struct player_power *) player->race->powers;
	require(power);
	require(streq(power->name, "turn to stone"));

	/* Enough to pay for it, and no failure worth waiting out */
	player->csp = player->msp = 200;
	player->chp = player->mhp;

	update_stuff(player);
	ac_before = player->state.to_a;
	speed_before = player->state.speed;

	/*
	 * Until it lands. `power-fail:8` means a use can simply fail, and a
	 * single attempt turns a test about the effect into a test about the
	 * dice -- it failed about one run in ten while this was being written.
	 */
	{
		int try;

		for (try = 0; try < 200 && !player->timed[TMD_SHIELD]; try++) {
			player->csp = player->msp;
			player->chp = player->mhp;
			require(player_use_power(player, power, 0));
		}
	}
	update_stuff(player);

	require(player->timed[TMD_SHIELD] > 0);
	eq(player->state.to_a - ac_before, 50);
	eq(player->state.speed, speed_before);

	/* And it is not the one that slows you */
	eq(player->timed[TMD_STONESKIN], 0);
	ok;
}

/**
 * Every flag a race or class carries can be read on the birth screen.
 *
 * `ui-birth.c` and `view_abilities()` both walk `player_abilities`, which is
 * built from `player_property.txt`, and skip any flag with no record there.
 * `UNDEAD` and `BLOOD_DIET` had none, so five races were never told they were
 * undead and the Vampire was never told why its food does nothing -- the two
 * flags of thirty-five that nothing could display.
 *
 * Written as a sweep of the data rather than as two assertions about those two
 * flags, because two named assertions would not have caught the next flag
 * added without a record, which is exactly how these two arrived.
 */
static int test_every_racial_flag_can_be_read(void *state) {
	const struct player_race *r;
	const struct player_class *c;
	bitflag used[PF_SIZE];
	int flag, missing = 0;

	pf_wipe(used);
	for (r = races; r; r = r->next) pf_union(used, r->pflags);
	for (c = classes; c; c = c->next) pf_union(used, c->pflags);

	for (flag = 1; flag < PF_MAX; flag++) {
		const struct player_ability *a;
		bool found = false;

		if (!pf_has(used, flag)) continue;

		for (a = player_abilities; a; a = a->next) {
			if (!streq(a->type, "player")) continue;
			if (a->index != flag) continue;
			found = true;
			break;
		}

		if (!found) {
			printf("  player flag %d is carried by a race or class and has "
				   "no player_property.txt record\n", flag);
			missing++;
		}
	}

	eq(missing, 0);

	/* And the two that were missing are there by name, with something to say */
	{
		const struct player_ability *a;
		bool undead = false, diet = false;

		for (a = player_abilities; a; a = a->next) {
			if (!streq(a->type, "player")) continue;
			if (a->index == PF_UNDEAD) undead = a->desc && a->desc[0];
			if (a->index == PF_BLOOD_DIET) diet = a->desc && a->desc[0];
		}
		require(undead);
		require(diet);
	}
	ok;
}

/**
 * Every imported race's stat line is the archive's (PLR-01, DEC-103).
 *
 * `game/wild` pins the experience factor, which was Zangband's balance dial,
 * and nothing pinned anything else: the five stat modifiers, the skills, the
 * hit die and infravision were all transcribed by hand from
 * [tables.c](../archive/zangband/src/tables.c) and then guarded by nobody.
 *
 * The stat line is pinned here and the rest is not, deliberately. Stats map one
 * to one -- Zangband's six drop CHR, which 4.2 does not have, and the remaining
 * five are in the same order -- so the table below can be compared against the
 * archive with no interpretation. The skills do not: 4.2 splits disarming into
 * physical and magical, and reconciling those against the archive's single
 * number is a judgement rather than a transcription. Pinning something that
 * needed interpreting would pin the interpretation.
 *
 * All seventeen were checked against the archive when this was written and all
 * seventeen already agreed, so what this guards is drift from here on.
 */
static int test_every_imported_race_keeps_its_stats(void *state) {
	static const struct {
		const char *name;
		int stat[STAT_MAX];
	} archive[] = {
		{ "Amberite",   {  1,  2,  2,  2,  3 } },
		{ "Beastman",   {  2, -2, -1, -1,  2 } },
		{ "Yeek",       { -2,  1,  1,  1, -2 } },
		{ "Draconian",  {  2,  1,  1,  1,  2 } },
		{ "Mindflayer", { -3,  4,  4,  0, -2 } },
		{ "Vampire",    {  3,  3, -1, -1,  1 } },
		{ "Golem",      {  4, -5, -5, -2,  4 } },
		{ "Barbarian",  {  3, -2, -1,  1,  2 } },
		{ "Klackon",    {  2, -1, -1,  1,  2 } },
		{ "Nibelung",   {  1, -1,  2,  0,  2 } },
		{ "Imp",        { -1, -1, -1,  1,  2 } },
		{ "Skeleton",   {  0, -2, -2,  0,  1 } },
		{ "Zombie",     {  2, -6, -6,  1,  4 } },
		{ "Spectre",    { -5,  4,  4,  2, -3 } },
		{ "Ghoul",      {  0, -1, -1, -1,  1 } },
		{ "Sprite",     { -4,  3,  3,  3, -2 } },
		{ "Half-Titan", {  5,  1,  1, -2,  3 } },
	};
	size_t i;
	int j, wrong = 0;

	for (i = 0; i < N_ELEMENTS(archive); i++) {
		const struct player_race *r = race_named(archive[i].name);

		if (!r) {
			printf("  %s is not a race any more\n", archive[i].name);
			wrong++;
			continue;
		}

		for (j = 0; j < STAT_MAX; j++) {
			if (r->r_adj[j] == archive[i].stat[j]) continue;

			printf("  %s stat %d is %d, the archive says %d\n",
				   archive[i].name, j, r->r_adj[j], archive[i].stat[j]);
			wrong++;
		}
	}

	eq(wrong, 0);

	/* And the list has not quietly shrunk under the loop. */
	eq((int) N_ELEMENTS(archive), 17);
	ok;
}

const char *suite_name = "player/race";
struct test tests[] = {
	{ "the-draconian-grows-into-its-scales",
			test_the_draconian_grows_into_its_scales },
	{ "a-sprite-flies-faster-as-it-grows",
			test_a_sprite_flies_faster_as_it_grows },
	{ "a-yeek-becomes-immune-to-acid",
			test_a_yeek_becomes_immune_to_acid },
	{ "the-mindflayer-grows-into-its-mind",
			test_the_mindflayer_grows_into_its_mind },
	{ "every-imported-race-keeps-its-stats",
	  test_every_imported_race_keeps_its_stats },
	{ "the-golems-power-is-a-shield", test_the_golems_power_is_a_shield },
	{ "every-racial-flag-can-be-read", test_every_racial_flag_can_be_read },
	{ "the-golem-is-made-of-something",
			test_the_golem_is_made_of_something },
	{ "the-vampire-glows-and-starves",
			test_the_vampire_glows_and_starves },
	{ "the-second-wave-of-races",
			test_the_second_wave_of_races },
	{ "the-undead-share-one-shape",
			test_the_undead_share_one_shape },
	{ "every-level-gate-actually-opens",
			test_every_level_gate_actually_opens },
	{ "a-vampire-gets-little-from-food",
			test_a_vampire_gets_little_from_food },
	{ "the-sun-burns-a-vampire-outdoors",
			test_the_sun_burns_a_vampire_outdoors },
	{ "a-vampire-can-put-out-the-daylight",
			test_a_vampire_can_put_out_the_daylight },
	{ "darkness-is-shelter-from-the-sun",
			test_darkness_is_shelter_from_the_sun },
	{ "a-spectre-walks-through-rock",
			test_a_spectre_walks_through_rock },
	{ "rock-grinds-a-spectre-but-does-not-kill-it",
			test_rock_grinds_a_spectre_but_does_not_kill_it },
	{ "a-mountain-costs-a-spectre-blood",
			test_a_mountain_costs_a_spectre_blood },
	{ "the-undead-wake-in-the-dark",
			test_the_undead_wake_in_the_dark },
	{ "every-race-has-a-body", test_every_race_has_a_body },
	{ "the-shared-races-kept-their-powers",
			test_the_shared_races_kept_their_powers },
	{ "a-power-expression-means-what-it-says",
			test_a_power_expression_means_what_it_says },
	{ "the-bloodless-do-not-bleed", test_the_bloodless_do_not_bleed },
	{ "a-draconian-says-what-it-breathed",
			test_a_draconian_says_what_it_breathed },
	{ "a-draconian-breathes-by-its-class",
			test_a_draconian_breathes_by_its_class },
	{ "every-class-named-by-a-race-exists",
			test_every_class_named_by_a_race_exists },
	{ "a-class-the-archive-lacks-falls-back",
			test_a_class_the_archive_lacks_falls_back },
	{ "chance-bands-are-alternatives",
			test_chance_bands_are_alternatives },
	{ NULL, NULL }
};
