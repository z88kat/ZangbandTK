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
	{ "a-sprite-flies-faster-as-it-grows",
			test_a_sprite_flies_faster_as_it_grows },
	{ "a-yeek-becomes-immune-to-acid",
			test_a_yeek_becomes_immune_to_acid },
	{ "the-mindflayer-grows-into-its-mind",
			test_the_mindflayer_grows_into_its_mind },
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
	{ "the-undead-wake-in-the-dark",
			test_the_undead_wake_in_the_dark },
	{ NULL, NULL }
};
