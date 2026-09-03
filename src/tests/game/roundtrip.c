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
 *
 * Since PLR-22 it also covers the monster record, which gained an allegiance
 * byte. That byte is the difference between reloading a level and reloading it
 * with your pets turned on you, and the version bump that goes with it is the
 * difference between old saves working and old saves reading one field's bytes
 * as another's.
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
#include "mon-util.h"
#include "obj-util.h"
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

/**
 * A pet is still a pet after a save and a load.
 *
 * Three monsters, one on each side, because the failure that matters is not
 * "allegiance was lost" -- a byte written and never read gives every monster
 * whatever happened to follow it, and one of the three would still look right.
 * Placed by hand and read back by race name.
 */
static int test_the_sides_survive_a_round_trip(void *state) {
	static const enum monster_allegiance sides[] = {
		MON_ALLEGIANCE_HOSTILE, MON_ALLEGIANCE_FRIENDLY, MON_ALLEGIANCE_PET
	};
	struct monster_group_info info = { 0, 0 };
	struct monster_race *race;
	int placed = 0, seen = 0, i, already = 0;

	/*
	 * After the birth, not before: `make_a_caster()` resets the game, and
	 * `cleanup_angband()` frees `r_info` on the way through, so a race
	 * pointer taken first is dangling by the time it is used. ASAN said so.
	 */
	require(make_a_caster());
	race = lookup_monster("soldier");
	notnull(race);

	/*
	 * The level may already hold soldiers of its own -- the generator picks
	 * by depth and this race is shallow -- so count them first and check the
	 * difference.  Without this the test passes or fails on what the level
	 * happened to contain, which is a coin toss with a seed on it.
	 */
	for (i = 1; i < cave_monster_max(cave); i++) {
		struct monster *mon = cave_monster(cave, i);

		if (mon->race == race) already++;
	}

	/*
	 * Three of the same race in a row, so only the byte distinguishes them.
	 * Up to fifty attempts, not three: `scatter_ext` looks for an empty grid
	 * within six and may not find one, and a loop that tries once per monster
	 * fails whenever the player lands somewhere crowded.  That was a one run
	 * in four flake before `scripts/check-flakes` found it.
	 */
	for (i = 0; i < 50 && placed < (int) N_ELEMENTS(sides); i++) {
		struct loc grid;

		if (scatter_ext(cave, &grid, 1, player->grid, 6, true,
						square_isempty) == 0) continue;
		if (!place_new_monster(cave, grid, race, false, false, info,
							   ORIGIN_DROP)) continue;
		monster_set_allegiance(square_monster(cave, grid), sides[placed]);
		placed++;
	}
	eq(placed, (int) N_ELEMENTS(sides));

	require(savefile_save(savename));
	reset_before_load();
	require(savefile_load(savename, false));

	/* Look it up again: the load reset the game and rebuilt r_info */
	race = lookup_monster("soldier");
	notnull(race);

	{
		int by_side[MON_ALLEGIANCE_MAX] = { 0 };

		for (i = 1; i < cave_monster_max(cave); i++) {
			struct monster *mon = cave_monster(cave, i);

			if (!mon->race || mon->race != race) continue;
			require(mon->allegiance >= 0
					&& mon->allegiance < MON_ALLEGIANCE_MAX);
			by_side[mon->allegiance]++;
			seen++;
		}
		eq(seen, already + 3);
		eq(by_side[MON_ALLEGIANCE_HOSTILE], already + 1);
		eq(by_side[MON_ALLEGIANCE_FRIENDLY], 1);
		eq(by_side[MON_ALLEGIANCE_PET], 1);
	}

	ok;
}

/**
 * The savefile says the monster record changed shape.
 *
 * Reads the block headers out of the file rather than trusting the table in
 * `savefile.c`, and asks for version 2 on both blocks that carry a monster
 * record -- "monsters" and "chunks", because `rd_chunks()` reads stored levels
 * through the same reader.
 *
 * This is the mistake worth a test: change the record, forget one of the two
 * version numbers, and an old savefile is read with the fields one byte out
 * from where they were written. That does not fail loudly. It produces a
 * monster with the wrong group index and a plausible allegiance.
 */
static int test_the_monster_blocks_say_version_two(void *state) {
	ang_file *f;
	uint8_t head[28];
	int found = 0;

	require(make_a_caster());
	require(savefile_save(savename));

	f = file_open(savename, MODE_READ, FTYPE_RAW);
	notnull(f);

	/* Eight bytes of magic and name come before the first block header */
	require(file_skip(f, 8));

	while (file_read(f, (char *) head, sizeof(head)) == (int) sizeof(head)) {
		char name[17];
		uint32_t version, size;

		memcpy(name, head, 16);
		name[16] = 0;
		version = head[16] | (head[17] << 8) | (head[18] << 16)
			| ((uint32_t) head[19] << 24);
		size = head[20] | (head[21] << 8) | (head[22] << 16)
			| ((uint32_t) head[23] << 24);

		if (streq(name, "monsters") || streq(name, "chunks")) {
			eq(version, 2);
			found++;
		}

		/* Blocks are padded to a multiple of four bytes */
		if (!file_skip(f, (int) ((size + 3) & ~3U))) break;
	}
	file_close(f);

	eq(found, 2);

	ok;
}

/**
 * Standing orders for pets survive a save (PLR-25, PLR-26).
 *
 * All three at once and all three away from their defaults, because the
 * failure mode of a positional savefile block is a *shift*: write two fields
 * and read three and every later field is wrong, while a test that only checks
 * one field cannot see it.
 *
 * The leash is set to a negative value on purpose. It is the one field whose
 * sign carries meaning, and reading a signed field as unsigned gives 65511
 * rather than -25 -- which is a legal-looking leash that no order can produce.
 */
static int test_the_pet_orders_survive_a_save(void *state) {
	require(make_a_caster());

	player->pet_follow_distance = PET_AWAY_DIST;
	player->pet_open_doors = true;
	player->pet_pickup_items = true;

	require(savefile_save(savename));
	reset_before_load();
	require(savefile_load(savename, false));

	eq(player->pet_follow_distance, PET_AWAY_DIST);
	require(player->pet_follow_distance < 0);
	require(player->pet_open_doors);
	require(player->pet_pickup_items);

	ok;
}

/**
 * The player block says version 6.
 *
 * Same reasoning as the monster block's: a shape change without a version bump
 * reads every older character's timed effects and energy out of the wrong
 * bytes, and does it quietly.
 */
static int test_the_player_block_says_version_six(void *state) {
	ang_file *f;
	uint8_t head[28];
	int found = 0;

	require(make_a_caster());
	require(savefile_save(savename));

	f = file_open(savename, MODE_READ, FTYPE_RAW);
	notnull(f);
	require(file_skip(f, 8));

	while (file_read(f, (char *) head, sizeof(head)) == (int) sizeof(head)) {
		char name[17];
		uint32_t version, size;

		memcpy(name, head, 16);
		name[16] = 0;
		version = head[16] | (head[17] << 8) | (head[18] << 16)
			| ((uint32_t) head[19] << 24);
		size = head[20] | (head[21] << 8) | (head[22] << 16)
			| ((uint32_t) head[23] << 24);

		if (streq(name, "player")) {
			eq(version, 6);
			found++;
		}

		if (!file_skip(f, (int) ((size + 3) & ~3U))) break;
	}
	file_close(f);

	eq(found, 1);

	ok;
}


/**
 * A savefile names what it holds, so renaming a thing is a compatibility event.
 *
 * These three exist because of a real one.  DEC-50 replaced Angband's five
 * prayer books with Zangband's four, which deleted the old object kinds -- and
 * a savefile records "prayer book", "[Novice's Handbook]" rather than a number.
 * Every save in the corpus carried the town temple's stock, so every save named
 * a book the game no longer had, and all thirty-five stopped loading.
 *
 * The corpus could not have caught that.  A test made of savefiles cannot
 * notice a change that stops them loading, because then there is nothing left
 * to test with.  So the break is made deliberately here, on a character the
 * suite has just written, against the live object table.
 */

/** The object kind of that name, from the live table. */
static struct object_kind *kind_named(const char *name) {
	int k;

	for (k = 0; k < z_info->k_max; k++) {
		if (k_info[k].name && streq(k_info[k].name, name)) return &k_info[k];
	}

	return NULL;
}

/** The kind of the first thing in the character's pack, or NULL. */
static struct object_kind *first_carried_kind(void) {
	struct object *obj;

	for (obj = player->gear; obj; obj = obj->next) {
		if (obj->kind && obj->kind->name) return obj->kind;
	}

	return NULL;
}

/** How many things of that name the character is carrying. */
static int carried_named(const char *name) {
	struct object *obj;
	int n = 0;

	for (obj = player->gear; obj; obj = obj->next) {
		if (obj->kind && obj->kind->name && streq(obj->kind->name, name)) n++;
	}

	return n;
}

/**
 * A thing renamed out from under a savefile is only lost, and the rest loads.
 *
 * The old behaviour was that it took the whole character with it -- and worse,
 * it took the *stream*: rd_item() returned NULL both for "the list ended" and
 * for "I cannot name this", the end-of-list marker being itself an item with no
 * kind.  A reader that met a vanished object stopped early and left the rest of
 * the list unread, so one deleted book desynchronised everything after it.
 */
static int test_a_vanished_object_is_only_lost(void *state) {
	struct object_kind *kind;
	char was[128];
	int carried;

	require(make_a_caster());

	kind = first_carried_kind();
	notnull(kind);
	my_strcpy(was, kind->name, sizeof(was));
	carried = carried_named(was);
	require(carried > 0);

	require(savefile_save(savename));

	reset_before_load();

	/*
	 * Renamed after the reset, not before it.  reset_before_load() runs
	 * init_angband(), which rebuilds the object table out of the data files --
	 * so a kind renamed before the reset is quietly put back, the load sees the
	 * name it saved, and the test passes having broken nothing.  It has to be
	 * renamed in the table the *load* will read.
	 *
	 * No rename.txt entry, so there is nothing to recover it with.
	 */
	kind = kind_named(was);
	notnull(kind);
	string_free(kind->name);
	kind->name = string_make("A Thing That Is Not There Any More");

	require(savefile_load(savename, false));

	/* The character came back... */
	notnull(player->class);
	notnull(player->race);

	/* ...without the thing, and without it taking anything else. */
	eq(carried_named(was), 0);

	ok;
}

/**
 * And with an entry in rename.txt it is not even lost.
 *
 * This is the cheap half of the answer: a rename costs one line in a data file,
 * and that line is the difference between a character keeping its sword and
 * not.
 */
static int test_a_renamed_object_is_recovered(void *state) {
	struct object_kind *kind;
	struct rename_entry *entry;
	char was[128];
	int carried, tval;

	require(make_a_caster());

	kind = first_carried_kind();
	notnull(kind);
	my_strcpy(was, kind->name, sizeof(was));
	tval = kind->tval;
	carried = carried_named(was);
	require(carried > 0);

	require(savefile_save(savename));

	reset_before_load();

	/*
	 * Both the rename and the entry go in after the reset: init_angband()
	 * rebuilds the object table *and* re-reads rename.txt, so anything done
	 * before it is discarded.
	 */
	kind = kind_named(was);
	notnull(kind);
	string_free(kind->name);
	kind->name = string_make("Renamed For The Test");

	/* What a rename.txt line amounts to, pushed onto the live list. */
	entry = mem_zalloc(sizeof *entry);
	entry->kind = RENAME_OBJECT;
	entry->tval = tval;
	entry->from = string_make(was);
	entry->to = string_make("Renamed For The Test");
	entry->next = renames;
	renames = entry;

	require(savefile_load(savename, false));

	notnull(player->class);

	/* Back, under its new name and in the same number. */
	eq(carried_named("Renamed For The Test"), carried);

	ok;
}

/**
 * Every rename in the data file points at something that exists.
 *
 * A typo here is silent in the worst way: the entry looks like protection and
 * does nothing, so the object it was meant to save is lost anyway and the file
 * says otherwise.  Removals are written `-` and name nothing on purpose.
 */
static int test_every_rename_points_somewhere_real(void *state) {
	const struct rename_entry *r;
	int checked = 0, removals = 0;

	for (r = renames; r; r = r->next) {
		notnull(r->from);
		require(r->from[0] != '\0');

		if (!r->to) {
			removals++;
			continue;
		}

		switch (r->kind) {
			case RENAME_OBJECT:
				require(r->tval >= 0);
				notnull(kind_named(r->to));
				break;
			case RENAME_MONSTER:
				notnull(lookup_monster(r->to));
				break;
			case RENAME_ARTIFACT:
				notnull(lookup_artifact_name(r->to));
				break;
			case RENAME_EGO:
				break;
		}
		checked++;
	}

	printf("RENAMES %d point somewhere real, %d record a removal\n", checked,
		   removals);

	/* The file is not empty, or this test checks nothing. */
	require(checked + removals > 0);

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
	{ "the-monster-blocks-say-version-two",
	  test_the_monster_blocks_say_version_two },
	{ "the-sides-survive-a-round-trip",
	  test_the_sides_survive_a_round_trip },
	{ "the-player-block-says-version-six",
	  test_the_player_block_says_version_six },
	{ "the-pet-orders-survive-a-save",
	  test_the_pet_orders_survive_a_save },
	{ "a-vanished-object-is-only-lost", test_a_vanished_object_is_only_lost },
	{ "a-renamed-object-is-recovered", test_a_renamed_object_is_recovered },
	{ "every-rename-points-somewhere-real",
	  test_every_rename_points_somewhere_real },
	{ NULL, NULL }
};
