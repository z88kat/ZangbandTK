/* game/roundtrip
 *
 * A character can be saved and loaded, and the spell-list fence works.
 *
 * On 2 September 2026 DEC-50 gave the Life realm Zangband's prayer books, which
 * deleted the object kinds of Angband's, and every one of the thirty-five
 * historical savefiles became unreadable in the same stroke -- every save
 * carries the town temple's stock and the temple sells prayer books. `game/saves`
 * now proves those files are refused *gracefully*, which is worth having, and
 * nothing proved a character could be saved and loaded at all.
 *
 * That matters most for the work that broke it. Three more realms have their
 * content replaced after this, each deleting its own book kinds, and the guard
 * that is supposed to catch a caster whose spell list moved underneath it --
 * the fingerprint in the `player spells` block -- had stopped being exercised
 * by anything. Every corpus file is now refused a step earlier, for a missing
 * object kind, so the fingerprint could have been broken outright and the whole
 * suite would still have passed.
 *
 * So this suite builds its own characters rather than reading a corpus, and
 * checks three things:
 *
 * 1. a caster with realms chosen and spells learned survives a round trip;
 * 2. the fingerprint **refuses** that character when the class's spell list has
 *    changed underneath it -- proved by changing it, not by trusting it;
 * 3. and **accepts** it when it has not, because a fence that always refuses
 *    passes any test that only checks it refuses something.
 *
 * The perturbation is a spell *rename*, on purpose. It leaves `total_spells`
 * alone, so the count check in `rd_player_spells()` cannot catch it and the
 * fingerprint is the only thing that can. Renaming is also exactly what DEC-50
 * does at scale: same book, same slot, different spell.
 */

#include "unit-test.h"
#include "test-utils.h"

#include "game-world.h"
#include "init.h"
#include "mon-make.h"
#include "player.h"
#include "generate.h"
#include "player-birth.h"
#include "player-calcs.h"
#include "player-spell.h"
#include "player-util.h"
#include "savefile.h"
#include "z-file.h"
#include "z-util.h"
#include "z-virt.h"

static char savename[1024];

static void println(const char *str) {
	printf("%s\n", str);
}

int setup_tests(void **state) {
	plog_aux = println;
	set_file_paths();
	if (!init_angband()) return 1;
#ifdef UNIX
	create_needed_dirs();
#endif
	test_savefile_name(savename, sizeof(savename), "Roundtrip");
	(void) test_seed_rng_reported(suite_name);
	return 0;
}

int teardown_tests(void *state) {
	file_delete(savename);
	if (cave) wipe_mon_list(cave, player);
	cleanup_angband();
	return 0;
}

/**
 * Put the game back where a load can start from.
 *
 * The same dance `game/basic` and the wilderness suite do: the loader builds a
 * game from nothing, so everything the last one left has to go first.
 */
static void reset_before_load(void) {
	play_again = true;
	if (cave) wipe_mon_list(cave, player);
	cleanup_angband();
	chunk_list_max = 0;
	init_angband();
	play_again = false;
}

/**
 * A full reset, paths and all, which a *new* character needs.
 *
 * `reset_before_load()` sets `play_again`, and `cleanup_angband()` returns
 * early on that flag before it reaches the directory strings -- which is the
 * only reason the existing suites survive resetting at all. This one goes all
 * the way and puts the paths back, which is what a fresh character after a
 * finished one requires.
 */
static void full_reset(void) {
	if (cave) wipe_mon_list(cave, player);
	cleanup_angband();
	chunk_list_max = 0;
	set_file_paths();
	init_angband();
}

/**
 * Somewhere a new character can be born, resetting only if one already exists.
 *
 * The reset is not free: `init_angband()` rebuilds the artifact upkeep table,
 * and running it twice with nothing in between leaves the artifact indices and
 * that table disagreeing, which `mark_artifact_created()` asserts on the first
 * artifact the new character's level tries to place. The state `setup_tests()`
 * leaves is already the state a birth wants, so do not disturb it.
 */
static void fresh_game(void) {
	if (character_generated || character_dungeon) full_reset();
}

/** A Priest on a real level, with two prayers learned. */
static bool make_a_caster(void) {
	fresh_game();
	if (!player_make_simple(NULL, "Priest", "Tester")) return false;

	prepare_next_level(player);
	on_new_level();

	/* Level enough to have the spell points the prayers cost. */
	player->lev = player->max_lev = 20;
	player->upkeep->update |= (PU_BONUS | PU_HP | PU_SPELLS);
	update_stuff(player);

	spell_learn(0);
	spell_learn(3);

	return true;
}

/** The class of that name, from the live list. */
static struct player_class *class_named(const char *name) {
	struct player_class *c;

	for (c = classes; c; c = c->next) {
		if (streq(c->name, name)) return c;
	}

	return NULL;
}

/** The spell at a flat index, or NULL. */
static struct class_spell *nth_spell(const struct player_class *c, int n) {
	int b, s, i = 0;

	if (!c) return NULL;

	for (b = 0; b < c->magic.num_books; b++) {
		for (s = 0; s < c->magic.books[b].num_spells; s++, i++) {
			if (i == n) return &c->magic.books[b].spells[s];
		}
	}

	return NULL;
}

/**
 * A caster with realms and spells comes back as itself.
 *
 * The fields checked are the ones DEC-50 puts at risk: the class, the realms
 * the character chose, and *which* spells were learned by flat index across the
 * class's books. The last is the one that goes wrong silently -- a character
 * that loads with the right number of spells and the wrong spells looks
 * perfectly healthy on the character sheet.
 */
static int test_a_caster_survives_a_round_trip(void *state) {
	char cls[64], realm0[64], first[64];
	int flags0, flags3, order0, order1, total, lev;

	require(make_a_caster());

	my_strcpy(cls, player->class->name, sizeof(cls));
	notnull(player->realm[0]);
	my_strcpy(realm0, player->realm[0]->name, sizeof(realm0));
	my_strcpy(first, nth_spell(player->class, 0)->name, sizeof(first));
	flags0 = player->spell_flags[0];
	flags3 = player->spell_flags[3];
	order0 = player->spell_order[0];
	order1 = player->spell_order[1];
	total = player->class->magic.total_spells;
	lev = player->lev;

	require(flags0 & PY_SPELL_LEARNED);
	require(flags3 & PY_SPELL_LEARNED);

	require(savefile_save(savename));
	require(file_exists(savename));

	reset_before_load();
	require(savefile_load(savename, false));

	notnull(player);
	notnull(player->class);
	require(streq(player->class->name, cls));
	eq(player->lev, lev);
	eq(player->class->magic.total_spells, total);

	notnull(player->realm[0]);
	require(streq(player->realm[0]->name, realm0));

	/* The same two spells, at the same two indices. */
	eq(player->spell_flags[0], flags0);
	eq(player->spell_flags[3], flags3);
	eq(player->spell_order[0], order0);
	eq(player->spell_order[1], order1);

	/* And index 0 still names the spell it named when it was learned. */
	require(streq(nth_spell(player->class, 0)->name, first));

	ok;
}

/**
 * Move the spell list underneath the character and the loader refuses.
 *
 * This is the assertion the corpus used to carry and cannot any more. A rename
 * leaves `total_spells` untouched, so `rd_player_spells()`'s count check passes
 * and only the fingerprint can object.
 *
 * Both directions in one test, because they are the same fence: refused when
 * the list moved, accepted when it did not. Checking only the first would pass
 * for a loader that refused everything, which is precisely how a savefile guard
 * fails safe into failing useless.
 */
static int test_the_fingerprint_refuses_a_moved_spell_list(void *state) {
	struct class_spell *spell;
	char kept[64];

	require(make_a_caster());
	require(savefile_save(savename));

	/* Unchanged: it loads. */
	reset_before_load();
	require(savefile_load(savename, false));
	notnull(player);
	require(streq(player->class->name, "Priest"));

	/*
	 * Now rename one prayer in the class data, which is what replacing a
	 * realm's content does to every slot at once, and load the same file.
	 */
	reset_before_load();
	spell = nth_spell(class_named("Priest"), 7);
	notnull(spell);
	my_strcpy(kept, spell->name, sizeof(kept));
	string_free(spell->name);
	spell->name = string_make("Somebody Else's Prayer");

	require(!savefile_load(savename, false));

	/*
	 * A refused load leaves the game half-built, which is fine for the game
	 * -- it shows the refusal and exits -- and not fine for a suite that has
	 * more to do. Put it back before handing on.
	 */
	reset_before_load();
	spell = nth_spell(class_named("Priest"), 7);
	notnull(spell);
	require(streq(spell->name, kept));

	ok;
}

/**
 * A character with no spells recorded is indifferent to the list moving.
 *
 * The guard only compares the fingerprint when the character actually has
 * spells, and that tolerance is not an oversight: a Warrior, or a caster who
 * has learned nothing, cannot be handed somebody else's spells because it holds
 * no indices into the list. It is also what let the Chaos-Warrior in the old
 * corpus keep loading when that class went from no books to a hundred and
 * twenty-eight.
 */
static int test_a_character_with_no_spells_does_not_care(void *state) {
	struct class_spell *spell;

	fresh_game();
	require(player_make_simple(NULL, "Warrior", "Tester"));
	prepare_next_level(player);
	on_new_level();
	eq(player->class->magic.total_spells, 0);

	require(savefile_save(savename));

	reset_before_load();

	/* Perturb a caster's list; the Warrior has no stake in it. */
	spell = nth_spell(class_named("Priest"), 7);
	notnull(spell);
	string_free(spell->name);
	spell->name = string_make("Somebody Else's Prayer");

	require(savefile_load(savename, false));
	notnull(player);
	require(streq(player->class->name, "Warrior"));

	ok;
}

const char *suite_name = "game/roundtrip";
struct test tests[] = {
	{ "a-caster-survives-a-round-trip",
	  test_a_caster_survives_a_round_trip },
	{ "the-fingerprint-refuses-a-moved-spell-list",
	  test_the_fingerprint_refuses_a_moved_spell_list },
	{ "a-character-with-no-spells-does-not-care",
	  test_a_character_with_no_spells_does_not_care },
	{ NULL, NULL }
};
