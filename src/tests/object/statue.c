/* object/statue.c
 *
 * Zangband's statues and its Statue Store (ZangbandTK, DEC-145), and its
 * Figurine Store (DEC-146).
 *
 * A statue is a monster rendered in a material and worth what the material is
 * worth: it does nothing else, so what is checked here is that it is that and
 * no more -- Zangband's ten, named for their monster, priced by their
 * material, never stacked -- and that the shop which deals in them stocks
 * statues and nothing else, buys them, and reaches deeper when it is better.
 */

#include "unit-test.h"
#include "test-utils.h"

#include "init.h"
#include "monster.h"
#include "obj-desc.h"
#include "obj-knowledge.h"
#include "obj-make.h"
#include "obj-pile.h"
#include "obj-power.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "object.h"
#include "player-birth.h"
#include "store.h"
#include "wild.h"

static void println(const char *str) {
	printf("%s\n", str);
}

int setup_tests(void **state) {
	plog_aux = println;
	set_file_paths();
	if (!init_angband()) return 1;
	(void) test_seed_rng_reported(suite_name);
	if (!player_make_simple(NULL, "Warrior", "Tester")) return 1;
	*state = NULL;
	return 0;
}

int teardown_tests(void *state) {
	cleanup_angband();
	return 0;
}

static struct object_kind *statue_kind(const char *material) {
	char name[80];

	strnfmt(name, sizeof(name), "%s Statue", material);
	return lookup_kind(TV_STATUE, lookup_sval(TV_STATUE, name));
}

/** A statue of that material and monster, as the player would know it. */
static struct object *statue_of(const char *material,
		struct monster_race *race) {
	struct object *obj = object_new();

	object_prep(obj, statue_kind(material), 0, MINIMISE);
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

static struct monster_race *a_race(bool unique) {
	int i;

	for (i = 1; i < z_info->r_max; i++) {
		struct monster_race *race = &r_info[i];

		if (!race->name || !race->rarity) continue;
		if (rf_has(race->flags, RF_UNIQUE) != unique) continue;
		if (!unique && is_a_vowel(race->name[0])) continue;
		return race;
	}
	return NULL;
}

static struct store *store_of(int feat) {
	int i;

	for (i = 0; i < z_info->store_max; i++)
		if (stores[i].feat == feat) return &stores[i];
	return NULL;
}

static bool buys(const struct store *s, int tval) {
	const struct object_buy *buy;

	for (buy = s->buy; buy; buy = buy->next)
		if ((int) buy->tval == tval) return true;
	return false;
}

/**
 * Zangband's ten, from k_info 568-577: its costs, and its weights in tenths of
 * a pound -- twenty pounds of wood, fifty of anything else.
 */
static int test_the_ten_are_zangbands(void *state) {
	static const struct { const char *material; int cost, weight, depth; }
	ten[] = {
		{ "Wooden", 75, 200, 9 }, { "Clay", 50, 500, 17 },
		{ "Stone", 100, 500, 26 }, { "Iron", 200, 500, 35 },
		{ "Copper", 500, 500, 43 }, { "Silver", 1000, 500, 52 },
		{ "Golden", 2500, 500, 63 }, { "Ivory", 2000, 500, 70 },
		{ "Mithril", 10000, 500, 95 }, { "Ornate", 5000, 500, 80 },
	};
	size_t i;

	for (i = 0; i < N_ELEMENTS(ten); i++) {
		struct object_kind *kind = statue_kind(ten[i].material);

		notnull(kind);
		eq(kind->cost, ten[i].cost);
		eq(kind->weight, ten[i].weight);
		eq(kind->alloc_min, ten[i].depth);
	}
	ok;
}

/** Any monster at all, uniques included, as Zangband's own choice was. */
static int test_a_made_statue_is_of_any_monster(void *state) {
	int i, uniques = 0;

	for (i = 0; i < 1000; i++) {
		struct object *obj = object_new();

		object_prep(obj, statue_kind("Stone"), 10, RANDOMISE);
		apply_magic(obj, 10, false, false, false, false);
		notnull(obj->race);
		require(obj->race->rarity > 0);
		if (rf_has(obj->race->flags, RF_UNIQUE)) uniques++;
		object_delete(NULL, NULL, &obj);
	}
	require(uniques > 0);
	ok;
}

/** "a Golden Statue of Smeagol": a unique by name, anything else with "a". */
static int test_its_name_is_its_likeness(void *state) {
	struct monster_race *unique = a_race(true), *common = a_race(false);
	struct object *obj;
	char name[160], want[160];

	notnull(unique);
	notnull(common);

	obj = statue_of("Golden", unique);
	object_desc(name, sizeof(name), obj, ODESC_PREFIX | ODESC_FULL, player);
	strnfmt(want, sizeof(want), "a Golden Statue of %s", unique->name);
	require(streq(name, want));
	unmake(obj);

	obj = statue_of("Iron", common);
	object_desc(name, sizeof(name), obj, ODESC_PREFIX | ODESC_FULL, player);
	strnfmt(want, sizeof(want), "an Iron Statue of a %s", common->name);
	require(streq(name, want));
	unmake(obj);
	ok;
}

/** Never in a stack of the player's own, even of one monster. */
static int test_statues_never_stack(void *state) {
	struct monster_race *race = a_race(false);
	struct object *one = statue_of("Clay", race), *two = statue_of("Clay", race);

	require(!object_similar(one, two, OSTACK_PACK));
	unmake(one);
	unmake(two);
	ok;
}

/** Priced by the material, whatever it depicts. */
static int test_a_statue_is_worth_its_material(void *state) {
	struct object *plain = statue_of("Silver", a_race(false));
	struct object *famous = statue_of("Silver", a_race(true));

	eq(object_value(plain, 1), 1000);
	eq(object_value(famous, 1), 1000);
	unmake(plain);
	unmake(famous);
	ok;
}

/**
 * The Statue Store sells statues and only statues, each of a monster, and buys
 * them; the general store and the black market take them too (DEC-145).
 */
static int test_the_statue_store_deals_in_statues(void *state) {
	struct store *s = store_of(FEAT_STORE_STATUE);
	struct store *general = store_of(FEAT_STORE_GENERAL);
	struct store *black = store_of(FEAT_STORE_BLACK);
	struct object *obj;
	size_t i;
	int seen = 0;

	notnull(s);
	notnull(s->owners);
	notnull(s->owners->next);
	require(buys(s, TV_STATUE));
	for (i = 0; i < s->normal_num; i++)
		eq(s->normal_table[i]->tval, TV_STATUE);

	store_stock_at_quality(s, 0);
	for (obj = s->stock; obj; obj = obj->next) {
		eq(obj->tval, TV_STATUE);
		notnull(obj->race);
		seen++;
	}
	require(seen > 0);

	/* And the two others that buy them, by the owner's ruling */
	notnull(general);
	require(buys(general, TV_STATUE));
	notnull(black);
	null(black->buy);
	ok;
}

/**
 * An Expert or Arcane one reaches for the precious metals: Zangband's Unusual
 * Statue Store, on the quality ladder rather than behind a door of its own.
 */
static int test_a_better_statue_store_reaches_deeper(void *state) {
	struct store *s = store_of(FEAT_STORE_STATUE);
	long plain = 0, arcane = 0;
	int n_plain = 0, n_arcane = 0, round;

	notnull(s);
	for (round = 0; round < 20; round++) {
		struct object *obj;

		store_stock_at_quality(s, 0);
		for (obj = s->stock; obj; obj = obj->next) {
			plain += obj->kind->level;
			n_plain++;
		}
		store_stock_at_quality(s, quality_tier_count);
		for (obj = s->stock; obj; obj = obj->next) {
			arcane += obj->kind->level;
			n_arcane++;
		}
	}
	require(n_plain > 0 && n_arcane > 0);
	require(arcane * n_plain > plain * n_arcane);
	ok;
}

/**
 * The Figurine Store sells figurines, of monsters about as deep as Zangband's
 * did -- level 26, from store.txt's `level:` -- rather than of whatever lives
 * by the town gate, and buys them (DEC-146).
 */
static int test_the_figurine_store_deals_in_figurines(void *state) {
	struct store *s = store_of(FEAT_STORE_FIGURINE);
	struct object *obj;
	int round, seen = 0, deep = 0;

	notnull(s);
	notnull(s->owners);
	notnull(s->owners->next);
	require(buys(s, TV_FIGURINE));
	eq(s->level_min, 26);
	eq(s->level_max, 26);

	/*
	 * Forty rounds, not five, and the bar at 0.45 rather than 0.5
	 * (ZangbandTK, DEC-153).
	 *
	 * The proportion of figurines holding a level-10-or-deeper monster has
	 * a mean of 0.60 whatever the sample size; what changes is the spread.
	 * At five rounds the standard deviation is 0.081, so a bar at 0.50 sits
	 * about 1.2 sd below the mean and the test failed 12.8% of the seeds
	 * measured here -- roughly one CI run in seven, on a suite that runs on
	 * every push on every platform. At forty rounds the deviation is 0.032.
	 *
	 * Both numbers moved, because widening alone is not enough: forty
	 * rounds against 0.50 is still only 3.1 sd and that is thin for
	 * something this often run. Forty against 0.45 is 4.7 sd, and 0 of 400
	 * seeds failed with the lowest ratio seen at 0.510.
	 *
	 * The bar moving is not the guarantee weakening. The claim is that this
	 * shop reaches meaningfully deeper than the monsters by the town gate,
	 * and 0.45 of a distribution centred on 0.60 still says that. What it
	 * stops doing is sitting on top of its own noise.
	 *
	 * Forty rounds costs 4ms: 74ms a run before, 78ms after, because the
	 * time goes on starting the process and reading the data rather than on
	 * stocking a shop.
	 */
	for (round = 0; round < 40; round++) {
		store_stock_at_quality(s, 0);
		for (obj = s->stock; obj; obj = obj->next) {
			eq(obj->tval, TV_FIGURINE);
			notnull(obj->race);
			require(!rf_has(obj->race->flags, RF_UNIQUE));
			seen++;
			if (obj->race->level >= 10) deep++;
		}
	}
	require(seen > 0);

	/* Mostly deeper than the surface's monsters, which a level of 5 gives */
	require(deep * 20 > seen * 9);
	ok;
}

/**
 * A shop with one keeper keeps them.  store_shuffle() wanted someone other
 * than the incumbent and looped for ever when there was nobody.
 */
static int test_a_lone_keeper_stays(void *state) {
	struct owner one;
	struct store shop;

	memset(&one, 0, sizeof(one));
	memset(&shop, 0, sizeof(shop));
	shop.owners = &one;
	shop.owner = &one;
	store_shuffle(&shop);
	require(shop.owner == &one);
	ok;
}

const char *suite_name = "object/statue";
struct test tests[] = {
	{ "the-ten-are-zangbands", test_the_ten_are_zangbands },
	{ "a-made-statue-is-of-any-monster", test_a_made_statue_is_of_any_monster },
	{ "its-name-is-its-likeness", test_its_name_is_its_likeness },
	{ "statues-never-stack", test_statues_never_stack },
	{ "a-statue-is-worth-its-material", test_a_statue_is_worth_its_material },
	{ "the-statue-store-deals-in-statues",
	  test_the_statue_store_deals_in_statues },
	{ "a-better-statue-store-reaches-deeper",
	  test_a_better_statue_store_reaches_deeper },
	{ "the-figurine-store-deals-in-figurines",
	  test_the_figurine_store_deals_in_figurines },
	{ "a-lone-keeper-stays", test_a_lone_keeper_stays },
	{ NULL, NULL }
};
