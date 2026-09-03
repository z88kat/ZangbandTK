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
#include "effects.h"
#include "player-util.h"
#include "test-utils.h"

int setup_tests(void **state) {
	set_file_paths();
	init_angband();
#ifdef UNIX
	create_needed_dirs();
#endif

	(void) test_seed_rng_reported(suite_name);

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
 * Kinds held back for a mechanism 4.2 has not got: ten statues and a figurine
 * whose names interpolate a monster, a wand and a figurine that make pets, a
 * potion that cures mutations, and the rest. Each was a decision with a reason
 * recorded in objmap.toml, and a later edit that quietly imports one anyway
 * would give the game a Wand of Tame Monster that does nothing.
 *
 * The list shrinks as the mechanisms arrive, and this test is meant to fail
 * when it does -- the Ring of Wizardry left it the day mana became something
 * an object could modify.
 */
static int test_the_deferred_objects_are_absent(void *state) {
	null(kind_named(TV_SCROLL, "Artifact Creation"));
	null(kind_named(TV_WAND, "Rockets"));

	/*
	 * And the Wand of Tame Monster is here, since 3.72.0. It left this list
	 * the day monster allegiance arrived, which is what the note above says
	 * is supposed to happen -- so it is asserted present rather than deleted
	 * from the test, because "we imported it" and "we forgot about it" look
	 * the same once the line is gone.
	 */
	notnull(kind_named(TV_WAND, "Tame Monster"));

	ok;
}

/**
 * And the potion of New Life arrived, because mutations did.
 *
 * The other side of the test above, and the reason it is worth writing both.
 * CNT-11 deferred this one on the single ground that "mutations are unbuilt";
 * M8 built them, so the deferral had to be revisited rather than left to sit.
 * A deferral whose stated reason has expired and which nobody went back to is
 * the failure mode this pair is built against.
 *
 * Half of Zangband's potion survives. Its Lua is
 * `do_cmd_rerate(); cure_all_mutations()`, and 4.2 fixes hit points at birth
 * and has no rerate command -- so the mutations go and the hit dice stay. The
 * description was overridden to stop promising the half that does not happen.
 */
static int test_the_new_life_potion_arrived_with_mutations(void *state) {
	struct object_kind *kind = kind_named(TV_POTION, "New Life");

	notnull(kind);
	notnull(kind->effect);
	eq(kind->effect->index, EF_LOSE_MUTATION);

	/* More than a character can carry, which is how "all of them" is said. */
	notnull(kind->effect->dice);
	require(dice_evaluate(kind->effect->dice, 0, AVERAGE, NULL) >= 96);

	/* And it no longer claims to reroll anything. */
	notnull(kind->text);
	require(!strstr(kind->text, "hitpoint"));

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

/**
 * A Lord takes an interest in three different ways.
 *
 * Zangband's condition for a level reward is `TR_PATRON || (one_in_(7) &&
 * TR_STRANGE_LUCK)` ([xtra2.c:102](../archive/zangband/src/xtra2.c#L102)), and
 * the second half of it was missed when `STRANGE_LUCK` was built: the Ring of
 * Fate has five effects, four of them about critical hits, and this is the
 * fifth. Reading the flag as a combat property alone left a quarter of the
 * patron system unreachable by anyone who is not a Chaos-Warrior.
 *
 * The three ways in are asserted separately, because they are three different
 * mistakes: forgetting the item flag, forgetting the luck branch, and breaking
 * the class that already worked.
 */
static int test_a_lord_notices_three_different_ways(void *state) {
	const struct patron *born_to = player->patron;
	int i, noticed = 0;

	of_off(player->state.flags, OF_PATRON);
	of_off(player->state.flags, OF_STRANGE_LUCK);

	/* Sworn to one: always, and this is what already worked. */
	player->patron = patron_random();
	notnull(player->patron);
	require(patron_owes_reward(player));

	/* Sworn to nobody and carrying nothing: never. */
	player->patron = NULL;
	require(!patron_owes_reward(player));

	/* Carrying something that draws an eye: always. */
	of_on(player->state.flags, OF_PATRON);
	require(patron_owes_reward(player));
	of_off(player->state.flags, OF_PATRON);

	/* Merely unlucky: about one level in seven, so sample it. */
	of_on(player->state.flags, OF_STRANGE_LUCK);
	for (i = 0; i < 2000; i++) {
		if (patron_owes_reward(player)) noticed++;
	}
	of_off(player->state.flags, OF_STRANGE_LUCK);
	require(noticed > 100);
	require(noticed < 600);

	player->patron = born_to;

	ok;
}

/**
 * A Lord that was never yours is kinder.
 *
 * The other half of the same code, and the direction is a trap. Zangband writes
 * `nasty_chance *= 2` for a character without the patron flag
 * ([xtra2.c:3114](../archive/zangband/src/xtra2.c#L3114)), and `nasty_chance`
 * is the denominator of a one-in-N roll — so doubling it *halves* how often the
 * roll reaches the bottom quarter of the ladder, where everything genuinely
 * unpleasant lives. The first version of this test asserted the opposite,
 * because "a Lord with no reason to be kind" is the reading the fiction
 * invites and the code says otherwise.
 */
static int test_a_borrowed_lord_is_kinder(void *state) {
	const struct patron *born_to = player->patron;
	int i, sworn_low = 0, borrowed_low = 0;
	int floor_slot = PATRON_LADDER / 4;

	player->lev = 20;

	player->patron = patron_random();
	notnull(player->patron);
	for (i = 0; i < 40000; i++) {
		if (patron_roll_slot(player) < floor_slot) sworn_low++;
	}

	player->patron = NULL;
	for (i = 0; i < 40000; i++) {
		if (patron_roll_slot(player) < floor_slot) borrowed_low++;
	}

	player->patron = born_to;

	/*
	 * A margin, not merely "fewer", and forty thousand rolls rather than four.
	 *
	 * The doubling takes the nasty roll from one in six to one in twelve, so a
	 * sworn character should reach the bottom of the ladder about twice as
	 * often as a borrower, and asking only for "fewer" would make this a coin
	 * flip if the doubling were removed -- both sides would sample the same
	 * distribution.
	 *
	 * The margin is 1.5, and at four thousand rolls that was not enough. The
	 * bottom quarter of the ladder is rare, so four thousand rolls produced
	 * only 74 to 99 events on the borrowed side and the ratio ranged from
	 * **1.475 to 2.284** across ten seeds -- the bound sat about one and a half
	 * standard deviations from the mean and the test failed roughly one
	 * whole-suite run in fifteen. Found on seed 1604416127, which came in at
	 * 1.475.
	 *
	 * Ten times the rolls fixes it, and the figure is derived rather than
	 * tried. Forty thousand gives about 850 events on the borrowed side and
	 * 1700 on the sworn, so the relative error is sqrt(850)/850 = 3.4% and
	 * sqrt(1700)/1700 = 2.4%, and the ratio carries their quadrature sum:
	 * 2.0 x sqrt(0.034^2 + 0.024^2), about 0.083. The bound is then six
	 * standard deviations below the mean instead of one and a half. Measured
	 * across twelve seeds afterwards: 1.788 to 2.117.
	 *
	 * The floor on `borrowed_low` is the part of that reasoning the test can
	 * check for itself. If the rate ever changes enough that the counts
	 * collapse, the standard-deviation argument above stops holding and this
	 * says so rather than quietly becoming fragile again. 400 is fifteen
	 * standard deviations below the expected 850, so it cannot fire by chance.
	 */
	require(borrowed_low > 400);
	require(sworn_low * 2 > borrowed_low * 3);

	ok;
}

/**
 * A Ring of Wizardry is worth more the further you have come.
 *
 * The part of `SP` its name does not tell you: Zangband adds
 * `sp_bonus * levels` ([xtra1.c:1869](../archive/zangband/src/xtra1.c#L1869)),
 * so the pval is mana *per casting level* and not a flat bonus. A ring with a
 * pval of one is worth one point to a novice and thirty to a veteran, which is
 * the whole reason it is worth a finger.
 *
 * Read from the data rather than a live character, because the arithmetic that
 * would prove the scaling needs a caster of two different levels and the value
 * here is the one that would be wrong: a flat reading would still put a number
 * in the modifier and still look right.
 */
static int test_wizardry_grants_mana(void *state) {
	struct object_kind *ring = kind_named(TV_RING, "Wizardry");

	notnull(ring);
	require(ring->modifiers[OBJ_MOD_MANA].base > 0
			|| ring->modifiers[OBJ_MOD_MANA].m_bonus > 0);

	ok;
}

/**
 * The Rod of Havoc rolls on a table rather than doing one thing.
 *
 * `call_chaos()` picks one of thirty damage types and throws it
 * ([spells2.c:3522](../archive/zangband/src/spells2.c#L3522)). 4.2 can express
 * that in the data file alone: `effect:RANDOM` with a count chooses one of the
 * effects that follow it, which is how Angband's own Wafer of Rations picks a
 * nourishment.
 *
 * The count is asserted, not merely the presence of an effect, because the
 * failure worth catching is a table that lost most of its entries and still
 * works -- a rod that always throws lightning is a rod, just not this one.
 */
static int test_havoc_rolls_on_a_table(void *state) {
	struct object_kind *rod = kind_named(TV_ROD, "Havoc");
	struct effect *effect;
	int n = 0;

	notnull(rod);
	notnull(rod->effect);
	eq(rod->effect->index, EF_RANDOM);

	for (effect = rod->effect->next; effect; effect = effect->next) n++;

	/* Twenty-five elements as balls, eight of them again as beams. */
	eq(n, 33);

	ok;
}

/**
 * A Scroll of Mundanity has something to do.
 *
 * The effect is the whole of the object -- there is no flag, no value and no
 * damage on it -- so a conversion that dropped the effect line would leave a
 * scroll that reads and does nothing, and nothing else about the entry would
 * look wrong.
 */
static int test_mundanity_has_its_effect(void *state) {
	struct object_kind *scroll = kind_named(TV_SCROLL, "Mundanity");

	notnull(scroll);
	notnull(scroll->effect);
	eq(scroll->effect->index, EF_MUNDANE);

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
	{ "the-new-life-potion-arrived-with-mutations",
	  test_the_new_life_potion_arrived_with_mutations },
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
	{ "a-lord-notices-three-different-ways",
	  test_a_lord_notices_three_different_ways },
	{ "a-borrowed-lord-is-kinder", test_a_borrowed_lord_is_kinder },
	{ "wizardry-grants-mana", test_wizardry_grants_mana },
	{ "havoc-rolls-on-a-table", test_havoc_rolls_on_a_table },
	{ "mundanity-has-its-effect", test_mundanity_has_its_effect },
	{ NULL, NULL }
};
