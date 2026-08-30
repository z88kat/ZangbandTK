/* object/imported
 *
 * Zangband's object kinds, on 4.2's model (ZangbandTK, CNT-11).
 *
 * The import is 82 object kinds out of Zangband's 551, and most of what can go
 * wrong with it is invisible: a kind that 4.2 already has under another name
 * arrives as a second copy of the same item, an artifact base arrives as an
 * ordinary object and displaces the dummy an artifact needed, or a flag that
 * scales by pval turns a +1 light radius into +125. None of those looks like a
 * failure. They look like more content.
 *
 * So the tests here mostly assert the *absence* of things, and each is written
 * so that removing the rule it guards makes it fail rather than merely making
 * it pass differently.
 */

#include "unit-test.h"

#include "init.h"
#include "mon-make.h"
#include "obj-util.h"
#include "object.h"
#include "player-birth.h"
#include "player-calcs.h"
#include "player-luck.h"
#include "test-utils.h"

int setup_tests(void **state) {
	set_file_paths();
	init_angband();
#ifdef UNIX
	create_needed_dirs();
#endif

	if (!player_make_simple(NULL, "Mage", "Tester")) {
		cleanup_angband();
		return 1;
	}

	*state = NULL;
	return 0;
}

int teardown_tests(void *state) {
	cleanup_angband();
	return 0;
}

/** The kind of `tval` whose name is `name`, or NULL. */
static struct object_kind *kind_named(int tval, const char *name)
{
	int i;

	for (i = 0; i < z_info->k_max; i++) {
		struct object_kind *kind = &k_info[i];

		if (!kind->name || kind->tval != tval) continue;
		if (!my_stricmp(kind->name, name)) return kind;
	}

	return NULL;
}

/**
 * A sample of the import arrived, across the shapes it takes.
 *
 * One weapon, one armour, one ring, one amulet, one potion and one blade with
 * a mechanism behind it -- between them they exercise every branch of the
 * converter that writes an entry.
 */
static int test_the_imported_kinds_are_there(void *state) {
	notnull(kind_named(TV_SWORD, "& No-dachi~"));
	notnull(kind_named(TV_SOFT_ARMOR, "& Hagaromo~"));
	notnull(kind_named(TV_RING, "Fate"));
	notnull(kind_named(TV_AMULET, "Anti-Magic"));
	notnull(kind_named(TV_POTION, "Booze"));
	notnull(kind_named(TV_SWORD, "& Psiblade~"));

	ok;
}

/**
 * No object kind was imported twice.
 *
 * The rename trap. Zangband renamed 26 of the kinds it inherited from Angband
 * 2.8.1 -- its Ring of Skill is Angband's Ring of Accuracy, its Scroll of
 * Enchant Weapon Deadliness is Enchant Weapon To-Dam -- and 4.2 still ships
 * ten of those under the older name. Comparing names alone does not find them,
 * because the names differ; comparing slots does.
 *
 * This is the check on the other half: the eleven kinds Zangband and 4.2 added
 * independently under the *same* name. Drop the converter's presence test and
 * eleven duplicates appear here.
 */
static int test_no_kind_is_imported_twice(void *state) {
	int i, j, duplicates = 0;

	for (i = 0; i < z_info->k_max; i++) {
		if (!k_info[i].name) continue;

		for (j = i + 1; j < z_info->k_max; j++) {
			if (!k_info[j].name) continue;
			if (k_info[i].tval != k_info[j].tval) continue;
			if (my_stricmp(k_info[i].name, k_info[j].name)) continue;

			duplicates++;
		}
	}

	eq(duplicates, 0);

	ok;
}

/**
 * Zangband's artifact bases stayed out.
 *
 * Sixteen of its kinds are marked INSTA_ART: they exist so an artifact has
 * something to hang on, are never generated in their own right, and seven of
 * them are all called "Ring". 4.2 writes its own dummy record for an artifact
 * base it does not have, so importing these fills no gap -- it puts a plain
 * Ring in front of the one the Ring of Barahir needs, and artifact.txt then
 * fails to parse with "not a special artifact".
 *
 * That failure is loud, but it happens at startup and names the wrong file, so
 * it is worth catching here where the cause is written down.
 */
static int test_the_artifact_bases_stayed_out(void *state) {
	struct object_kind *ring = kind_named(TV_RING, "& Ring~");
	struct object_kind *amulet = kind_named(TV_AMULET, "& Amulet~");

	/* If either exists at all it must be the dummy, which has no level. */
	if (ring) require(kf_has(ring->kind_flags, KF_INSTA_ART));
	if (amulet) require(kf_has(amulet->kind_flags, KF_INSTA_ART));

	ok;
}

/**
 * Every flavoured tval has a flavour to spare.
 *
 * 4.2 gives each ring, amulet, potion, scroll, wand, staff, rod and mushroom
 * its own flavour and calls `quit()` if it runs out. The import takes rings
 * from 30 kinds to 45 against 39 flavours and amulets from 17 to 26 against
 * 20, so both pools needed Zangband's own longer lists behind them.
 *
 * The margin, rather than the shortage, is what is asserted here, and
 * deliberately: a pool that has actually run dry kills the game during
 * `init_angband()`, so this test would never get to run -- what happens
 * instead is that seventeen suites die at once and none of them mentions
 * flavours. Requiring at least one spare catches the state just before that,
 * where the game still starts and the next kind added breaks it. The tightest
 * pool is wands, with five.
 */
static int test_every_flavoured_tval_has_enough_flavours(void *state) {
	static const int flavoured[] = {
		TV_RING, TV_AMULET, TV_POTION, TV_SCROLL,
		TV_WAND, TV_STAFF, TV_ROD, TV_MUSHROOM
	};
	size_t t;

	for (t = 0; t < N_ELEMENTS(flavoured); t++) {
		int kinds = 0, choices = 0, i;
		struct flavor *f;

		for (i = 0; i < z_info->k_max; i++) {
			if (k_info[i].name && k_info[i].tval == flavoured[t]) kinds++;
		}
		for (f = flavors; f; f = f->next) {
			if (f->tval == flavoured[t]) choices++;
		}

		require(choices > kinds);
	}

	ok;
}

/**
 * The objects that were deferred are not in the game.
 *
 * Nineteen kinds were held back for a mechanism 4.2 has not got: ten statues
 * and a figurine whose names interpolate a monster, a wand and a figurine that
 * make pets, a potion that cures mutations, and five more. Each was a decision
 * with a reason recorded in objmap.toml, and a later edit that quietly imports
 * one anyway would give the game a Wand of Tame Monster that does nothing.
 */
static int test_the_deferred_objects_are_absent(void *state) {
	null(kind_named(TV_WAND, "Tame Monster"));
	null(kind_named(TV_POTION, "New Life"));
	null(kind_named(TV_SCROLL, "Artifact Creation"));
	null(kind_named(TV_RING, "Wizardry"));
	null(kind_named(TV_ROD, "Havoc"));

	ok;
}

/**
 * Weird luck cuts both ways.
 *
 * The whole reason the flag needed reading rather than guessing. `STRANGE_LUCK`
 * is on a Ring of Fate and sounds like a blessing; what it does is multiply
 * every melee critical by 3/2 including the ones monsters land on the wearer,
 * and Zangband's own comment above the third of its four sites reads "Luck
 * isn't always good for you...".
 *
 * A version of this flag that only helped the player would pass a test that
 * checked the player's side alone, which is why both sides are checked here
 * and why the second assertion is the point of the test.
 */
static int test_weird_luck_cuts_both_ways(void *state) {
	int plain_player, plain_monster;

	of_off(player->state.flags, OF_STRANGE_LUCK);
	plain_player = luck_crit_scale(player, 100);
	plain_monster = luck_monster_crit(player, 100);
	eq(plain_player, 100);
	eq(plain_monster, 100);

	of_on(player->state.flags, OF_STRANGE_LUCK);
	require(luck_crit_scale(player, 100) > plain_player);
	require(luck_monster_crit(player, 100) > plain_monster);

	of_off(player->state.flags, OF_STRANGE_LUCK);

	ok;
}

/**
 * And it puts things on the level that do not belong there.
 *
 * The fourth site, and the one furthest from anything the name suggests: one
 * monster in thirteen is generated from up to forty levels deeper. Sampled
 * rather than asserted once, since the common case is no boost at all.
 */
static int test_weird_luck_reaches_out_of_depth(void *state) {
	int i, boosted = 0, biggest = 0;

	of_off(player->state.flags, OF_STRANGE_LUCK);
	for (i = 0; i < 500; i++) {
		eq(luck_depth_boost(player), 0);
	}

	of_on(player->state.flags, OF_STRANGE_LUCK);
	for (i = 0; i < 2000; i++) {
		int boost = luck_depth_boost(player);

		if (boost) boosted++;
		if (boost > biggest) biggest = boost;
	}
	of_off(player->state.flags, OF_STRANGE_LUCK);

	/* One in thirteen of 2000 is about 154; well clear of zero either way. */
	require(boosted > 50);
	require(boosted < 500);
	require(biggest > 10);

	ok;
}

/**
 * A psiblade spends mana to land its criticals, and stops when there is none.
 *
 * The flag is not a free bonus: each critical it powers costs one to three
 * points. A version that forgot to spend would be strictly better than the
 * original and would pass any test that only looked at the damage.
 */
static int test_a_psiblade_spends_mana(void *state) {
	int before, bonus;

	of_on(player->state.flags, OF_PSI_CRIT);

	player->csp = 30;
	before = player->csp;
	bonus = psi_crit_spend(player);
	require(bonus >= 1);
	require(bonus <= 3);
	eq(player->csp, before - bonus);

	/* Empty, and it cannot arm at all. */
	player->csp = 0;
	require(!psi_crit_armed(player));

	of_off(player->state.flags, OF_PSI_CRIT);
	player->csp = 0;

	ok;
}

/**
 * An anti-magic shell stops the wearer casting.
 */
static int test_an_anti_magic_shell_stops_casting(void *state) {
	of_off(player->state.flags, OF_NO_MAGIC);
	require(!player_magic_blocked(player, false));

	of_on(player->state.flags, OF_NO_MAGIC);
	require(player_magic_blocked(player, false));

	of_off(player->state.flags, OF_NO_MAGIC);

	ok;
}

/**
 * A light radius is a radius, not a pval.
 *
 * Zangband's `LITE` is a boolean -- `if (FLAG(o_ptr, TR_LITE)) cur_lite++` --
 * and reading it as one of the flags its single pval scales gave the Crown of
 * Chaos, whose pval is 125, a light radius of 125. That lights the whole level
 * and every level below it, and nothing about the data file looks wrong.
 *
 * Angband's own most generous artifact light is 4, so anything above it is the
 * bug coming back.
 */
static int test_a_light_radius_is_not_a_pval(void *state) {
	int i;

	for (i = 0; i < z_info->a_max; i++) {
		if (!a_info[i].name) continue;

		require(a_info[i].modifiers[OBJ_MOD_LIGHT] <= 4);
	}

	ok;
}

/**
 * An immunity is not overwritten by a resistance to the same thing.
 *
 * Zangband gives one item both, and both map onto one 4.2 property at
 * different levels. 4.2 assigns as it parses, so `RES_FIRE[3] | RES_FIRE[1]`
 * ends at 1 and the immunity is quietly lost -- the item still resists fire,
 * so nothing looks broken.
 */
static int test_an_immunity_outlives_its_resistance(void *state) {
	int i;
	bool checked = false;

	for (i = 0; i < z_info->a_max; i++) {
		if (!a_info[i].name) continue;
		if (my_stricmp(a_info[i].name, "'Twilight'")) continue;

		eq(a_info[i].el_info[ELEM_FIRE].res_level, 3);
		checked = true;
	}

	require(checked);

	ok;
}

/**
 * No imported ego is an Angband ego under another name.
 *
 * Ego indices are 98.9% stable between Angband 2.8.1 and Zangband -- 86 of the
 * 87 they share carry the same ego -- which makes the index a usable identity
 * and makes the single divergence worth looking at. It was a rename: Zangband's
 * `of Levitation` is 2.8.1's `of Slow Descent`, same slot, same lone FEATHER
 * flag, and Angband still ships it under the older name. The pair matches the
 * Ring of Levitation and Ring of Feather Falling that CNT-11 found among the
 * objects, which is what prompted looking here at all.
 */
static int test_no_ego_duplicates_an_angband_ego(void *state) {
	int i, levitation = 0, slow_descent = 0;

	for (i = 0; i < z_info->e_max; i++) {
		if (!e_info[i].name) continue;
		if (!my_stricmp(e_info[i].name, "of Levitation")) levitation++;
		if (!my_stricmp(e_info[i].name, "of Slow Descent")) slow_descent++;
	}

	eq(levitation, 0);
	eq(slow_descent, 1);

	ok;
}

/**
 * No two artifacts share a name.
 *
 * Not tidiness: Angband writes an artifact's name into the savefile and reads
 * it back with `lookup_artifact_name`, which returns the first exact match
 * (obj-util.c:520, load.c:149). Two artifacts of one name means a saved
 * character can come back holding the other one.
 *
 * Zangband has two called "of Sawall" -- an Incandescent Globe and a Hard
 * Leather Cap, which read distinctly there because Zangband shows the base
 * object in the name. Only one is imported, and the converter now defers the
 * other with that reason written down instead of losing it to whichever the
 * reader happened to overwrite.
 *
 * This is an invariant rather than a test of that deferral: the second Sawall
 * is blocked by its base object as well, so removing the deferral does not
 * currently produce a clash. It is here because the clash is the thing that
 * would be expensive to find later -- a savefile that comes back wrong -- and
 * the next person to give that artifact a base is one edit away from it.
 */
static int test_no_two_artifacts_share_a_name(void *state) {
	int i, j, clashes = 0;

	for (i = 0; i < z_info->a_max; i++) {
		if (!a_info[i].name) continue;

		for (j = i + 1; j < z_info->a_max; j++) {
			if (!a_info[j].name) continue;
			if (my_stricmp(a_info[i].name, a_info[j].name)) continue;

			clashes++;
		}
	}

	eq(clashes, 0);

	ok;
}

/**
 * Every object kind has a row of the allocation table it can appear in.
 *
 * `alloc_init_objects()` builds one row per depth from 0 to
 * `obj-make:max-depth`, and `get_obj_num()` clamps every request into that
 * range (obj-make.c:1127). A kind whose whole allocation band lies below the
 * last row is never reachable: there is no level at which `lev >= alloc_min`
 * can be true. Nothing reports it, because the kind parses and the game runs.
 *
 * Zangband's dungeon went deeper than 4.2's table does, so its own depths do
 * not survive the crossing unaltered. Its Potion of Invulnerability sits at
 * 105 and arrived as `alloc:11:105 to 105`, which is five rows past the end of
 * a hundred-row table -- a potion in the data that no game could ever produce.
 *
 * Asserted for the whole object list rather than for that one potion, and
 * against the game's own constant rather than a literal 100, so that moving
 * the constant cannot quietly strand a kind on either side.
 */
static int test_every_kind_can_actually_be_generated(void *state) {
	int i, stranded = 0;

	for (i = 0; i < z_info->k_max; i++) {
		struct object_kind *kind = &k_info[i];

		if (!kind->name) continue;
		/* Artifact bases and their dummies are never rolled for. */
		if (kf_has(kind->kind_flags, KF_INSTA_ART)) continue;
		if (!kind->alloc_prob) continue;

		if (kind->alloc_min > z_info->max_obj_depth) stranded++;
		if (kind->alloc_min > kind->alloc_max) stranded++;
	}

	eq(stranded, 0);

	ok;
}

/**
 * And the deep imports are reachable at the bottom of the dungeon.
 *
 * The other half of the same fix, and the half that would still be wrong if
 * the clamp had been written as "drop anything too deep" instead. All three of
 * Zangband's below-100 objects should be findable in the deep game, not
 * discarded from it: the table's last row is the row every deeper level is
 * clamped into, so an object living there is available for the whole of the
 * bottom of the dungeon rather than on one level of it.
 */
static int test_the_deep_imports_reach_the_bottom(void *state) {
	static const char *deep[] = { "Invulnerability", "Lordly Protection",
								  "& Diamond Edge~" };
	size_t n;

	for (n = 0; n < N_ELEMENTS(deep); n++) {
		struct object_kind *kind = NULL;
		int i;

		for (i = 0; i < z_info->k_max; i++) {
			if (k_info[i].name && !my_stricmp(k_info[i].name, deep[n])) {
				kind = &k_info[i];
				break;
			}
		}

		notnull(kind);
		require(kind->alloc_prob > 0);
		require(kind->alloc_min <= z_info->max_obj_depth);
		require(kind->alloc_max >= z_info->max_obj_depth);
	}

	ok;
}

const char *suite_name = "object/imported";
struct test tests[] = {
	{ "the-imported-kinds-are-there", test_the_imported_kinds_are_there },
	{ "no-kind-is-imported-twice", test_no_kind_is_imported_twice },
	{ "the-artifact-bases-stayed-out", test_the_artifact_bases_stayed_out },
	{ "every-flavoured-tval-has-enough-flavours",
	  test_every_flavoured_tval_has_enough_flavours },
	{ "the-deferred-objects-are-absent", test_the_deferred_objects_are_absent },
	{ "weird-luck-cuts-both-ways", test_weird_luck_cuts_both_ways },
	{ "weird-luck-reaches-out-of-depth", test_weird_luck_reaches_out_of_depth },
	{ "a-psiblade-spends-mana", test_a_psiblade_spends_mana },
	{ "an-anti-magic-shell-stops-casting",
	  test_an_anti_magic_shell_stops_casting },
	{ "a-light-radius-is-not-a-pval", test_a_light_radius_is_not_a_pval },
	{ "an-immunity-outlives-its-resistance",
	  test_an_immunity_outlives_its_resistance },
	{ "no-ego-duplicates-an-angband-ego",
	  test_no_ego_duplicates_an_angband_ego },
	{ "no-two-artifacts-share-a-name", test_no_two_artifacts_share_a_name },
	{ "every-kind-can-actually-be-generated",
	  test_every_kind_can_actually_be_generated },
	{ "the-deep-imports-reach-the-bottom",
	  test_the_deep_imports_reach_the_bottom },
	{ NULL, NULL }
};
